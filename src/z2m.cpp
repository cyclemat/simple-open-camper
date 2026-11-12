#include "z2m.h"
#include "i18n/i18n.h"
#include "settings.h"
#include "app.h"
#include "net.h"
#include "shelly.h"
#include "wled.h"
#include "panelsat.h"
#include "layout.h"
#include <LittleFS.h>

#include <Arduino.h>
#include <atomic>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <algorithm>
#include <set>
#include <unordered_map>
#include "esp_heap_caps.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ---------------------------------------------------------------- PSRAM-Allokator fuer ArduinoJson
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
static SpiRamAllocator s_psram;

// ---------------------------------------------------------------- Modell
struct Control {
  std::string key, ep;
  CtlKind kind = CTL_LIGHT;
  std::string p_state, p_bri, p_ct, p_color;
  std::string v_on = "ON", v_off = "OFF", v_toggle = "TOGGLE";
  bool bool_vals = false;
  int bri_max = 254, ct_min = 153, ct_max = 500;
  uint8_t color = COLOR_NONE;
  bool known = false, on = false;
  int bri = -1, ct = -1;
  float hue = -1, sat = -1, cx = -1, cy = -1;
  std::string cmode;
  uint32_t pending_until = 0;
};
struct Device {
  std::string ieee, name, model, vendor, description;
  bool online = true;
  std::vector<Control> controls;
};
enum TopicType : uint8_t { T_STATE = 1, T_AVAIL = 2 };
struct TopicRef { int dev; TopicType type; };

static SemaphoreHandle_t s_mtx;
static std::vector<Device> s_devs;
static std::unordered_map<std::string, std::pair<int, int>> s_key_idx;
static std::unordered_map<std::string, TopicRef> s_topic_idx;
static std::atomic<uint32_t> s_dev_ver{1}, s_state_ver{1};

namespace {   // dateilokal: gleichnamige Typen anderer Dateien duerfen sich nicht vermischen
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

// ---------------------------------------------------------------- MQTT
static esp_mqtt_client_handle_t s_client = nullptr;
static volatile bool s_connected = false;
static volatile bool s_bridge_online = false;
static char s_status[80] = "Nicht eingerichtet";
static char s_diag[120] = "Noch keine Geräteliste empfangen";
static volatile uint32_t s_connected_at = 0;
static volatile bool s_got_devices = false;
static std::string s_base;          // Basis-Topic, z. B. "zigbee2mqtt"
static std::string s_host, s_user, s_pass, s_client_id;
static uint16_t s_port = 1883;
static String s_auto_host;               // im Panel-WLAN gefundener Broker
static volatile uint32_t s_disc_since = 0;
static std::set<std::string> s_subscribed;

// Empfangspuffer fuer fragmentierte Nachrichten (grosse bridge/devices). Bei ~150 Geraeten ist die
// Geraeteliste von Zigbee2MQTT mehrere MB gross – sie liegt komplett im PSRAM.
static const size_t RX_MAX = 8 * 1024 * 1024;
static const size_t PSRAM_RESERVE = 512 * 1024;   // fuer Hintergruende, Oberflaeche, Auswertung
static char *s_rx = nullptr;
static size_t s_rx_cap = 0;
static std::string s_rx_topic;
static bool s_rx_skip = false;                     // aktuelle Nachricht passt nicht – Rest ignorieren

enum JobType : uint8_t { JOB_DEVICES = 1, JOB_RESUBSCRIBE, JOB_REFRESH, JOB_RESTART, JOB_RELOAD_DEVICES };
struct Job { JobType type; char *data; size_t len; };
static QueueHandle_t s_jobs;
static std::vector<std::string> s_get_queue;   // Topics, an die {"state":""} geschickt wird (gedrosselt)
static std::vector<std::string> s_get_payload;

// ---------------------------------------------------------------- Zwischenspeicher der Geraeteliste
// Die Geraeteliste von Zigbee2MQTT ist gross (bei ~130 Geraeten ueber 500 KB) und laeuft beim Empfang durch die
// WLAN/TCP-Puffer im knappen internen RAM. Deshalb wird sie nur auf Anfrage geholt (Editor geoeffnet, Knopf
// "Geraeteliste neu laden", Umbenennen) und das Noetige klein in LittleFS gemerkt. Beim Start reicht diese Datei.
// Abonniert werden nur die Geraete, die auf dem Panel liegen.
static const char *CACHE_PATH = "/z2m_devices.json";
static bool s_cache_loaded = false;        // Modell stammt aus dem Zwischenspeicher
static std::string s_cache_base;           // Basis-Topic, unter dem der Zwischenspeicher entstand
static volatile bool s_devices_wanted = false;   // bridge/devices gerade abonniert (Liste wird geholt)
static std::set<std::string> s_placed;     // IEEE-Adressen der Geraete auf dem Panel
static uint32_t s_placed_ver = 0;          // Layout-Version, fuer die s_placed gilt

static void publish(const std::string &topic, const char *payload) {
  if (!s_client || !s_connected) return;
  esp_mqtt_client_enqueue(s_client, topic.c_str(), payload, 0, 0, 0, true);
}

static void handle_state(int di, const char *payload, size_t len) {
  JsonDocument doc(&s_psram);   // nicht im knappen internen RAM – nach dem Abonnieren kommen Hunderte auf einmal
  if (deserializeJson(doc, payload, len)) return;
  JsonObject o = doc.as<JsonObject>();
  if (o.isNull()) return;
  Lock l;
  if (di < 0 || di >= (int)s_devs.size()) return;
  for (auto &c : s_devs[di].controls) {
    JsonVariant v = o[c.p_state];
    if (!v.isNull()) {
      if (v.is<bool>()) c.on = v.as<bool>();
      else if (v.is<const char *>()) c.on = c.v_on == v.as<const char *>();
      c.known = true;
      c.pending_until = 0;
    }
    if (!c.p_bri.empty() && o[c.p_bri].is<int>()) c.bri = o[c.p_bri].as<int>();
    if (!c.p_ct.empty() && o[c.p_ct].is<int>()) c.ct = o[c.p_ct].as<int>();
    if (!c.p_color.empty()) {
      JsonObject col = o[c.p_color];
      if (!col.isNull()) {
        if (col["x"].is<float>()) c.cx = col["x"].as<float>();
        if (col["y"].is<float>()) c.cy = col["y"].as<float>();
        if (col["hue"].is<float>()) c.hue = col["hue"].as<float>();
        if (col["saturation"].is<float>()) c.sat = col["saturation"].as<float>();
      }
    }
    std::string cm_key = "color_mode";
    if (!c.ep.empty() && o["color_mode_" + c.ep].is<const char *>()) cm_key = "color_mode_" + c.ep;
    if (o[cm_key].is<const char *>()) c.cmode = o[cm_key].as<const char *>();
  }
  s_state_ver++;
}

static void handle_avail(int di, const char *payload, size_t len) {
  bool online = true;
  if (len && payload[0] == '{') {
    JsonDocument doc(&s_psram);
    if (deserializeJson(doc, payload, len)) return;
    online = strcmp(doc["state"] | "online", "online") == 0;
  } else {
    online = len >= 6 && strncmp(payload, "online", 6) == 0;
  }
  Lock l;
  if (di >= 0 && di < (int)s_devs.size()) s_devs[di].online = online;
  s_state_ver++;
}

static void handle_message(const std::string &topic, char *payload, size_t len) {
  const std::string b = s_base + "/bridge/";
  if (topic.compare(0, b.size(), b) == 0) {
    std::string sub = topic.substr(b.size());
    if (sub == "devices") {
      s_got_devices = true;
      snprintf(s_diag, sizeof(s_diag), tr("Geräteliste empfangen (%u KB), wird ausgewertet …"), (unsigned)(len / 1024));
      char *copy;
      if (payload == s_rx) {
        // Empfangspuffer direkt weitergeben statt mehrere MB zu kopieren; fuer die naechste Nachricht gibt es einen neuen
        copy = s_rx;
        s_rx = nullptr;
        s_rx_cap = 0;
      } else {
        copy = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!copy) return;
        memcpy(copy, payload, len);
      }
      copy[len] = 0;
      Job j{JOB_DEVICES, copy, len};
      if (xQueueSend(s_jobs, &j, pdMS_TO_TICKS(200)) != pdTRUE) heap_caps_free(copy);
    } else if (sub == "state") {
      bool on = strstr(payload, "online") != nullptr;
      s_bridge_online = on;
      s_state_ver++;
    } else if (sub == "response/device/rename") {
      JsonDocument doc;
      if (!deserializeJson(doc, payload, len)) {
        if (strcmp(doc["status"] | "", "ok") == 0) {
          app_toast("In Zigbee2MQTT umbenannt: %s", (const char *)(doc["data"]["to"] | ""));
          Job j{JOB_RELOAD_DEVICES, nullptr, 0};   // neuer Name: Liste neu holen (Topics aendern sich)
          xQueueSend(s_jobs, &j, 0);
        }
        else
          app_toast("Umbenennen fehlgeschlagen: %s", (const char *)(doc["error"] | "unbekannt"));
      }
    }
    return;
  }
  if (shelly_handle(topic, payload, len)) return;
  if (wled_handle(topic, payload, len)) return;
  if (panelsat_handle(topic, payload, len)) return;
  int di = -1;
  TopicType t = T_STATE;
  {
    Lock l;
    auto it = s_topic_idx.find(topic);
    if (it == s_topic_idx.end()) return;
    di = it->second.dev;
    t = it->second.type;
  }
  if (t == T_STATE) handle_state(di, payload, len);
  else handle_avail(di, payload, len);
}

static void subscribe(const std::string &t) {
  if (!s_client || !s_connected) return;
  if (esp_mqtt_client_subscribe(s_client, t.c_str(), 0) >= 0) s_subscribed.insert(t);
}
static void unsubscribe(const std::string &t) {
  if (s_client && s_connected) esp_mqtt_client_unsubscribe(s_client, t.c_str());
  s_subscribed.erase(t);
}

// Viele Topics gebuendelt abonnieren: Jede SUBSCRIBE-Nachricht liegt bis zur Bestaetigung im internen RAM
// der MQTT-Bibliothek. Bei ~150 Geraeten (300 Topics) einzeln abonniert war der interne RAM voll und das
// Panel stuerzte ab. Gebuendelt sind es ~10 Pakete; dazwischen kurz warten, damit der Broker bestaetigt.
static void subscribe_many(const std::vector<std::string> &topics) {
  if (!s_client || !s_connected || topics.empty()) return;
  // Kleine Pakete: Auf jedes Abo antwortet der Broker sofort mit dem gespeicherten Zustand. Kommen
  // 140 davon auf einmal, laufen die WLAN-Empfangspuffer im internen RAM ueber (Absturz bei ~150 Geraeten).
  const size_t MAX_BYTES = 600;    // etwa 12 Topics je Paket
  const size_t MIN_FREE = 24 * 1024;
  std::vector<esp_mqtt_topic_t> batch;
  std::vector<const std::string *> names;
  size_t bytes = 0, sent = 0;
  auto flush = [&]() {
    if (batch.empty()) return;
    if (esp_mqtt_client_subscribe_multiple(s_client, batch.data(), batch.size()) >= 0) {
      for (auto *n : names) s_subscribed.insert(*n);
      sent += batch.size();
    } else {
      log_w("Abo-Paket mit %u Topics abgelehnt", (unsigned)batch.size());
    }
    batch.clear();
    names.clear();
    bytes = 0;
    // Erst weiter, wenn die Antworten abgearbeitet sind und wieder genug interner RAM frei ist
    vTaskDelay(pdMS_TO_TICKS(150));
    for (int i = 0; i < 40 && s_connected && heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < MIN_FREE; i++)
      vTaskDelay(pdMS_TO_TICKS(100));
  };
  for (auto &t : topics) {
    if (s_subscribed.count(t)) continue;
    if (bytes + t.size() + 3 > MAX_BYTES) flush();
    if (!s_connected) return;
    batch.push_back({t.c_str(), 0});
    names.push_back(&t);
    bytes += t.size() + 3;
  }
  flush();
  log_i("MQTT: %u Topics abonniert (frei intern: %u)", (unsigned)sent, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  auto *ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED: {
      s_connected = true;
      s_disc_since = 0;
      s_connected_at = millis();
      snprintf(s_status, sizeof(s_status), tr("Verbunden mit %s"), s_host.c_str());
      Job j{JOB_RESUBSCRIBE, nullptr, 0};
      xQueueSend(s_jobs, &j, 0);
      break;
    }
    case MQTT_EVENT_DISCONNECTED:
      if (s_connected) {
        snprintf(s_status, sizeof(s_status), "Getrennt, verbinde neu …");
        log_w("MQTT getrennt (frei intern: %u, größter Block: %u)", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
              (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
      }
      if (s_connected || !s_disc_since) s_disc_since = millis();
      s_connected = false;
      s_bridge_online = false;
      break;
    case MQTT_EVENT_ERROR:
      if (ev->error_handle) {
        if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
          int rc = ev->error_handle->connect_return_code;
          snprintf(s_status, sizeof(s_status), "%s",
                   (rc == MQTT_CONNECTION_REFUSE_BAD_USERNAME || rc == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED)
                       ? "Zugang abgelehnt – Benutzer/Passwort prüfen"
                       : "Verbindung vom Broker abgelehnt");
        } else if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
          snprintf(s_status, sizeof(s_status), tr("Broker %s:%u nicht erreichbar"), s_host.c_str(), (unsigned)s_port);
        }
      }
      break;
    case MQTT_EVENT_DATA: {
      size_t total = ev->total_data_len;
      if (ev->current_data_offset == 0) {
        s_rx_topic.assign(ev->topic, ev->topic_len);
        s_rx_skip = false;
        const char *why = nullptr;
        if (total > RX_MAX) {
          why = "zu groß";
        } else if (total + 1 > s_rx_cap) {
          // neu anlegen statt realloc: der alte Inhalt wird nicht gebraucht und muss nicht kopiert werden
          heap_caps_free(s_rx);
          s_rx = nullptr;
          s_rx_cap = 0;
          if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < total + 1 + PSRAM_RESERVE) why = "zu wenig Speicher";
          else if ((s_rx = (char *)heap_caps_malloc(total + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) s_rx_cap = total + 1;
          else why = "zu wenig Speicher";
        }
        if (why) {
          s_rx_skip = true;
          log_e("MQTT-Nachricht %.*s (%u KB) verworfen: %s", ev->topic_len, ev->topic, (unsigned)(total / 1024), why);
          if (s_rx_topic == s_base + "/bridge/devices") {
            s_got_devices = true;
            snprintf(s_diag, sizeof(s_diag), tr("Geräteliste %s (%u KB, frei %u KB)"), why, (unsigned)(total / 1024),
                     (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
            s_state_ver++;
          }
          return;
        }
      }
      if (s_rx_skip || !s_rx || ev->current_data_offset + ev->data_len > s_rx_cap) return;
      // Messung bei grossen Nachrichten: Verlauf des internen RAM waehrend des Empfangs
      if (total > 256 * 1024) {
        size_t a = ev->current_data_offset, b = a + ev->data_len;
        if (a == 0 || a / (256 * 1024) != b / (256 * 1024))
          log_i("Empfang %s: %u/%u KB, frei intern %u (Tiefstwert %u, größter Block %u)", s_rx_topic.c_str(),
                (unsigned)(b / 1024), (unsigned)(total / 1024), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
      }
      memcpy(s_rx + ev->current_data_offset, ev->data, ev->data_len);
      if (ev->current_data_offset + ev->data_len >= total) {
        s_rx[total] = 0;
        handle_message(s_rx_topic, s_rx, total);
      }
      break;
    }
    default: break;
  }
}

static void stop_client() {
  if (!s_client) return;
  esp_mqtt_client_stop(s_client);
  esp_mqtt_client_destroy(s_client);
  s_client = nullptr;
  s_connected = false;
  s_bridge_online = false;
  s_subscribed.clear();
}

static void start_client() {
  stop_client();
  Settings st = settings_copy();
  if (!st.mqtt_on) {
    snprintf(s_status, sizeof(s_status), "MQTT ausgeschaltet");
    return;
  }
  bool auto_mode = (st.net_mode == NET_AP || st.net_mode == NET_APSTA) && st.mqtt_auto;
  if (auto_mode) st.mqtt_host = s_auto_host;
  if (st.mqtt_host.isEmpty()) {
    snprintf(s_status, sizeof(s_status), "Nicht eingerichtet");
    return;
  }
  s_host = st.mqtt_host.c_str();
  s_user = st.mqtt_user.c_str();
  s_pass = st.mqtt_pass.c_str();
  s_base = st.mqtt_base.c_str();
  s_port = st.mqtt_port;
  while (!s_base.empty() && s_base.back() == '/') s_base.pop_back();
  char cid[40];
  snprintf(cid, sizeof(cid), "simply-open-camper-%06llx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF));
  s_client_id = cid;

  esp_mqtt_client_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.broker.address.hostname = s_host.c_str();
  cfg.broker.address.port = s_port;
  cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  cfg.credentials.client_id = s_client_id.c_str();
  if (!s_user.empty()) cfg.credentials.username = s_user.c_str();
  if (!s_pass.empty()) cfg.credentials.authentication.password = s_pass.c_str();
  cfg.session.keepalive = 30;
  cfg.network.reconnect_timeout_ms = 5000;
  cfg.network.timeout_ms = 8000;
  cfg.buffer.size = 4096;
  cfg.buffer.out_size = 2048;
  cfg.task.stack_size = 6144;   // gemessen: max. ~2,7 KB benutzt
  cfg.task.priority = 5;

  s_client = esp_mqtt_client_init(&cfg);
  if (!s_client) {
    snprintf(s_status, sizeof(s_status), "MQTT-Client konnte nicht starten");
    return;
  }
  esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event, nullptr);
  esp_mqtt_client_start(s_client);
  snprintf(s_status, sizeof(s_status), tr("Verbinde mit %s …"), s_host.c_str());
}

// ---------------------------------------------------------------- Geraeteliste auswerten
static void rebuild_index_locked() {
  s_key_idx.clear();
  s_topic_idx.clear();
  for (int d = 0; d < (int)s_devs.size(); d++) {
    for (int c = 0; c < (int)s_devs[d].controls.size(); c++) s_key_idx[s_devs[d].controls[c].key] = {d, c};
    s_topic_idx[s_base + "/" + s_devs[d].name] = {d, T_STATE};
    s_topic_idx[s_base + "/" + s_devs[d].name + "/availability"] = {d, T_AVAIL};
  }
}

static void queue_gets_for(const Device &d) {
  JsonDocument doc;
  bool any = false;
  for (auto &c : d.controls)
    if (!c.known) {
      doc[c.p_state] = "";
      any = true;
    }
  if (!any) return;
  std::string p;
  serializeJson(doc, p);
  s_get_queue.push_back(s_base + "/" + d.name + "/get");
  s_get_payload.push_back(p);
}

static void parse_devices_inner(char *json, size_t len);
static void save_cache();
static void sync_device_topics(bool force);

// Kurze Schluessel, Standardwerte weggelassen – bei ~70 schaltbaren Geraeten etwa 20 KB
static void save_cache() {
  JsonDocument doc(&s_psram);
  {
    Lock l;
    doc["base"] = s_base;
    JsonArray da = doc["devs"].to<JsonArray>();
    for (auto &d : s_devs) {
      JsonObject o = da.add<JsonObject>();
      o["i"] = d.ieee;
      o["n"] = d.name;
      if (!d.model.empty()) o["m"] = d.model;
      if (!d.vendor.empty()) o["v"] = d.vendor;
      if (!d.description.empty()) o["d"] = d.description;
      JsonArray ca = o["c"].to<JsonArray>();
      for (auto &c : d.controls) {
        JsonObject x = ca.add<JsonObject>();
        if (!c.ep.empty()) x["e"] = c.ep;
        if (c.kind != CTL_LIGHT) x["k"] = (int)c.kind;
        x["s"] = c.p_state;
        if (!c.p_bri.empty()) { x["b"] = c.p_bri; if (c.bri_max != 254) x["bm"] = c.bri_max; }
        if (!c.p_ct.empty()) { x["t"] = c.p_ct; x["t0"] = c.ct_min; x["t1"] = c.ct_max; }
        if (!c.p_color.empty()) { x["c"] = c.p_color; x["cc"] = c.color; }
        if (c.bool_vals) x["bv"] = true;
        if (c.v_on != "ON") x["on"] = c.v_on;
        if (c.v_off != "OFF") x["off"] = c.v_off;
        if (c.v_toggle != "TOGGLE") x["tg"] = c.v_toggle;
      }
    }
  }
  File f = LittleFS.open("/z2m_devices.tmp", "w");
  if (!f) return;
  size_t n = serializeJson(doc, f);
  f.close();
  LittleFS.remove(CACHE_PATH);
  if (n && LittleFS.rename("/z2m_devices.tmp", CACHE_PATH)) {
    s_cache_loaded = true;
    s_cache_base = s_base;
    log_i("Geräteliste zwischengespeichert (%u Bytes)", (unsigned)n);
  }
}

// Beim Start: Modell aus dem Zwischenspeicher, damit die Kacheln sofort stimmen und die grosse Liste nicht noetig ist
static void load_cache() {
  File f = LittleFS.open(CACHE_PATH, "r");
  if (!f) return;
  JsonDocument doc(&s_psram);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    log_w("Zwischenspeicher der Geräteliste unlesbar: %s", err.c_str());
    return;
  }
  heap_caps_malloc_extmem_enable(16);   // viele kleine Texte: in den PSRAM (siehe parse_devices)
  std::vector<Device> devs;
  for (JsonObject o : doc["devs"].as<JsonArray>()) {
    Device d;
    d.ieee = o["i"] | "";
    d.name = o["n"] | "";
    d.model = o["m"] | "";
    d.vendor = o["v"] | "";
    d.description = o["d"] | "";
    for (JsonObject x : o["c"].as<JsonArray>()) {
      Control c;
      c.ep = x["e"] | "";
      c.key = c.ep.empty() ? d.ieee : d.ieee + "#" + c.ep;
      c.kind = (CtlKind)(x["k"] | (int)CTL_LIGHT);
      c.p_state = x["s"] | "state";
      c.p_bri = x["b"] | "";
      c.bri_max = x["bm"] | 254;
      c.p_ct = x["t"] | "";
      c.ct_min = x["t0"] | 153;
      c.ct_max = x["t1"] | 500;
      c.p_color = x["c"] | "";
      c.color = x["cc"] | 0;
      c.bool_vals = x["bv"] | false;
      c.v_on = x["on"] | "ON";
      c.v_off = x["off"] | "OFF";
      c.v_toggle = x["tg"] | "TOGGLE";
      d.controls.push_back(c);
    }
    if (!d.ieee.empty() && !d.name.empty() && !d.controls.empty()) devs.push_back(std::move(d));
  }
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  Lock l;
  s_devs.swap(devs);
  heap_caps_malloc_extmem_enable(16);
  rebuild_index_locked();   // Kacheln finden ihr Geraet sofort; die Topics stimmen erst nach dem Verbinden (Basis-Topic)
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  s_cache_base = doc["base"] | "";
  s_cache_loaded = true;
  heap_caps_malloc_extmem_enable(16);
  rebuild_index_locked();   // Kacheln finden ihre Geraete sofort; die Topics stimmen nach dem Verbinden (Basis-Topic)
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  s_dev_ver++;
  snprintf(s_diag, sizeof(s_diag), tr("%u Geräte mit Lampe/Schalter aus dem Zwischenspeicher – im Editor neu laden"),
           (unsigned)s_devs.size());
  log_i("Geräteliste aus dem Zwischenspeicher: %u Geräte", (unsigned)s_devs.size());
}

// Das Geraetemodell besteht aus sehr vielen kleinen Texten (Namen, Topics, Eigenschaften). Solche Bloecke
// unter 1 KB landen sonst im internen RAM – bei ~70 schaltbaren Geraeten reichte der nicht mehr (Absturz).
// Waehrend des Aufbaus duerfen deshalb auch kleine Bloecke in den PSRAM.
static void parse_devices(char *json, size_t len) {
  heap_caps_malloc_extmem_enable(16);
  parse_devices_inner(json, len);
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  log_i("Geräteliste ausgewertet, frei intern %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static void parse_devices_inner(char *json, size_t len) {
  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  f["ieee_address"] = true;
  f["friendly_name"] = true;
  f["type"] = true;
  f["disabled"] = true;
  JsonObject fd = f["definition"].to<JsonObject>();
  fd["model"] = true;
  fd["vendor"] = true;
  fd["description"] = true;
  JsonObject fe = fd["exposes"][0].to<JsonObject>();
  fe["type"] = true;
  fe["endpoint"] = true;
  JsonObject ff = fe["features"][0].to<JsonObject>();
  for (const char *k : {"name", "property", "value_on", "value_off", "value_toggle", "value_min", "value_max"}) ff[k] = true;

  JsonDocument doc(&s_psram);
  // Die Geraeteliste ist tief verschachtelt (z. B. Thermostat-Zeitplaene). Mit der Standardgrenze (10)
  // wuerde die gesamte Liste verworfen – deshalb deutlich hoeher.
  DeserializationError err = deserializeJson(doc, json, len, DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(64));
  if (err) {
    log_e("bridge/devices (%u Bytes): %s", (unsigned)len, err.c_str());
    snprintf(s_diag, sizeof(s_diag), tr("Geräteliste nicht lesbar (%s, %u KB)"), err.c_str(), (unsigned)(len / 1024));
    s_state_ver++;
    return;
  }
  std::vector<Device> devs;
  int total = 0, no_def = 0;
  for (JsonObject d : doc.as<JsonArray>()) {
    if (strcmp(d["type"] | "", "Coordinator") == 0) continue;
    total++;
    if (d["disabled"] | false) continue;
    JsonObject def = d["definition"];
    if (def.isNull()) {
      no_def++;
      continue;
    }
    Device dev;
    dev.ieee = d["ieee_address"] | "";
    dev.name = d["friendly_name"] | "";
    dev.model = def["model"] | "";
    dev.vendor = def["vendor"] | "";
    dev.description = def["description"] | "";
    if (dev.ieee.empty() || dev.name.empty()) continue;
    for (JsonObject ex : def["exposes"].as<JsonArray>()) {
      const char *t = ex["type"] | "";
      bool light = strcmp(t, "light") == 0;
      if (!light && strcmp(t, "switch") != 0) continue;
      Control c;
      c.kind = light ? CTL_LIGHT : CTL_SWITCH;
      c.ep = ex["endpoint"] | "";
      c.key = c.ep.empty() ? dev.ieee : dev.ieee + "#" + c.ep;
      for (JsonObject ft : ex["features"].as<JsonArray>()) {
        const char *n = ft["name"] | "";
        const char *p = ft["property"] | "";
        if (!strcmp(n, "state")) {
          c.p_state = p;
          if (ft["value_on"].is<bool>()) c.bool_vals = true;
          else {
            c.v_on = ft["value_on"] | "ON";
            c.v_off = ft["value_off"] | "OFF";
            c.v_toggle = ft["value_toggle"] | "TOGGLE";
          }
        } else if (!strcmp(n, "brightness")) {
          c.p_bri = p;
          c.bri_max = ft["value_max"] | 254;
        } else if (!strcmp(n, "color_temp")) {
          c.p_ct = p;
          c.ct_min = ft["value_min"] | 153;
          c.ct_max = ft["value_max"] | 500;
        } else if (!strcmp(n, "color_xy")) {
          c.color |= COLOR_XY;
          c.p_color = p;
        } else if (!strcmp(n, "color_hs")) {
          c.color |= COLOR_HS;
          c.p_color = p;
        }
      }
      if (c.p_state.empty()) continue;
      bool dup = false;
      for (auto &o : dev.controls) dup |= o.key == c.key;
      if (!dup) dev.controls.push_back(c);
    }
    if (!dev.controls.empty()) devs.push_back(std::move(dev));
  }

  {
    Lock l;
    // Bekannte Zustaende uebernehmen, damit nichts aufblinkt
    for (auto &nd : devs) {
      for (auto &nc : nd.controls) {
        auto it = s_key_idx.find(nc.key);
        if (it == s_key_idx.end()) continue;
        const Control &oc = s_devs[it->second.first].controls[it->second.second];
        nc.known = oc.known; nc.on = oc.on; nc.bri = oc.bri; nc.ct = oc.ct;
        nc.hue = oc.hue; nc.sat = oc.sat; nc.cx = oc.cx; nc.cy = oc.cy; nc.cmode = oc.cmode;
        nd.online = s_devs[it->second.first].online;
      }
    }
    s_devs.swap(devs);
    rebuild_index_locked();
    s_dev_ver++;
    s_state_ver++;
  }
  save_cache();
  // Liste ist da: nicht weiter abonnieren (jede Aenderung in Zigbee2MQTT wuerde sie sonst komplett neu schicken)
  if (s_devices_wanted) {
    unsubscribe(s_base + "/bridge/devices");
    s_devices_wanted = false;
  }
  sync_device_topics(true);
  size_t ctl = 0;
  {
    Lock l;
    for (auto &d : s_devs) ctl += d.controls.size();
  }
  snprintf(s_diag, sizeof(s_diag), "%d Geräte empfangen, %u mit Lampe/Schalter (%u Kacheln, %u auf dem Panel)%s", total,
           (unsigned)s_devs.size(), (unsigned)ctl, (unsigned)s_placed.size(), no_def ? tr(", einige ohne Gerätedefinition") : "");
  log_i("Zigbee2MQTT: %s", s_diag);
}

// Geraete auf dem Panel aus dem Layout (Schluessel "<ieee>" bzw. "<ieee>#<endpoint>")
static bool update_placed() {
  uint32_t v = layout_version();
  if (v == s_placed_ver && s_placed_ver) return false;
  s_placed_ver = v;
  std::set<std::string> p;
  Layout lay = layout_get();
  for (auto &pg : lay.pages)
    for (auto &t : pg.tiles) p.insert(t.key.substr(0, t.key.find('#')));
  bool changed = p != s_placed;
  s_placed.swap(p);
  return changed;
}

// Abos der Zigbee-Geraete an das Layout anpassen: nur Geraete auf dem Panel (Zustand + Erreichbarkeit)
static void sync_device_topics(bool force) {
  if (!s_connected) return;
  if (!update_placed() && !force) return;
  std::set<std::string> want;
  std::vector<int> fresh;   // neu abonnierte Geraete: Zustand abfragen
  {
    Lock l;
    for (int d = 0; d < (int)s_devs.size(); d++) {
      if (!s_placed.count(s_devs[d].ieee)) continue;
      std::string st = s_base + "/" + s_devs[d].name;
      if (!s_subscribed.count(st)) fresh.push_back(d);
      want.insert(st);
      want.insert(st + "/availability");
    }
  }
  std::vector<std::string> drop;
  {
    Lock l;
    for (auto &t : s_subscribed)
      if (s_topic_idx.count(t) && !want.count(t)) drop.push_back(t);
  }
  for (auto &t : drop) unsubscribe(t);
  heap_caps_malloc_extmem_enable(16);   // Topic-Listen in den PSRAM
  subscribe_many(std::vector<std::string>(want.begin(), want.end()));
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  Lock l;
  for (int d : fresh)
    if (d < (int)s_devs.size()) queue_gets_for(s_devs[d]);
  log_i("MQTT: %u Geräte auf dem Panel abonniert (%u neu, %u abbestellt)", (unsigned)want.size() / 2,
        (unsigned)fresh.size(), (unsigned)drop.size() / 2);
}

// Geraeteliste von Zigbee2MQTT holen: Abonnieren schickt die gespeicherte (retained) Liste sofort
static void request_devices() {
  if (!s_connected) return;
  if (s_devices_wanted) unsubscribe(s_base + "/bridge/devices");   // erneut abonnieren = Liste erneut senden
  s_devices_wanted = true;
  s_got_devices = false;
  s_connected_at = millis();
  snprintf(s_diag, sizeof(s_diag), "%s", tr("Geräteliste wird geholt …"));
  subscribe(s_base + "/bridge/devices");
}

static void resubscribe_all() {
  s_subscribed.clear();
  s_devices_wanted = false;
  subscribe(s_base + "/bridge/state");
  subscribe(s_base + "/bridge/response/device/rename");
  {
    Lock l;
    rebuild_index_locked();   // Basis-Topic kann sich geaendert haben
  }
  // Ohne passenden Zwischenspeicher die Liste holen, sonst reicht er
  if (!s_cache_loaded || s_cache_base != s_base) request_devices();
  else s_got_devices = true;
  sync_device_topics(true);
  std::vector<std::string> topics;
  for (auto &t : shelly_topics()) topics.push_back(t);
  for (auto &t : wled_topics()) topics.push_back(t);
  for (auto &t : panelsat_topics()) topics.push_back(t);
  subscribe_many(topics);
  shelly_on_connect();
  wled_on_connect();
}

static bool s_want_start = false;

static void worker(void *) {
  uint32_t last_get = 0, last_probe = 0;
  for (;;) {
    bool auto_mode = settings_mqtt_auto_active();
    if (s_want_start && !g_settings.mqtt_on) {   // ausgeschaltet: gar nicht erst suchen oder verbinden
      s_want_start = false;
      snprintf(s_status, sizeof(s_status), "MQTT ausgeschaltet");
    }
    if (s_want_start) {
      if (auto_mode) {
        // Router-/Kombi-Modus: Broker (z. B. SMHUB Nano) unter den WLAN-Teilnehmern suchen
        if (net_ap_clients() == 0) {
          snprintf(s_status, sizeof(s_status), "Warte, bis sich der Hub mit dem Panel-WLAN verbindet …");
        } else if (millis() - last_probe > 6000) {
          last_probe = millis();
          snprintf(s_status, sizeof(s_status), "Suche MQTT-Broker im Panel-WLAN …");
          String h;
          if (net_find_broker(h)) {
            s_auto_host = h;
            s_want_start = false;
            start_client();
          } else {
            snprintf(s_status, sizeof(s_status), "Kein Broker im Panel-WLAN gefunden – läuft Mosquitto auf dem Hub?");
          }
        }
      } else if (net_connected()) {
        s_want_start = false;
        start_client();
      } else if (settings_mqtt_configured()) {
        snprintf(s_status, sizeof(s_status), "Warte auf WLAN …");
      }
    }
    // Kommt nach dem Verbinden keine Geraeteliste, stimmt meist das Basis-Topic nicht
    if (s_connected && !s_got_devices && s_connected_at && millis() - s_connected_at > 15000) {
      snprintf(s_diag, sizeof(s_diag), tr("Keine Geräteliste unter „%s/bridge/devices“ – Basis-Topic prüfen"), s_base.c_str());
      s_connected_at = 0;
      s_state_ver++;
    }
    // Im Auto-Modus nach laengerer Trennung neu suchen (Hub hat evtl. neue Adresse)
    if (auto_mode && s_client && !s_connected && s_disc_since && millis() - s_disc_since > 30000) {
      s_disc_since = 0;
      stop_client();
      s_want_start = true;
    }
    // Waechter fuer festen Broker: nach 60 s ohne Verbindung den Client komplett neu aufbauen
    if (!auto_mode && s_client && !s_connected && s_disc_since && millis() - s_disc_since > 60000 && net_connected()) {
      log_w("MQTT seit 60 s getrennt – Client wird neu aufgebaut (frei intern: %u)",
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
      s_disc_since = 0;
      stop_client();
      s_want_start = true;
    }
    // Messung: knapper interner RAM wird sekuendlich gemeldet (Fehlersuche bei grossen Installationen)
    static uint32_t s_heap_log = 0;
    if (millis() - s_heap_log > 1000) {
      s_heap_log = millis();
      size_t fr = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      if (fr < 20 * 1024)
        log_w("Interner RAM knapp: frei %u, größter Block %u, Tiefstwert %u", (unsigned)fr,
              (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
              (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    }
    Job j;
    if (xQueueReceive(s_jobs, &j, pdMS_TO_TICKS(50)) == pdTRUE) {
      switch (j.type) {
        case JOB_DEVICES:
          parse_devices(j.data, j.len);
          heap_caps_free(j.data);
          break;
        case JOB_RESUBSCRIBE:
          heap_caps_malloc_extmem_enable(16);   // Topic-Listen in den PSRAM (siehe parse_devices)
          resubscribe_all();
          heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
          break;
        case JOB_REFRESH: {
          Lock l;
          s_get_queue.clear();
          s_get_payload.clear();
          for (auto &d : s_devs) {
            if (!s_placed.count(d.ieee)) continue;   // nur abonnierte Geraete melden sich zurueck
            for (auto &c : d.controls) c.known = false;
            queue_gets_for(d);
          }
          shelly_refresh();
          break;
        }
        case JOB_RELOAD_DEVICES:
          request_devices();
          break;
        case JOB_RESTART:
          s_get_queue.clear();
          s_get_payload.clear();
          stop_client();
          s_want_start = true;
          break;
      }
    }
    // /get-Anfragen gedrosselt verschicken, um das Zigbee-Netz nicht zu fluten
    if (s_connected && !s_get_queue.empty() && millis() - last_get > 150) {
      last_get = millis();
      publish(s_get_queue.front(), s_get_payload.front().c_str());
      s_get_queue.erase(s_get_queue.begin());
      s_get_payload.erase(s_get_payload.begin());
    }
    // Layout geaendert (Kachel dazu/entfernt): Abos anpassen
    static uint32_t last_lay = 0;
    if (s_connected && millis() - last_lay > 1000) {
      last_lay = millis();
      if (layout_version() != s_placed_ver) sync_device_topics(false);
    }
    shelly_tick();
    wled_tick();
    // Ausstehende Befehle ohne Rueckmeldung zuruecksetzen
    static uint32_t last_pend = 0;
    if (millis() - last_pend > 500) {
      last_pend = millis();
      Lock l;
      uint32_t now = millis();
      for (auto &d : s_devs)
        for (auto &c : d.controls)
          if (c.pending_until && (int32_t)(now - c.pending_until) > 0) {
            c.pending_until = 0;
            s_state_ver++;
          }
    }
  }
}

// ---------------------------------------------------------------- oeffentliche API
void z2m_begin() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  shelly_init();
  wled_init();
  s_jobs = xQueueCreate(8, sizeof(Job));
  load_cache();
  xTaskCreatePinnedToCore(worker, "z2m", 6144, nullptr, 4, nullptr, 0);   // gemessen: max. ~2,2 KB benutzt
  z2m_restart();
}

void z2m_restart() {
  Job j{JOB_RESTART, nullptr, 0};
  xQueueSend(s_jobs, &j, pdMS_TO_TICKS(100));
}

bool z2m_connected() { return s_connected; }
const char *z2m_diag_text() { return s_diag; }
bool z2m_bridge_online() { return s_bridge_online; }
const char *z2m_status_text() { return s_status; }
uint32_t z2m_devices_version() { return s_dev_ver + shelly_devices_version() + wled_devices_version(); }
uint32_t z2m_state_version() { return s_state_ver + shelly_state_version() + wled_state_version(); }

void z2m_publish(const std::string &topic, const std::string &payload) { publish(topic, payload.c_str()); }
std::string z2m_client_id() {
  char cid[40];
  snprintf(cid, sizeof(cid), "simply-open-camper-%06llx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF));
  return cid;
}

static void fill_view(const Device &d, const Control &c, ControlView &v) {
  v.key = c.key; v.ieee = d.ieee; v.ep = c.ep; v.name = d.name;
  v.model = d.model; v.vendor = d.vendor; v.description = d.description;
  v.kind = c.kind; v.has_bri = !c.p_bri.empty(); v.has_ct = !c.p_ct.empty(); v.color = c.color;
  v.bri_max = c.bri_max; v.ct_min = c.ct_min; v.ct_max = c.ct_max;
  v.known = c.known; v.on = c.on; v.online = d.online; v.pending = c.pending_until != 0;
  v.bri = c.bri; v.ct = c.ct; v.hue = c.hue; v.sat = c.sat; v.cx = c.cx; v.cy = c.cy; v.cmode = c.cmode;
}

bool z2m_get(const std::string &key, ControlView &out) {
  if (shelly_is_key(key)) return shelly_get(key, out);
  if (wled_is_key(key)) return wled_get(key, out);
  Lock l;
  auto it = s_key_idx.find(key);
  if (it == s_key_idx.end()) return false;
  const Device &d = s_devs[it->second.first];
  fill_view(d, d.controls[it->second.second], out);
  return true;
}

std::vector<ControlView> z2m_all() {
  std::vector<ControlView> out;
  {
    Lock l;
    for (auto &d : s_devs)
      for (auto &c : d.controls) {
        ControlView v;
        fill_view(d, c, v);
        out.push_back(std::move(v));
      }
  }
  shelly_all(out);
  wled_all(out);
  std::sort(out.begin(), out.end(), [](const ControlView &a, const ControlView &b) {
    return strcasecmp(a.name.c_str(), b.name.c_str()) < 0 || (a.name == b.name && a.ep < b.ep);
  });
  return out;
}

size_t z2m_count() {
  Lock l;
  size_t n = 0;
  for (auto &d : s_devs) n += d.controls.size();
  return n + shelly_count() + wled_count();
}

// Baut ein Set-Kommando fuer ein Control und markiert es als ausstehend
template <typename F>
static void send_cmd(const std::string &key, F fill) {
  std::string topic, payload;
  {
    Lock l;
    auto it = s_key_idx.find(key);
    if (it == s_key_idx.end()) return;
    Device &d = s_devs[it->second.first];
    Control &c = d.controls[it->second.second];
    JsonDocument doc;
    fill(c, doc);
    if (doc.isNull() || doc.size() == 0) return;
    serializeJson(doc, payload);
    topic = s_base + "/" + d.name + "/set";
    c.pending_until = millis() + 3000;
    s_state_ver++;
  }
  publish(topic, payload.c_str());
}

void z2m_toggle(const std::string &key) {
  if (shelly_is_key(key)) return shelly_toggle(key);
  if (wled_is_key(key)) return wled_toggle(key);
  send_cmd(key, [](Control &c, JsonDocument &doc) {
    if (c.bool_vals) doc[c.p_state] = !c.on;
    else doc[c.p_state] = c.v_toggle;
  });
}

void z2m_set_on(const std::string &key, bool on) {
  if (shelly_is_key(key)) return shelly_set_on(key, on);
  if (wled_is_key(key)) return wled_set_on(key, on);
  send_cmd(key, [on](Control &c, JsonDocument &doc) {
    if (c.bool_vals) doc[c.p_state] = on;
    else doc[c.p_state] = on ? c.v_on : c.v_off;
  });
}

void z2m_set_brightness_pct(const std::string &key, int pct) {
  pct = constrain(pct, 1, 100);
  if (shelly_is_key(key)) return shelly_set_brightness_pct(key, pct);
  if (wled_is_key(key)) return wled_set_brightness_pct(key, pct);
  send_cmd(key, [pct](Control &c, JsonDocument &doc) {
    if (c.p_bri.empty()) return;
    int v = (pct * c.bri_max + 50) / 100;
    if (v < 1) v = 1;
    if (c.bool_vals) doc[c.p_state] = true;
    else doc[c.p_state] = c.v_on;
    doc[c.p_bri] = v;
  });
}

void z2m_set_ct(const std::string &key, int mired) {
  if (shelly_is_key(key)) return shelly_set_ct(key, mired);
  if (wled_is_key(key)) return;   // WLED: keine Farbtemperatur ueber MQTT
  send_cmd(key, [mired](Control &c, JsonDocument &doc) {
    if (c.p_ct.empty()) return;
    if (c.bool_vals) doc[c.p_state] = true;
    else doc[c.p_state] = c.v_on;
    doc[c.p_ct] = constrain(mired, c.ct_min, c.ct_max);
  });
}

static void hsv_to_hex(float h, float s, char *out) {
  float S = s / 100.f, V = 1.f, C = V * S;
  float X = C * (1 - fabsf(fmodf(h / 60.f, 2) - 1)), m = V - C;
  float r = 0, g = 0, b = 0;
  if (h < 60) { r = C; g = X; }
  else if (h < 120) { r = X; g = C; }
  else if (h < 180) { g = C; b = X; }
  else if (h < 240) { g = X; b = C; }
  else if (h < 300) { r = X; b = C; }
  else { r = C; b = X; }
  snprintf(out, 8, "#%02X%02X%02X", (int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
}

void z2m_set_color_hs(const std::string &key, float hue, float sat) {
  if (shelly_is_key(key)) return shelly_set_color_hs(key, hue, sat);
  if (wled_is_key(key)) return wled_set_color_hs(key, hue, sat);
  send_cmd(key, [hue, sat](Control &c, JsonDocument &doc) {
    if (!c.color) return;
    if (c.bool_vals) doc[c.p_state] = true;
    else doc[c.p_state] = c.v_on;
    JsonObject col = doc[c.p_color].to<JsonObject>();
    if (c.color & COLOR_HS) {
      col["hue"] = (int)roundf(hue);
      col["saturation"] = (int)roundf(sat);
    } else {
      char hex[8];
      hsv_to_hex(hue, sat, hex);
      col["hex"] = hex;
    }
  });
}

std::string z2m_rename(const std::string &ieee, const std::string &new_name) {
  if (!s_connected) return "Keine Verbindung zu MQTT";
  std::string n = new_name;
  while (!n.empty() && n.back() == ' ') n.pop_back();
  while (!n.empty() && n.front() == ' ') n.erase(0, 1);
  if (n.empty()) return "Name darf nicht leer sein";
  if (n.find_first_of("#+") != std::string::npos) return "Die Zeichen # und + sind nicht erlaubt";
  size_t slash = n.rfind('/');
  if (slash != std::string::npos && slash + 1 < n.size() &&
      n.find_first_not_of("0123456789", slash + 1) == std::string::npos)
    return "Name darf nicht auf /Zahl enden";
  std::string old;
  {
    Lock l;
    for (auto &d : s_devs)
      if (d.ieee == ieee) old = d.name;
  }
  if (old.empty()) return "Gerät nicht gefunden";
  if (old == n) return "";
  JsonDocument doc;
  doc["from"] = old;
  doc["to"] = n;
  doc["homeassistant_rename"] = true;
  std::string p;
  serializeJson(doc, p);
  publish(s_base + "/bridge/request/device/rename", p.c_str());
  return "";
}

void z2m_refresh_states() {
  Job j{JOB_REFRESH, nullptr, 0};
  xQueueSend(s_jobs, &j, 0);
}

void z2m_reload_devices() {
  Job j{JOB_RELOAD_DEVICES, nullptr, 0};
  xQueueSend(s_jobs, &j, 0);
}
