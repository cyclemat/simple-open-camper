// Shelly Gen2+ per Bluetooth (BLE-RPC, Beta) – siehe shelly_ble.h
// Protokoll nach der oeffentlichen Shelly-API-Doku ("BLE RPC", "Authentication"): Dienst mit drei Kanaelen –
// tx_ctl (Laenge der Anfrage, 4 Byte big-endian), data (JSON-Rahmen in Stuecken), rx_ctl (Laenge der Antwort).
#include "shelly_ble.h"
#if WITH_SHELLY_BLE
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <math.h>
#include <algorithm>
#include "app_config.h"
#include "esp_random.h"
#include "mbedtls/sha256.h"
#include "i18n/i18n.h"

namespace {

const char *SVC = "5f6d4f53-5f52-5043-5f53-56435f49445f";    // "_mOS_RPC_SVC_ID_"
const char *DATA = "5f6d4f53-5f52-5043-5f64-6174615f5f5f";   // "_mOS_RPC_data___"
const char *TXC = "5f6d4f53-5f52-5043-5f74-785f63746c5f";    // "_mOS_RPC_tx_ctl_"
const char *RXC = "5f6d4f53-5f52-5043-5f72-785f63746c5f";    // "_mOS_RPC_rx_ctl_"
const uint32_t POLL_MS = 30000;
const int32_t OFFLINE_S = 180;
const uint16_t SHELLY_CID = 0x0BA9;   // Hersteller-ID (Allterco/Shelly)

struct Cmd {
  std::string comp;   // "switch:0"
  int8_t on = -1;     // -1 = umschalten (Toggle), 0/1 = fest
  int bri = -1;       // Licht: Helligkeit 0..100, -1 = unveraendert
};
struct Dev {
  std::string mac, name, pw, model, status;
  uint8_t type = 0;
  bool auth = false, info_done = false;
  std::vector<ShbChannel> ch;
  std::vector<Cmd> cmds;
  uint32_t last_ok = 0, next_poll = 0;
  int rssi = 0;
  uint8_t fails = 0;
};

SemaphoreHandle_t s_mx = nullptr;
std::vector<Dev> s_devs;
std::vector<ShbFound> s_found;
bool s_on = false;
uint32_t s_search_until = 0;
volatile uint32_t s_ver = 1;
uint32_t s_rpc_id = 1;

struct Lock {
  Lock() { xSemaphoreTake(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_mx); }
};

std::string lower(std::string s) {
  for (auto &c : s) c = (char)tolower((unsigned char)c);
  return s;
}

Dev *find(const std::string &mac) {
  for (auto &d : s_devs)
    if (d.mac == mac) return &d;
  return nullptr;
}

void save() {   // unter Lock
  Preferences p;
  if (!p.begin("shellyble", false)) return;
  p.putBool("on", s_on);
  p.putUChar("n", (uint8_t)s_devs.size());
  for (size_t i = 0; i < s_devs.size(); i++) {
    char k[6];
    snprintf(k, sizeof(k), "m%u", (unsigned)i);
    p.putString(k, s_devs[i].mac.c_str());
    k[0] = 't';
    p.putUChar(k, s_devs[i].type);
    k[0] = 'n';
    p.putString(k, s_devs[i].name.c_str());
    k[0] = 'p';
    p.putString(k, s_devs[i].pw.c_str());
  }
  p.end();
}

void load() {
  Preferences p;
  if (!p.begin("shellyble", true)) return;
  s_on = p.getBool("on", false);
  uint8_t n = p.getUChar("n", 0);
  for (uint8_t i = 0; i < n && i < SHB_MAX; i++) {
    char k[6];
    Dev d;
    snprintf(k, sizeof(k), "m%u", (unsigned)i);
    d.mac = p.getString(k, "").c_str();
    k[0] = 't';
    d.type = p.getUChar(k, 0);
    k[0] = 'n';
    d.name = p.getString(k, "").c_str();
    k[0] = 'p';
    d.pw = p.getString(k, "").c_str();
    d.next_poll = millis() + 3000 + i * 2000;
    if (!d.mac.empty()) s_devs.push_back(d);
  }
  p.end();
}

std::string sha256hex(const std::string &s) {
  uint8_t h[32];
  mbedtls_sha256((const unsigned char *)s.data(), s.size(), h, 0);
  char out[65];
  for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", h[i]);
  return out;
}

// Eine JSON-RPC-Anfrage ueber die drei Kanaele; res = vollstaendige Antwort. Passwort: Digest SHA-256 (Benutzer "admin").
bool rpc(NimBLEClient *c, const std::string &method, const std::string &params, const std::string &pw, JsonDocument &res,
         std::string &err, bool &auth_needed) {
  NimBLERemoteService *svc = c->getService(NimBLEUUID(SVC));
  if (!svc) {
    err = tr("kein Bluetooth-RPC (Gen1 oder in der Shelly-App ausgeschaltet)");
    return false;
  }
  NimBLERemoteCharacteristic *data = svc->getCharacteristic(NimBLEUUID(DATA)), *tx = svc->getCharacteristic(NimBLEUUID(TXC)),
                             *rx = svc->getCharacteristic(NimBLEUUID(RXC));
  if (!data || !tx || !rx) {
    err = tr("RPC-Kanäle fehlen");
    return false;
  }
  std::string auth;
  for (int attempt = 0; attempt < 2; attempt++) {
    std::string frame = "{\"id\":" + std::to_string(s_rpc_id++) + ",\"src\":\"soc-panel\",\"method\":\"" + method + "\"";
    if (!params.empty()) frame += ",\"params\":" + params;
    if (!auth.empty()) frame += ",\"auth\":" + auth;
    frame += "}";
    uint8_t len[4] = {(uint8_t)(frame.size() >> 24), (uint8_t)(frame.size() >> 16), (uint8_t)(frame.size() >> 8),
                      (uint8_t)frame.size()};
    if (!tx->writeValue(len, 4, true)) {
      err = tr("Senden fehlgeschlagen");
      return false;
    }
    size_t chunk = c->getMTU() > 23 ? c->getMTU() - 3 : 20;
    for (size_t o = 0; o < frame.size(); o += chunk) {
      if (!data->writeValue((const uint8_t *)frame.data() + o, std::min(chunk, frame.size() - o), true)) {
        err = tr("Senden fehlgeschlagen");
        return false;
      }
    }
    // Antwortlaenge abwarten (bis 6 s)
    uint32_t want = 0, t0 = millis();
    while (!want && millis() - t0 < 6000) {
      std::string v = rx->readValue();
      if (v.size() >= 4)
        want = (uint32_t)(uint8_t)v[0] << 24 | (uint32_t)(uint8_t)v[1] << 16 | (uint32_t)(uint8_t)v[2] << 8 | (uint8_t)v[3];
      if (!want) delay(100);
    }
    if (!want || want > 16384) {
      err = want ? tr("Antwort zu groß") : tr("keine Antwort");
      return false;
    }
    std::string buf;
    while (buf.size() < want) {
      std::string part = data->readValue();
      if (part.empty()) break;
      buf += part;
    }
    res.clear();
    if (deserializeJson(res, buf)) {
      err = tr("Antwort unlesbar");
      return false;
    }
    if (!res["error"].is<JsonObject>()) return true;
    int code = res["error"]["code"] | 0;
    if (code != 401) {
      err = std::string(tr("Fehler ")) + std::to_string(code) + ": " + (const char *)(res["error"]["message"] | "");
      return false;
    }
    auth_needed = true;
    if (pw.empty() || attempt) {
      err = pw.empty() ? tr("Shelly verlangt ein Passwort") : tr("Passwort falsch");
      return false;
    }
    // Herausforderung steht als JSON-Text in error.message
    JsonDocument ch;
    if (deserializeJson(ch, (const char *)(res["error"]["message"] | "{}"))) {
      err = tr("Anmeldung unlesbar");
      return false;
    }
    std::string realm = ch["realm"] | "", nonce = std::to_string((unsigned long)(ch["nonce"] | 0UL));
    std::string cnonce = std::to_string(esp_random() % 1000000000UL);
    std::string ha1 = sha256hex("admin:" + realm + ":" + pw), ha2 = sha256hex("dummy_method:dummy_uri");
    std::string resp = sha256hex(ha1 + ":" + nonce + ":1:" + cnonce + ":auth:" + ha2);
    auth = "{\"realm\":\"" + realm + "\",\"username\":\"admin\",\"nonce\":" + nonce + ",\"cnonce\":" + cnonce +
           ",\"response\":\"" + resp + "\",\"algorithm\":\"SHA-256\"}";
  }
  return false;
}

// Kanaele aus Shelly.GetStatus (switch:N mit output/apower/aenergy, light:N mit output/brightness)
std::vector<ShbChannel> parse_status(JsonObjectConst r) {
  std::vector<ShbChannel> out;
  for (JsonPairConst kv : r) {
    std::string k = kv.key().c_str();
    bool sw = k.rfind("switch:", 0) == 0, li = k.rfind("light:", 0) == 0;
    if (!sw && !li) continue;
    JsonObjectConst v = kv.value().as<JsonObjectConst>();
    ShbChannel c;
    c.comp = k;
    c.known = v["output"].is<bool>();
    c.on = v["output"] | false;
    if (li && v["brightness"].is<int>()) c.bri = v["brightness"];
    if (v["apower"].is<float>()) c.power = v["apower"];
    if (v["aenergy"]["total"].is<float>()) c.energy = v["aenergy"]["total"].as<float>() / 1000.0f;
    out.push_back(c);
  }
  std::sort(out.begin(), out.end(), [](const ShbChannel &a, const ShbChannel &b) { return a.comp < b.comp; });
  return out;
}

std::string fmt_num(float v, int dec) {
  char b[24];
  snprintf(b, sizeof(b), "%.*f", dec, v);
#if UI_DECIMAL_COMMA
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
#endif
  return b;
}

void fill_view(const Dev &d, const ShbChannel &c, ControlView &v) {
  v.key = "shb:" + d.mac + "#" + c.comp;
  v.ieee = d.mac;
  v.ep = c.comp;
  bool multi = d.ch.size() > 1;
  v.name = d.name.empty() ? d.mac : d.name;
  if (multi) {
    size_t p = c.comp.find(':');
    v.name += " " + std::to_string(atoi(c.comp.c_str() + p + 1) + 1);
  }
  v.model = d.model;
  v.vendor = "Shelly";
  v.description = "Bluetooth";
  bool light = c.comp.rfind("light:", 0) == 0;
  v.kind = light ? CTL_LIGHT : CTL_SWITCH;
  v.has_bri = light;
  v.bri_max = 100;
  v.bri = light ? c.bri : -1;
  v.known = c.known;
  v.on = c.on;
  v.online = d.last_ok && (int32_t)((millis() - d.last_ok) / 1000) < OFFLINE_S;
  for (auto &m : d.cmds) v.pending |= m.comp == c.comp;
  if (!isnan(c.power)) {
    v.info = fmt_num(c.power, c.power < 10 ? 1 : 0) + " W";
    if (!isnan(c.energy)) v.info += "  ·  " + fmt_num(c.energy, 2) + " kWh";
  }
}

bool split(const std::string &key, std::string &mac, std::string &comp) {
  if (!shb_is_key(key)) return false;
  size_t h = key.find('#');
  if (h == std::string::npos) return false;
  mac = key.substr(4, h - 4);
  comp = key.substr(h + 1);
  return true;
}

void queue(const std::string &key, int8_t on, int bri) {
  std::string mac, comp;
  if (!split(key, mac, comp) || !s_mx) return;
  Lock l;
  Dev *d = find(mac);
  if (!d || d->cmds.size() > 6) return;
  for (auto &c : d->ch)   // sofort anzeigen; der naechste Status bestaetigt bzw. korrigiert
    if (c.comp == comp) {
      if (on < 0) c.on = !c.on;
      else c.on = on;
      if (bri >= 0) c.bri = bri;
    }
  d->cmds.push_back({comp, on, bri});
  d->next_poll = 0;
  s_ver++;
}

}  // namespace

void shb_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  Lock l;
  load();
}

bool shb_enabled() { return s_on; }

void shb_set_enabled(bool on) {
  if (!s_mx) return;
  Lock l;
  s_on = on;
  save();
  s_ver++;
}

std::vector<ShbInfo> shb_list() {
  std::vector<ShbInfo> r;
  if (!s_mx) return r;
  Lock l;
  for (auto &d : s_devs) {
    ShbInfo i;
    i.mac = d.mac;
    i.name = d.name;
    i.model = d.model;
    i.status = d.status;
    i.has_pw = !d.pw.empty();
    i.auth = d.auth;
    i.online = d.last_ok && (int32_t)((millis() - d.last_ok) / 1000) < OFFLINE_S;
    i.age_s = d.last_ok ? (int32_t)((millis() - d.last_ok) / 1000) : -1;
    i.rssi = d.rssi;
    i.ch = d.ch;
    r.push_back(i);
  }
  return r;
}

std::vector<ShbFound> shb_found() {
  if (!s_mx) return {};
  Lock l;
  std::vector<ShbFound> r = s_found;
  for (auto &f : r) f.configured = find(f.mac) != nullptr;
  return r;
}

void shb_search(uint32_t ms) {
  if (!s_mx) return;
  Lock l;
  s_search_until = ms ? millis() + ms : 0;
  if (ms) s_found.clear();
  s_ver++;
}

bool shb_searching() { return s_search_until != 0; }

std::string shb_add(const std::string &mac_in, uint8_t type, const std::string &name, const std::string &pw) {
  if (!s_mx) return tr("nicht verfügbar");
  std::string mac = lower(mac_in);
  Lock l;
  if (find(mac)) return tr("schon eingerichtet");
  if (s_devs.size() >= SHB_MAX) return tr("Kein Platz mehr – es sind schon 8 Shellys eingerichtet.");
  Dev d;
  d.mac = mac;
  d.type = type;
  d.name = name;
  d.pw = pw;
  d.status = tr("Warte auf erste Verbindung …");
  s_devs.push_back(d);
  s_on = true;
  save();
  s_ver++;
  return "";
}

void shb_remove(const std::string &mac) {
  if (!s_mx) return;
  Lock l;
  for (size_t i = 0; i < s_devs.size(); i++)
    if (s_devs[i].mac == mac) {
      s_devs.erase(s_devs.begin() + i);
      break;
    }
  save();
  s_ver++;
}

void shb_rename(const std::string &mac, const std::string &name) {
  if (!s_mx) return;
  Lock l;
  if (Dev *d = find(mac)) d->name = name;
  save();
  s_ver++;
}

void shb_set_password(const std::string &mac, const std::string &pw) {
  if (!s_mx) return;
  Lock l;
  if (Dev *d = find(mac)) {
    d->pw = pw;
    d->next_poll = 0;
    d->fails = 0;
  }
  save();
  s_ver++;
}

uint32_t shb_version() { return s_ver; }

bool shb_get(const std::string &key, ControlView &out) {
  std::string mac, comp;
  if (!split(key, mac, comp) || !s_mx) return false;
  Lock l;
  Dev *d = find(mac);
  if (!d) return false;
  for (auto &c : d->ch)
    if (c.comp == comp) {
      fill_view(*d, c, out);
      return true;
    }
  return false;
}

void shb_all(std::vector<ControlView> &out) {
  if (!s_mx) return;
  Lock l;
  for (auto &d : s_devs)
    for (auto &c : d.ch) {
      ControlView v;
      fill_view(d, c, v);
      out.push_back(std::move(v));
    }
}

size_t shb_count() {
  if (!s_mx) return 0;
  Lock l;
  size_t n = 0;
  for (auto &d : s_devs) n += d.ch.size();
  return n;
}

void shb_toggle(const std::string &key) { queue(key, -1, -1); }
void shb_set_on(const std::string &key, bool on) { queue(key, on ? 1 : 0, -1); }
void shb_set_brightness_pct(const std::string &key, int pct) { queue(key, 1, std::max(0, std::min(100, pct))); }

// ---------------------------------------------------------------- Bluetooth-Verteiler (Bluetooth-Task)
bool shb_ble_needed() {
  if (!s_mx) return false;
  Lock l;
  return s_on && (!s_devs.empty() || s_search_until);
}

bool shb_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!s_mx) return false;
  std::string name = dev->haveName() ? dev->getName() : "";
  bool hit = lower(name).rfind("shelly", 0) == 0;
  if (!hit && dev->haveManufacturerData()) {   // Gen3/Gen4: Hersteller-ID 0x0BA9 (BLU-Sensoren senden BTHome 0xFCD2)
    std::string md = dev->getManufacturerData();
    hit = md.size() >= 2 && ((uint8_t)md[0] | (uint8_t)md[1] << 8) == SHELLY_CID &&
          !dev->isAdvertisingService(NimBLEUUID((uint16_t)0xFCD2));
  }
  if (!hit && dev->haveServiceUUID()) hit = dev->isAdvertisingService(NimBLEUUID(SVC));
  if (!hit) return false;
  std::string mac = lower(dev->getAddress().toString());
  Lock l;
  bool used = false;
  if (Dev *d = find(mac)) {
    d->type = dev->getAddress().getType();
    d->rssi = dev->getRSSI();
    used = true;
  }
  if (s_search_until) {
    used = true;
    for (auto &f : s_found)
      if (f.mac == mac) {
        f.rssi = dev->getRSSI();
        if (!name.empty()) f.name = name;
        return true;
      }
    if (s_found.size() < 16) {
      ShbFound f;
      f.mac = mac;
      f.name = name;
      f.type = dev->getAddress().getType();
      f.rssi = dev->getRSSI();
      s_found.push_back(f);
      s_ver++;
    }
  }
  return used;
}

void shb_scan_wanted(bool &, bool &active) {
  if (!s_mx) return;
  Lock l;
  if (s_search_until && (int32_t)(millis() - s_search_until) >= 0) {
    s_search_until = 0;
    s_ver++;
  }
  if (s_on && s_search_until) active = true;   // Namen stehen oft erst in der Scan-Antwort
}

bool shb_want_connect() {
  if (!s_mx) return false;
  Lock l;
  if (!s_on) return false;
  for (auto &d : s_devs)
    if (!d.cmds.empty() || (int32_t)(millis() - d.next_poll) >= 0) return true;
  return false;
}

void shb_connect_now() {
  // faelliges Geraet waehlen: zuerst eins mit Befehlen
  Dev job;
  {
    Lock l;
    Dev *pick = nullptr;
    for (auto &d : s_devs)
      if (!d.cmds.empty()) {
        pick = &d;
        break;
      }
    if (!pick)
      for (auto &d : s_devs)
        if ((int32_t)(millis() - d.next_poll) >= 0) {
          pick = &d;
          break;
        }
    if (!pick) return;
    job = *pick;
    pick->cmds.clear();
    pick->next_poll = millis() + POLL_MS;   // falls es scheitert, unten neu gesetzt
  }
  NimBLEClient *c = NimBLEDevice::createClient();
  c->setConnectTimeout(8000);
  bool ok = c->connect(NimBLEAddress(job.mac, job.type), true);
  std::string err, model = job.model;
  bool auth_needed = false, info_ok = job.info_done;
  std::vector<ShbChannel> ch;
  if (ok) {
    JsonDocument res;
    if (!info_ok && rpc(c, "Shelly.GetDeviceInfo", "", job.pw, res, err, auth_needed)) {
      JsonObjectConst r = res["result"];
      const char *app = r["app"] | "", *mo = r["model"] | "";
      model = *app ? app : *mo ? mo : "Shelly";
      info_ok = true;
    }
    for (auto &m : job.cmds) {
      bool light = m.comp.rfind("light:", 0) == 0;
      std::string id = m.comp.substr(m.comp.find(':') + 1);
      std::string method = std::string(light ? "Light." : "Switch.") + (m.on < 0 ? "Toggle" : "Set");
      std::string params = "{\"id\":" + id + (m.on >= 0 ? std::string(",\"on\":") + (m.on ? "true" : "false") : "") +
                           (light && m.bri >= 0 ? ",\"brightness\":" + std::to_string(m.bri) : "") + "}";
      if (!rpc(c, method, params, job.pw, res, err, auth_needed)) break;
    }
    if (err.empty() && rpc(c, "Shelly.GetStatus", "", job.pw, res, err, auth_needed))
      ch = parse_status(res["result"].as<JsonObjectConst>());
    c->disconnect();
  } else {
    err = tr("nicht erreichbar – in Reichweite? Bluetooth am Shelly an?");
  }
  NimBLEDevice::deleteClient(c);
  Lock l;
  Dev *d = find(job.mac);
  if (!d) return;
  d->auth = auth_needed;
  d->model = model;
  d->info_done = info_ok;
  if (err.empty()) {
    d->ch = ch;
    d->last_ok = millis();
    d->status = "";
    d->fails = 0;
    d->next_poll = millis() + POLL_MS;
  } else {
    d->status = err;
    d->fails = d->fails < 8 ? d->fails + 1 : 8;
    d->next_poll = millis() + std::min<uint32_t>(POLL_MS * d->fails, 300000);
    log_w("Shelly-BT %s: %s", job.mac.c_str(), err.c_str());
  }
  s_ver++;
}

#endif   // WITH_SHELLY_BLE
