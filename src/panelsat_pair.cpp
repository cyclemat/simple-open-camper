// Panel: Satelliten per Knopf einrichten – siehe panelsat_pair.h und sat_pair_proto.h
#include "panelsat_pair.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sat_pair_proto.h"
#include "sat_pair_crypto.h"
#include "sat_proto.h"
#include "panelsat.h"
#include "settings.h"
#include "i18n/i18n.h"
#include "z2m.h"
#include "net.h"

namespace {
struct Cand {
  std::string mac;
  uint8_t type = 0;
  int rssi = 0;
  uint32_t seen = 0;
};
SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() {
    if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
    xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
  }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

static std::vector<Cand> s_cands;
static std::string s_target, s_payload, s_status, s_ip;
static uint8_t s_target_type = 0;
static volatile bool s_want = false;
static volatile int s_state = 0;
static volatile uint32_t s_ver = 1;
static const uint32_t CAND_TTL_MS = 15000;   // so lange ohne Werbepaket gilt ein Kandidat als weg
static volatile uint32_t s_search_until = 0; // Suche laeuft bis (0 = aus); nur dann scannt das Panel dafuer

bool satpair_searching() { return s_search_until && (int32_t)(s_search_until - millis()) > 0; }
uint32_t satpair_search_left() { return satpair_searching() ? (s_search_until - millis()) / 1000 + 1 : 0; }
void satpair_search_start() {
  Lock l;
  s_search_until = (millis() + SAT_PAIR_SECONDS * 1000UL) | 1;
  if (s_state != 1) {
    s_state = 0;
    s_status.clear();
  }
  s_ver++;
}
void satpair_search_stop() {
  Lock l;
  s_search_until = 0;
  s_cands.clear();
  s_ver++;
}

static void set_state(int st, const std::string &txt) {
  Lock l;
  s_state = st;
  s_status = txt;
  s_ver++;
}

bool satpair_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!satpair_searching() || !dev->isAdvertisingService(NimBLEUUID(SAT_PAIR_SVC))) return false;
  std::string mac = dev->getAddress().toString();
  for (auto &c : mac) c = tolower((unsigned char)c);
  Lock l;
  uint32_t now = millis();
  for (auto &c : s_cands)
    if (c.mac == mac) {
      c.rssi = dev->getRSSI();
      c.seen = now;
      return true;
    }
  Cand c;
  c.mac = mac;
  c.type = dev->getAddress().getType();
  c.rssi = dev->getRSSI();
  c.seen = now;
  s_cands.push_back(c);
  s_ver++;
  return true;
}

std::vector<SatPairCand> satpair_candidates() {
  Lock l;
  uint32_t now = millis();
  if (!satpair_searching() && !s_cands.empty()) s_cands.clear();   // Suche vorbei
  size_t before = s_cands.size();
  s_cands.erase(std::remove_if(s_cands.begin(), s_cands.end(), [&](const Cand &c) { return now - c.seen > CAND_TTL_MS; }),
                s_cands.end());
  if (s_cands.size() != before) s_ver++;
  std::vector<SatPairCand> out;
  for (auto &c : s_cands) {
    SatPairCand o;
    o.mac = c.mac;
    o.name = sat_ble_name(c.mac);
    o.rssi = c.rssi;
    out.push_back(o);
  }
  return out;
}

// WLAN-Daten je Betriebsart: Camp-WLAN -> dieses Netz, Router/Kombi -> Panel-WLAN, WLAN aus -> keine Einrichtung
std::string satpair_start(const std::string &mac) {
  Settings st = settings_copy();
  std::string ssid, pass;
  switch (st.net_mode) {
    case NET_OFF: return tr("WLAN ist am Panel aus – Satelliten lassen sich nur mit WLAN einrichten.");
    case NET_STA:
      if (st.wifi_ssid.isEmpty()) return tr("Am Panel ist noch kein Camp-WLAN eingerichtet.");
      ssid = st.wifi_ssid.c_str();
      pass = st.wifi_pass.c_str();
      break;
    default:   // Router und Kombi: das Panel-WLAN bleibt gleich, auch wenn sich das Camp-WLAN aendert
      ssid = st.ap_ssid.c_str();
      pass = st.ap_pass.c_str();
      break;
  }
  JsonDocument d;
  d["s"] = ssid;
  d["p"] = pass;
  d["k"] = panelsat_password();
  d["pi"] = net_ip().c_str();   // IP des Panels: Zusatzdisplays melden sich direkt dort
  // MQTT-Broker des Panels mitgeben (Satellit sendet dann auch dorthin, UDP bleibt): nur wenn das Panel einen nutzt
  std::string mh = z2m_broker_host();
  if (!mh.empty()) {
    bool own = false;
#if WITH_BROKER
    own = st.broker_on;   // eingebauter Broker: dessen Zugang
#endif
    d["mh"] = mh;
    d["mp"] = own ? 1883 : st.mqtt_port;
    const String &u = own ? st.broker_user : st.mqtt_user, &w = own ? st.broker_pass : st.mqtt_pass;
    if (!u.isEmpty()) d["mu"] = u.c_str();
    if (!w.isEmpty()) d["mw"] = w.c_str();
  }
  std::string payload;
  serializeJson(d, payload);
  Lock l;
  if (s_want || s_state == 1) return tr("Es wird gerade schon ein Satellit eingerichtet.");
  auto it = std::find_if(s_cands.begin(), s_cands.end(), [&](const Cand &c) { return c.mac == mac; });
  if (it == s_cands.end()) return tr("Der Satellit ist nicht mehr im Einrichtungsmodus – am Satelliten erneut BOOT 3 s halten.");
  s_search_until = 0;   // gefunden: Suche beenden
  s_target = mac;
  s_target_type = it->type;
  s_payload = payload;
  s_ip.clear();
  s_state = 1;
  s_status = tr("Verbinde mit dem Satelliten …");
  s_want = true;
  s_ver++;
  return "";
}

bool satpair_want_connect() { return s_want; }

// Laeuft im Bluetooth-Task von truma.cpp, waehrend der Scan pausiert
void satpair_connect_now() {
  std::string mac, payload;
  uint8_t type;
  {
    Lock l;
    s_want = false;
    mac = s_target;
    payload = s_payload;
    type = s_target_type;
  }
  NimBLEClient *c = NimBLEDevice::createClient();
  c->setConnectTimeout(10000);
  auto done = [&](int st, const std::string &txt) {
    if (c->isConnected()) c->disconnect();
    NimBLEDevice::deleteClient(c);
    {
      Lock l;
      s_payload.assign(s_payload.size(), '\0');   // WLAN-Passwort nicht im Speicher liegen lassen
      s_payload.clear();
    }
    set_state(st, txt);
    log_w("Satellit einrichten %s: %s", mac.c_str(), txt.c_str());
  };
  // Fehlersuche (P4: Funk ueber ESP32-C6): welche eigene Adresse nimmt die Bluetooth-Software an?
  log_w("Satellit einrichten: eigene Bluetooth-Adresse laut Host %s", NimBLEDevice::getAddress().toString().c_str());
  if (!c->connect(NimBLEAddress(mac, type), true)) return done(3, tr("Verbindung zum Satelliten fehlgeschlagen."));
  // Kein Bluetooth-Pairing (scheitert am P4): eigener Schluesseltausch, die Daten gehen schon verschluesselt raus
  NimBLERemoteService *svc = c->getService(NimBLEUUID(SAT_PAIR_SVC));
  NimBLERemoteCharacteristic *cred = svc ? svc->getCharacteristic(NimBLEUUID(SAT_PAIR_CRED)) : nullptr;
  NimBLERemoteCharacteristic *stat = svc ? svc->getCharacteristic(NimBLEUUID(SAT_PAIR_STAT)) : nullptr;
  NimBLERemoteCharacteristic *pubc = svc ? svc->getCharacteristic(NimBLEUUID(SAT_PAIR_PUB)) : nullptr;
  if (!cred || !stat || !pubc) return done(3, tr("Der Satellit hat eine ältere Firmware – bitte zuerst aktualisieren."));
  std::string sat_pub = pubc->readValue();
  if (sat_pub.size() != 65 || (uint8_t)sat_pub[0] != 0x04) return done(3, tr("Schlüssel des Satelliten ungültig."));
  uint8_t priv[32], pub[65], z[32], enc[32], mk[32];
  bool kok = satpair::make_key(priv, pub) && satpair::ecdh(priv, (const uint8_t *)sat_pub.data(), z);
  if (kok) satpair::derive(z, (const uint8_t *)sat_pub.data(), pub, enc, mk);
  memset(priv, 0, sizeof(priv));
  memset(z, 0, sizeof(z));
  if (!kok) return done(3, tr("Schlüsseltausch fehlgeschlagen."));
  std::string ss;   // Netzname fuer die Anzeige (vor dem Loeschen der Daten auslesen)
  {
    JsonDocument pd;
    if (!deserializeJson(pd, payload)) ss = pd["s"] | "";
  }
  std::string blob((const char *)pub, 65);
  blob += satpair::seal(enc, mk, payload);
  memset(enc, 0, sizeof(enc));
  std::fill(payload.begin(), payload.end(), '\0');
  set_state(1, std::string(tr("Übertrage WLAN-Daten (verschlüsselt) für ")) + "\"" + ss + "\" …");
  log_w("Satellit einrichten: sende WLAN \"%s\"", ss.c_str());
  if (!cred->writeValue((const uint8_t *)blob.data(), blob.size(), true)) return done(3, tr("Übertragen fehlgeschlagen."));
  // Ergebnis abfragen: der Satellit testet das WLAN (bis ~30 s) und meldet seine IP. Selten fragen: Bluetooth und
  // WLAN teilen sich beim Satelliten die Antenne, haeufige Abfragen bremsen seinen WLAN-Aufbau.
  uint32_t t0 = millis();
  while (millis() - t0 < 45000 && c->isConnected()) {
    std::string v = stat->readValue();
    JsonDocument d;
    if (!deserializeJson(d, v)) {
      std::string st = d["st"] | "";
      if (st == "verbinde") set_state(1, tr("Satellit verbindet sich mit dem WLAN …"));
      if (st == "ok") {
        std::string ip = d["ip"] | "";
        {
          Lock l;
          s_ip = ip;
        }
        return done(2, ip.empty() ? std::string(tr("Eingerichtet.")) : std::string(tr("Eingerichtet – IP ")) + ip);
      }
      if (st == "fehler") {
        std::string msg = d["msg"] | "";
        return done(3, std::string(tr("Satellit meldet: ")) + (msg.empty() ? "Fehler" : msg));
      }
    }
    delay(1500);
  }
  done(3, tr("Keine Rückmeldung vom Satelliten (WLAN erreichbar?)."));
}

int satpair_state() { return s_state; }
std::string satpair_status() {
  Lock l;
  return s_status;
}
std::string satpair_ip() {
  Lock l;
  return s_ip;
}
uint32_t satpair_version() { return s_ver; }
