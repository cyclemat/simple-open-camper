// Satellit: Einrichten per Knopf – siehe sat_pair.h und ../sat_pair_proto.h
#include "sat_pair.h"

#if SAT_PAIR
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include "sat_config.h"
#include "sat_send.h"
#include "../devsetup.h"
#include "../truma.h"
#include "../sat_pair_proto.h"
#include "../sat_pair_crypto.h"

#ifndef SAT_PAIR_BTN
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define SAT_PAIR_BTN 9   // BOOT-Taste der C3-/C6-Boards
#else
#define SAT_PAIR_BTN 0   // BOOT-Taste (GPIO0 ist nur beim Einschalten besonders, danach ein normaler Eingang)
#endif
#endif

static const uint32_t HOLD_MS = 3000;          // so lange BOOT halten
static const uint32_t WIFI_TEST_MS = 30000;    // so lange auf das WLAN warten (Bluetooth teilt sich die Antenne)

static bool s_active = false;
static uint32_t s_until = 0, s_btn_ms = 0, s_test_ms = 0, s_restart_ms = 0;
static NimBLEServer *s_srv = nullptr;
static NimBLECharacteristic *s_cred = nullptr, *s_stat = nullptr, *s_pub_ch = nullptr;
static uint8_t s_priv[32], s_pub[65];   // Schluesselpaar dieses Einrichtungsvorgangs (neu bei jedem Start)
static volatile bool s_got = false;     // Daten vom Panel angekommen (Bluetooth-Task -> loop)
static std::string s_rx;                // nur bis zur Uebernahme in loop()
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int s_wifi_reason = 0;   // letzter Trenngrund beim WLAN-Test (0 = keiner)
static String s_test_ssid;               // Netz, das gerade getestet wird (fuer die Meldung)
static bool s_evt = false;

static void set_stat(const char *st, const String &ip = "", const char *msg = "") {
  if (!s_stat) return;
  JsonDocument d;
  d["st"] = st;
  if (ip.length()) d["ip"] = ip;
  if (*msg) d["msg"] = msg;
  std::string s;
  serializeJson(d, s);
  s_stat->setValue(s);
  s_stat->notify();
  log_i("Einrichtung: %s %s %s", st, ip.c_str(), msg);
}

namespace {
class CredCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    portENTER_CRITICAL(&s_mux);
    s_rx.assign((const char *)v.data(), v.length());
    s_got = true;
    portEXIT_CRITICAL(&s_mux);
  }
};
CredCB s_cred_cb;
// Fehlersuche: mit welcher Adresse meldet sich das Panel? (P4: Funk ueber ESP32-C6/ESP-Hosted)
class SrvCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &ci) override {
    log_w("Einrichtung: Panel verbunden, Adresse %s (Typ %d)", ci.getAddress().toString().c_str(), ci.getAddress().getType());
  }
};
SrvCB s_srv_cb;
}  // namespace

static void pair_start() {
  if (s_active) {
    s_until = millis() + SAT_PAIR_SECONDS * 1000UL;
    return;
  }
  truma_ble_init();
  sat_send_adv_suspend(true);   // die normalen Werbepakete (Werte) pausieren
  // Kein Bluetooth-Pairing: die Daten verschluesselt die Software selbst (sat_pair_crypto.h, ECDH P-256 + AES)
  if (!satpair::make_key(s_priv, s_pub)) {
    log_e("Einrichtung: Schluessel konnte nicht erzeugt werden");
    sat_send_adv_suspend(false);
    return;
  }
  if (!s_srv) {
    s_srv = NimBLEDevice::createServer();
    s_srv->setCallbacks(&s_srv_cb, false);
    NimBLEService *svc = s_srv->createService(SAT_PAIR_SVC);
    // Die Daten kommen schon verschluesselt (eigener Schluesseltausch), das WLAN-Passwort ist nie im Klartext in der Luft
    s_cred = svc->createCharacteristic(SAT_PAIR_CRED, NIMBLE_PROPERTY::WRITE);
    s_cred->setCallbacks(&s_cred_cb);
    s_stat = svc->createCharacteristic(SAT_PAIR_STAT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_pub_ch = svc->createCharacteristic(SAT_PAIR_PUB, NIMBLE_PROPERTY::READ);
    svc->start();
    s_srv->advertiseOnDisconnect(true);
  }
  s_pub_ch->setValue(s_pub, sizeof(s_pub));
  set_stat("warte");
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->stop();
  NimBLEAdvertisementData d;
  d.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  d.setCompleteServices(NimBLEUUID(SAT_PAIR_SVC));
  adv->setAdvertisementData(d);
  NimBLEAdvertisementData sr;
  sr.setName(sat_ble_name_now().c_str());
  adv->setScanResponseData(sr);
  adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  adv->setMinInterval(160);
  adv->setMaxInterval(240);
  adv->start();
  s_active = true;
  s_until = millis() + SAT_PAIR_SECONDS * 1000UL;
  s_test_ms = 0;
  log_w("Einrichtungsmodus: %u s lang per Bluetooth sichtbar (%s)", SAT_PAIR_SECONDS, sat_ble_name_now().c_str());
}

static void pair_stop() {
  if (!s_active) return;
  s_active = false;
  NimBLEDevice::getAdvertising()->stop();
  if (s_srv) {
    s_srv->advertiseOnDisconnect(false);
    for (auto h : s_srv->getPeerDevices()) s_srv->disconnect(h);
  }
  sat_send_adv_suspend(false);
  memset(s_priv, 0, sizeof(s_priv));
  log_i("Einrichtungsmodus beendet");
}

bool sat_pair_active() { return s_active; }

void sat_pair_begin() {
#if SAT_PAIR_BTN >= 0
  pinMode(SAT_PAIR_BTN, INPUT_PULLUP);
#endif
  if (devsetup::force_ap()) pair_start();   // 3x schnell neu gestartet: auch ohne Taste einrichten
}

// Daten vom Panel uebernehmen: speichern, WLAN testen
static void apply(const std::string &raw) {
  // Panel-Schluessel (65) | IV | Geheimtext | Pruefwert -> entschluesseln
  std::string plain;
  uint8_t z[32], enc[32], mac[32];
  const uint8_t *r = (const uint8_t *)raw.data();
  bool ok = raw.size() > 65 + 32 && satpair::ecdh(s_priv, r, z);
  if (ok) {
    satpair::derive(z, s_pub, r, enc, mac);
    ok = satpair::open(enc, mac, r + 65, raw.size() - 65, plain);
  }
  memset(z, 0, sizeof(z));
  memset(enc, 0, sizeof(enc));
  if (!ok) {
    set_stat("fehler", "", "Entschluesseln fehlgeschlagen");
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, plain) || !d["k"].is<const char *>()) {
    set_stat("fehler", "", "Daten unvollstaendig");
    return;
  }
  String ssid = d["s"] | "", pass = d["p"] | "", key = d["k"] | "";
  if (key.length()) g_sat.ble_pw = key;
  String mh = d["mh"] | "";
  if (mh.length()) {   // MQTT-Broker des Panels uebernehmen (zusaetzlich zu UDP)
    g_sat.mqtt_host = mh;
    g_sat.mqtt_port = d["mp"] | 1883;
    g_sat.mqtt_user = d["mu"] | "";
    g_sat.mqtt_pass = d["mw"] | "";
    g_sat.modes |= SAT_M_MQTT;
    log_w("Einrichtung: MQTT-Broker %s:%u uebernommen", mh.c_str(), (unsigned)g_sat.mqtt_port);
  }
  if (ssid.length()) {
    g_sat.wifi_ssid = ssid;
    g_sat.wifi_pass = pass;
    g_sat.modes |= SAT_M_UDP;   // im WLAN schneller und mit Reichweite
  }
  sat_config_save();
  if (!ssid.length()) {   // nur das Passwort (Panel ohne WLAN – sollte das Panel gar nicht schicken)
    set_stat("ok");
    s_restart_ms = millis() + 3000;
    return;
  }
  set_stat("verbinde");
  if (!s_evt) {   // Trenngrund merken: zeigt, ob das Netz fehlt oder das Passwort abgelehnt wird
    s_evt = true;
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) { s_wifi_reason = info.wifi_sta_disconnected.reason; },
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  }
  s_wifi_reason = 0;
  s_test_ssid = ssid;
  log_w("Einrichtung: teste WLAN \"%s\" (Passwort %u Zeichen)", ssid.c_str(), (unsigned)pass.length());
  WiFi.disconnect(false, false);
  WiFi.mode((WiFi.getMode() & WIFI_AP) ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  s_test_ms = millis();
}

void sat_pair_loop() {
  uint32_t now = millis();
#if SAT_PAIR_BTN >= 0
  if (digitalRead(SAT_PAIR_BTN) == LOW) {
    if (!s_btn_ms) s_btn_ms = now | 1;
    else if (s_btn_ms != 1 && now - s_btn_ms >= HOLD_MS) {
      s_btn_ms = 1;   // je Druck nur einmal
      pair_start();
    }
  } else {
    s_btn_ms = 0;
  }
#endif
  if (s_restart_ms && (int32_t)(now - s_restart_ms) >= 0) {
    log_w("Einrichtung abgeschlossen – Neustart");
    delay(100);
    ESP.restart();
  }
  if (!s_active) return;
  if (s_got) {
    std::string raw;
    portENTER_CRITICAL(&s_mux);
    raw.swap(s_rx);
    s_got = false;
    portEXIT_CRITICAL(&s_mux);
    apply(raw);
  }
  if (s_test_ms) {
    if (WiFi.isConnected()) {
      s_test_ms = 0;
      set_stat("ok", WiFi.localIP().toString());
      s_restart_ms = now + 4000;   // Zeit, damit das Panel die IP liest
    } else if (now - s_test_ms > WIFI_TEST_MS) {
      s_test_ms = 0;
      int r = s_wifi_reason;
      char msg[96];
      if (r == WIFI_REASON_NO_AP_FOUND)
        snprintf(msg, sizeof(msg), "WLAN \"%s\" nicht gefunden (Reichweite? nur 2,4 GHz)", s_test_ssid.c_str());
      else if (r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_HANDSHAKE_TIMEOUT ||
               r == WIFI_REASON_AUTH_EXPIRE || r == WIFI_REASON_MIC_FAILURE)
        snprintf(msg, sizeof(msg), "Passwort fuer \"%s\" abgelehnt (Grund %d)", s_test_ssid.c_str(), r);
      else if (r)
        snprintf(msg, sizeof(msg), "WLAN \"%s\": keine Verbindung (Grund %d)", s_test_ssid.c_str(), r);
      else
        snprintf(msg, sizeof(msg), "WLAN \"%s\": keine Antwort in %u s", s_test_ssid.c_str(), (unsigned)(WIFI_TEST_MS / 1000));
      log_w("Einrichtung: %s", msg);
      set_stat("fehler", "", msg);
    }
  }
  if (!s_restart_ms && (int32_t)(now - s_until) >= 0) pair_stop();
}

#else
void sat_pair_begin() {}
void sat_pair_loop() {}
bool sat_pair_active() { return false; }
#endif
