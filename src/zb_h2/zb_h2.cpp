// Simple Open Camper – Zigbee-Funkmodul (ESP32-H2 im Steckplatz des CrowPanel-P4)
//
// Grundgeruest: Zigbee-Koordinator (Espressif-Zigbee-Stack), Anlernen auf Befehl, Abfrage neuer Geraete
// (Endpunkte, Cluster, Hersteller/Modell), Binden + Melden fuer Standard-Cluster, Weitergabe aller Attribut-
// Meldungen und Befehle an Geraete. Verbindung zum Panel: JSON-Zeilen ueber UART, Protokoll siehe src/zb_link.h.
// Dieselben Befehle gehen auch ueber die USB-Buchse des Moduls (zum Testen, z. B. {"c":"info"}).
//
// Spaeter (Plan 2026-11-17): Treiber im Modul (Umrechnung wie Zigbee2MQTT, Ausgabe im Zigbee2MQTT-Format).
#include <sdkconfig.h>   // CONFIG_IDF_TARGET_* – nur im H2-Build wird diese Datei uebersetzt (sonst leer)
#if defined(CONFIG_IDF_TARGET_ESP32H2) && defined(ZIGBEE_MODE_ZCZR)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "esp_zigbee_core.h"
#include "../soc_version.h"
#include "../zb_link.h"

#ifndef ZB_H2_TX
#define ZB_H2_TX 24   // Modul "U1TXD" -> P4 GPIO 28 (vermutet, siehe zb_link.h)
#endif
#ifndef ZB_H2_RX
#define ZB_H2_RX 23   // Modul "U1RXD" <- P4 GPIO 27
#endif
#ifndef ZB_H2_LED
#define ZB_H2_LED 25   // blaue LED "USER" auf dem Modul
#endif

namespace {

constexpr uint8_t COORD_EP = 1;
HardwareSerial &Link = Serial1;
SemaphoreHandle_t s_out_mx = nullptr;

volatile bool s_ready = false;
volatile uint8_t s_open = 0;     // Anlernen offen (Sekunden laut Stack)
uint32_t s_open_until = 0;       // millis()

// ---------------------------------------------------------------- Geraete
struct Ep {
  uint8_t ep = 0;
  uint16_t prof = 0, dev = 0;
  std::vector<uint16_t> in, out;
};
struct Dev {
  uint8_t ieee[8] = {0};
  uint16_t nwk = 0xFFFF;
  std::string mf, model;
  std::vector<Ep> eps;
  bool done = false;
};
std::vector<Dev> s_devs;   // nur im Stack-Task oder unter esp_zb_lock

std::string ieee_str(const uint8_t *ieee) {   // "0x00124b0012345678" (wie Zigbee2MQTT, hoechstes Byte zuerst)
  char b[19];
  snprintf(b, sizeof(b), "0x%02x%02x%02x%02x%02x%02x%02x%02x", ieee[7], ieee[6], ieee[5], ieee[4], ieee[3], ieee[2],
           ieee[1], ieee[0]);
  return b;
}
bool ieee_parse(const char *s, uint8_t *out) {
  if (!s) return false;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
  if (strlen(s) != 16) return false;
  for (int i = 0; i < 8; i++) {
    char t[3] = {s[i * 2], s[i * 2 + 1], 0};
    char *e;
    long v = strtol(t, &e, 16);
    if (*e) return false;
    out[7 - i] = (uint8_t)v;
  }
  return true;
}
Dev *find_ieee(const uint8_t *ieee) {
  for (auto &d : s_devs)
    if (!memcmp(d.ieee, ieee, 8)) return &d;
  return nullptr;
}
Dev *find_nwk(uint16_t nwk) {
  for (auto &d : s_devs)
    if (d.nwk == nwk) return &d;
  return nullptr;
}

// ---------------------------------------------------------------- Ausgabe (UART zum Panel + USB)
void send(JsonDocument &d) {
  String s;
  serializeJson(d, s);
  s += '\n';
  if (s_out_mx) xSemaphoreTake(s_out_mx, portMAX_DELAY);
  Link.print(s);
  Serial.print(s);
  if (s_out_mx) xSemaphoreGive(s_out_mx);
}
void send_msg(const char *t, const char *fmt, ...) {
  char m[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m, sizeof(m), fmt, ap);
  va_end(ap);
  JsonDocument d;
  d["t"] = t;
  d["m"] = m;
  send(d);
}
#define LOGZ(...) send_msg("log", __VA_ARGS__)

void dev_json(JsonObject o, const Dev &d) {
  o["ieee"] = ieee_str(d.ieee);
  o["nwk"] = d.nwk;
  o["mf"] = d.mf;
  o["model"] = d.model;
  o["done"] = d.done;
  JsonArray eps = o["eps"].to<JsonArray>();
  for (auto &e : d.eps) {
    JsonObject x = eps.add<JsonObject>();
    x["ep"] = e.ep;
    x["prof"] = e.prof;
    x["dev"] = e.dev;
    JsonArray in = x["in"].to<JsonArray>();
    for (uint16_t c : e.in) in.add(c);
    JsonArray out = x["out"].to<JsonArray>();
    for (uint16_t c : e.out) out.add(c);
  }
}
void send_dev(const Dev &d) {
  JsonDocument j;
  j["t"] = "dev";
  dev_json(j.to<JsonObject>(), d);
  j["t"] = "dev";
  send(j);
}

// ---------------------------------------------------------------- Speichern (NVS "zbh2")
void save_devs() {
  Preferences p;
  if (!p.begin("zbh2", false)) return;
  p.clear();
  p.putUChar("n", (uint8_t)s_devs.size());
  for (size_t i = 0; i < s_devs.size(); i++) {
    JsonDocument j;
    dev_json(j.to<JsonObject>(), s_devs[i]);
    String s;
    serializeJson(j, s);
    char k[6];
    snprintf(k, sizeof(k), "d%u", (unsigned)i);
    p.putString(k, s);
  }
  p.end();
}
void load_devs() {
  Preferences p;
  if (!p.begin("zbh2", true)) return;
  uint8_t n = p.getUChar("n", 0);
  for (uint8_t i = 0; i < n && i < ZB_MAX_DEVICES; i++) {
    char k[6];
    snprintf(k, sizeof(k), "d%u", (unsigned)i);
    JsonDocument j;
    if (deserializeJson(j, p.getString(k, ""))) continue;
    Dev d;
    if (!ieee_parse(j["ieee"] | "", d.ieee)) continue;
    d.nwk = j["nwk"] | 0xFFFF;
    d.mf = (const char *)(j["mf"] | "");
    d.model = (const char *)(j["model"] | "");
    d.done = j["done"] | false;
    for (JsonObject x : j["eps"].as<JsonArray>()) {
      Ep e;
      e.ep = x["ep"] | 0;
      e.prof = x["prof"] | 0;
      e.dev = x["dev"] | 0;
      for (uint16_t c : x["in"].as<JsonArray>()) e.in.push_back(c);
      for (uint16_t c : x["out"].as<JsonArray>()) e.out.push_back(c);
      d.eps.push_back(e);
    }
    s_devs.push_back(d);
  }
  p.end();
}

// ---------------------------------------------------------------- Attributwerte -> JSON
void put_value(JsonObject o, const esp_zb_zcl_attribute_t &a) {
  uint8_t type = (uint8_t)a.data.type;
  o["type"] = type;
  const uint8_t *p = (const uint8_t *)a.data.value;
  if (!p) {
    o["v"] = nullptr;
    return;
  }
  auto le = [&](int n) {
    uint32_t v = 0;
    for (int i = n - 1; i >= 0; i--) v = (v << 8) | p[i];
    return v;
  };
  auto hex = [&](const uint8_t *q, int n) {
    std::string s;
    char b[3];
    for (int i = 0; i < n; i++) {
      snprintf(b, sizeof(b), "%02x", q[i]);
      s += b;
    }
    return s;
  };
  switch (type) {
    case 0x10: o["v"] = p[0] != 0; break;                                    // bool
    case 0x08: case 0x18: case 0x20: case 0x30: o["v"] = p[0]; break;        // 8 Bit, Bitmap, uint8, enum8
    case 0x09: case 0x19: case 0x21: case 0x31: o["v"] = le(2); break;       // 16 Bit, Bitmap, uint16, enum16
    case 0x22: o["v"] = le(3); break;                                         // uint24
    case 0x0b: case 0x1b: case 0x23: o["v"] = le(4); break;                   // 32 Bit, Bitmap, uint32
    case 0x28: o["v"] = (int8_t)p[0]; break;                                  // int8
    case 0x29: o["v"] = (int16_t)le(2); break;                                // int16
    case 0x2a: { int32_t v = le(3); if (v & 0x800000) v |= 0xFF000000; o["v"] = v; } break;   // int24
    case 0x2b: o["v"] = (int32_t)le(4); break;                                // int32
    case 0x39: { float f; memcpy(&f, p, 4); o["v"] = f; } break;              // float
    case 0x42: o["v"] = std::string((const char *)p + 1, p[0]); break;        // Text (Laenge vorn)
    case 0x41: o["v"] = hex(p + 1, p[0]); break;                              // Bytefolge
    default: o["v"] = hex(p, a.data.size); break;
  }
}
void send_attr(uint16_t nwk, uint8_t ep, uint16_t cl, const esp_zb_zcl_attribute_t &a) {
  JsonDocument j;
  j["t"] = "attr";
  Dev *d = find_nwk(nwk);
  uint8_t ieee[8];
  if (d) j["ieee"] = ieee_str(d->ieee);
  else if (esp_zb_ieee_address_by_short(nwk, ieee) == ESP_OK) j["ieee"] = ieee_str(ieee);
  j["nwk"] = nwk;
  j["ep"] = ep;
  j["cl"] = cl;
  j["at"] = a.id;
  put_value(j.as<JsonObject>(), a);
  send(j);
  // Hersteller/Modell aus der Abfrage (Basic-Cluster) gleich merken
  if (d && cl == 0x0000 && (a.id == 0x0004 || a.id == 0x0005) && a.data.value && (uint8_t)a.data.type == 0x42) {
    const uint8_t *p = (const uint8_t *)a.data.value;
    (a.id == 0x0004 ? d->mf : d->model) = std::string((const char *)p + 1, p[0]);
  }
}

// ---------------------------------------------------------------- Abfrage neuer Geraete (Interview)
struct Job {
  uint8_t ieee[8];
  uint16_t nwk;
};
std::deque<Job> s_jobs;
bool s_busy = false;
uint8_t s_gen = 0;   // Zaehler, damit verspaetete Zeitgeber alte Abfragen nicht beenden
Job s_cur;
std::vector<uint8_t> s_cur_eps;
size_t s_cur_i = 0;

void start_next();
void finish_job(uint8_t gen);

void bind_and_report(const Dev &d) {
  // Standard-Cluster an den Koordinator binden und Meldungen einrichten (Treiber koennen das spaeter verfeinern)
  struct Rep {
    uint16_t cl, at;
    uint8_t type;
    uint16_t min, max;
    uint32_t change;
  };
  static const Rep REPS[] = {
      {0x0006, 0x0000, 0x10, 0, 600, 0},       // Ein/Aus
      {0x0008, 0x0000, 0x20, 1, 600, 1},       // Helligkeit
      {0x0402, 0x0000, 0x29, 10, 600, 10},     // Temperatur 0,01 °C
      {0x0405, 0x0000, 0x21, 10, 600, 50},     // Feuchte 0,01 %
      {0x0001, 0x0021, 0x20, 3600, 21600, 2},  // Batterie 0,5 %
  };
  esp_zb_ieee_addr_t me;
  esp_zb_get_long_address(me);
  for (auto &e : d.eps) {
    for (auto &r : REPS) {
      if (std::find(e.in.begin(), e.in.end(), r.cl) == e.in.end()) continue;
      esp_zb_zdo_bind_req_param_t b = {};
      memcpy(b.src_address, d.ieee, 8);
      b.src_endp = e.ep;
      b.cluster_id = r.cl;
      b.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
      memcpy(b.dst_address_u.addr_long, me, 8);
      b.dst_endp = COORD_EP;
      b.req_dst_addr = d.nwk;
      esp_zb_zdo_device_bind_req(&b, [](esp_zb_zdp_status_t, void *) {}, nullptr);

      uint32_t change = r.change;
      esp_zb_zcl_config_report_record_t rec = {};
      rec.direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND;
      rec.attributeID = r.at;
      rec.attrType = r.type;
      rec.min_interval = r.min;
      rec.max_interval = r.max;
      rec.reportable_change = &change;
      esp_zb_zcl_config_report_cmd_t c = {};
      c.zcl_basic_cmd.dst_addr_u.addr_short = d.nwk;
      c.zcl_basic_cmd.dst_endpoint = e.ep;
      c.zcl_basic_cmd.src_endpoint = COORD_EP;
      c.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      c.clusterID = r.cl;
      c.record_number = 1;
      c.record_field = &rec;
      esp_zb_zcl_config_report_cmd_req(&c);
    }
  }
}

void finish_alarm(uint8_t gen) { finish_job(gen); }

void finish_job(uint8_t gen) {
  if (!s_busy || gen != s_gen) return;
  Dev *d = find_ieee(s_cur.ieee);
  if (d) {
    d->done = !d->eps.empty();
    if (d->done) bind_and_report(*d);
    save_devs();
    send_dev(*d);
    LOGZ("Abfrage %s fertig: %u Endpunkt(e), %s %s", ieee_str(d->ieee).c_str(), (unsigned)d->eps.size(), d->mf.c_str(),
         d->model.c_str());
  }
  s_busy = false;
  start_next();
}

void read_basic() {
  Dev *d = find_ieee(s_cur.ieee);
  if (!d || d->eps.empty()) return finish_job(s_gen);
  uint8_t ep = d->eps[0].ep;
  for (auto &e : d->eps)
    if (std::find(e.in.begin(), e.in.end(), 0x0000) != e.in.end()) {
      ep = e.ep;
      break;
    }
  static uint16_t ATTRS[] = {0x0004, 0x0005};   // Hersteller, Modell
  esp_zb_zcl_read_attr_cmd_t r = {};
  r.zcl_basic_cmd.dst_addr_u.addr_short = s_cur.nwk;
  r.zcl_basic_cmd.dst_endpoint = ep;
  r.zcl_basic_cmd.src_endpoint = COORD_EP;
  r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  r.clusterID = 0x0000;
  r.attr_number = 2;
  r.attr_field = ATTRS;
  esp_zb_zcl_read_attr_cmd_req(&r);
  esp_zb_scheduler_alarm(finish_alarm, s_gen, 6000);   // Antwort abwarten (oder ohne Antwort weiter)
}

void next_desc() {
  if (s_cur_i >= s_cur_eps.size()) return read_basic();
  esp_zb_zdo_simple_desc_req_param_t p = {};
  p.addr_of_interest = s_cur.nwk;
  p.endpoint = s_cur_eps[s_cur_i];
  uint8_t gen = s_gen;
  esp_zb_zdo_simple_desc_req(
      &p,
      [](esp_zb_zdp_status_t st, esp_zb_af_simple_desc_1_1_t *sd, void *ctx) {
        if ((uint8_t)(uintptr_t)ctx != s_gen || !s_busy) return;
        Dev *d = find_ieee(s_cur.ieee);
        if (st == ESP_ZB_ZDP_STATUS_SUCCESS && sd && d) {
          Ep e;
          e.ep = sd->endpoint;
          e.prof = sd->app_profile_id;
          e.dev = sd->app_device_id;
          for (int i = 0; i < sd->app_input_cluster_count; i++) e.in.push_back(sd->app_cluster_list[i]);
          for (int i = 0; i < sd->app_output_cluster_count; i++)
            e.out.push_back(sd->app_cluster_list[sd->app_input_cluster_count + i]);
          d->eps.erase(std::remove_if(d->eps.begin(), d->eps.end(), [&](const Ep &x) { return x.ep == e.ep; }),
                       d->eps.end());
          d->eps.push_back(e);
        } else {
          LOGZ("Endpunkt %u: keine Beschreibung (Status %d)", s_cur_eps[s_cur_i], (int)st);
        }
        s_cur_i++;
        next_desc();
      },
      (void *)(uintptr_t)gen);
}

void start_next() {
  if (s_busy || s_jobs.empty()) return;
  s_cur = s_jobs.front();
  s_jobs.pop_front();
  s_busy = true;
  s_gen++;
  s_cur_eps.clear();
  s_cur_i = 0;
  esp_zb_zdo_active_ep_req_param_t p = {};
  p.addr_of_interest = s_cur.nwk;
  esp_zb_zdo_active_ep_req(
      &p,
      [](esp_zb_zdp_status_t st, uint8_t n, uint8_t *list, void *ctx) {
        if ((uint8_t)(uintptr_t)ctx != s_gen || !s_busy) return;
        if (st != ESP_ZB_ZDP_STATUS_SUCCESS) {
          LOGZ("Endpunkte nicht abfragbar (Status %d) – Geraet schlaeft? Taste am Geraet druecken.", (int)st);
          return finish_job(s_gen);
        }
        s_cur_eps.assign(list, list + n);
        next_desc();
      },
      (void *)(uintptr_t)s_gen);
}

void enqueue(const uint8_t *ieee, uint16_t nwk) {
  for (auto &j : s_jobs)
    if (!memcmp(j.ieee, ieee, 8)) {
      j.nwk = nwk;
      return;
    }
  Job j;
  memcpy(j.ieee, ieee, 8);
  j.nwk = nwk;
  s_jobs.push_back(j);
  start_next();
}

// ---------------------------------------------------------------- Stack-Ereignisse
void commissioning_cb(uint8_t mode) { esp_zb_bdb_start_top_level_commissioning(mode); }

void on_announce(uint16_t nwk, const uint8_t *ieee) {
  Dev *d = find_ieee(ieee);
  if (!d) {
    if (s_devs.size() >= ZB_MAX_DEVICES) {
      send_msg("err", "Geraeteliste voll (%d)", ZB_MAX_DEVICES);
      return;
    }
    Dev n;
    memcpy(n.ieee, ieee, 8);
    s_devs.push_back(n);
    d = &s_devs.back();
  }
  bool moved = d->nwk != nwk;
  d->nwk = nwk;
  JsonDocument j;
  j["t"] = "joined";
  j["ieee"] = ieee_str(ieee);
  j["nwk"] = nwk;
  j["known"] = d->done;
  send(j);
  if (!d->done) enqueue(ieee, nwk);
  else if (moved) save_devs();
}

esp_err_t action_cb(esp_zb_core_action_callback_id_t id, const void *msg) {
  switch (id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID: {
      auto *m = (const esp_zb_zcl_report_attr_message_t *)msg;
      send_attr(m->src_address.u.short_addr, m->src_endpoint, m->cluster, m->attribute);
      break;
    }
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID: {
      auto *m = (const esp_zb_zcl_cmd_read_attr_resp_message_t *)msg;
      bool basic = false;
      for (auto *v = m->variables; v; v = v->next) {
        if (v->status != ESP_ZB_ZCL_STATUS_SUCCESS) continue;
        send_attr(m->info.src_address.u.short_addr, m->info.src_endpoint, m->info.cluster, v->attribute);
        if (m->info.cluster == 0x0000 && (v->attribute.id == 0x0004 || v->attribute.id == 0x0005)) basic = true;
      }
      // Antwort auf die Hersteller/Modell-Abfrage: Interview gleich abschliessen
      if (basic && s_busy && m->info.src_address.u.short_addr == s_cur.nwk) finish_job(s_gen);
      break;
    }
    default:
      break;
  }
  return ESP_OK;
}

void form_network() {
  esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
}

}  // namespace

// Signale des Zigbee-Stacks (Name ist vom Stack vorgegeben)
extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *s) {
  uint32_t *p = s->p_app_signal;
  esp_err_t err = s->esp_err_status;
  esp_zb_app_signal_type_t type = (esp_zb_app_signal_type_t)*p;
  switch (type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err != ESP_OK) {
        send_msg("err", "Zigbee-Start fehlgeschlagen (%s), neuer Versuch", esp_err_to_name(err));
        esp_zb_scheduler_alarm(commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
      } else if (esp_zb_bdb_is_factory_new()) {
        LOGZ("Neues Zigbee-Netz wird gebildet ...");
        form_network();
      } else {
        s_ready = true;
        LOGZ("Zigbee-Netz bereit: PAN 0x%04x, Kanal %d", esp_zb_get_pan_id(), esp_zb_get_current_channel());
      }
      break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
      if (err == ESP_OK) {
        s_ready = true;
        LOGZ("Zigbee-Netz gebildet: PAN 0x%04x, Kanal %d", esp_zb_get_pan_id(), esp_zb_get_current_channel());
      } else {
        send_msg("err", "Netzbildung fehlgeschlagen (%s), neuer Versuch", esp_err_to_name(err));
        esp_zb_scheduler_alarm(commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
      auto *a = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p);
      on_announce(a->device_short_addr, a->ieee_addr);
      break;
    }
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION: {
      auto *l = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p);
      if (!l->rejoin) {
        JsonDocument j;
        j["t"] = "left";
        j["ieee"] = ieee_str(l->device_addr);
        send(j);
        s_devs.erase(std::remove_if(s_devs.begin(), s_devs.end(),
                                    [&](const Dev &d) { return !memcmp(d.ieee, l->device_addr, 8); }),
                     s_devs.end());
        save_devs();
      }
      break;
    }
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: {
      uint8_t sec = *(uint8_t *)esp_zb_app_signal_get_params(p);
      s_open = sec;
      s_open_until = millis() + sec * 1000UL;
      JsonDocument j;
      j["t"] = "open";
      j["s"] = sec;
      send(j);
      break;
    }
    default:
      break;
  }
}

namespace {

// ---------------------------------------------------------------- Befehle vom Panel (bzw. USB)
bool target(JsonDocument &c, uint16_t &nwk, uint8_t &ep) {
  uint8_t ieee[8];
  if (!ieee_parse(c["ieee"] | "", ieee)) {
    send_msg("err", "ieee fehlt oder ungueltig");
    return false;
  }
  Dev *d = find_ieee(ieee);
  nwk = d ? d->nwk : esp_zb_address_short_by_ieee(ieee);
  if (nwk == 0xFFFF || nwk == 0xFFFE) {
    send_msg("err", "Geraet %s unbekannt", (const char *)(c["ieee"] | ""));
    return false;
  }
  ep = c["ep"] | (d && !d->eps.empty() ? d->eps[0].ep : 1);
  return true;
}

void send_info() {
  JsonDocument j;
  j["t"] = "info";
  j["fw"] = SOC_VERSION;
  j["ready"] = (bool)s_ready;
  if (s_ready) {
    j["pan"] = esp_zb_get_pan_id();
    j["ch"] = esp_zb_get_current_channel();
    esp_zb_ieee_addr_t me;
    esp_zb_get_long_address(me);
    j["ieee"] = ieee_str(me);
  }
  j["open"] = (s_open && (int32_t)(s_open_until - millis()) > 0) ? (int)((s_open_until - millis()) / 1000) : 0;
  j["devs"] = s_devs.size();
  j["heap"] = ESP.getFreeHeap();         // freier RAM (Byte)
  j["heap_min"] = ESP.getMinFreeHeap();  // Tiefstwert seit dem Start
  j["heap_blk"] = ESP.getMaxAllocHeap(); // groesster zusammenhaengender Block
  send(j);
}

void handle(const char *line) {
  JsonDocument c;
  if (deserializeJson(c, line)) {
    send_msg("err", "kein JSON: %.60s", line);
    return;
  }
  const char *cmd = c["c"] | "";
  if (!esp_zb_lock_acquire(pdMS_TO_TICKS(2000))) {
    send_msg("err", "Zigbee-Stack beschaeftigt");
    return;
  }
  if (!strcmp(cmd, "info")) {
    send_info();
  } else if (!strcmp(cmd, "permit")) {
    int s = c["s"] | 180;
    if (!s_ready) send_msg("err", "Netz noch nicht bereit");
    else if (s <= 0) esp_zb_bdb_close_network();
    else esp_zb_bdb_open_network((uint8_t)min(s, 254));
  } else if (!strcmp(cmd, "devices")) {
    for (auto &d : s_devs) send_dev(d);
    JsonDocument j;
    j["t"] = "devs_end";
    j["n"] = s_devs.size();
    send(j);
  } else if (!strcmp(cmd, "remove")) {
    uint8_t ieee[8];
    if (ieee_parse(c["ieee"] | "", ieee)) {
      Dev *d = find_ieee(ieee);
      esp_zb_zdo_mgmt_leave_req_param_t l = {};
      memcpy(l.device_address, ieee, 8);
      l.dst_nwk_addr = d ? d->nwk : esp_zb_address_short_by_ieee(ieee);
      l.rejoin = 0;
      l.remove_children = 0;
      esp_zb_zdo_device_leave_req(&l, [](esp_zb_zdp_status_t, void *) {}, nullptr);
      s_devs.erase(std::remove_if(s_devs.begin(), s_devs.end(), [&](const Dev &x) { return !memcmp(x.ieee, ieee, 8); }),
                   s_devs.end());
      save_devs();
      JsonDocument j;
      j["t"] = "left";
      j["ieee"] = ieee_str(ieee);
      send(j);
    }
  } else if (!strcmp(cmd, "read")) {
    uint16_t nwk;
    uint8_t ep;
    if (target(c, nwk, ep)) {
      static uint16_t at[16];
      uint8_t n = 0;
      for (uint16_t a : c["at"].as<JsonArray>())
        if (n < 16) at[n++] = a;
      esp_zb_zcl_read_attr_cmd_t r = {};
      r.zcl_basic_cmd.dst_addr_u.addr_short = nwk;
      r.zcl_basic_cmd.dst_endpoint = ep;
      r.zcl_basic_cmd.src_endpoint = COORD_EP;
      r.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      r.clusterID = c["cl"] | 0;
      uint16_t mf = c["mf"] | 0;
      if (mf) {
        r.manuf_specific = 1;
        r.manuf_code = mf;
      }
      r.attr_number = n;
      r.attr_field = at;
      if (n) esp_zb_zcl_read_attr_cmd_req(&r);
    }
  } else if (!strcmp(cmd, "onoff")) {
    uint16_t nwk;
    uint8_t ep;
    if (target(c, nwk, ep)) {
      esp_zb_zcl_on_off_cmd_t o = {};
      o.zcl_basic_cmd.dst_addr_u.addr_short = nwk;
      o.zcl_basic_cmd.dst_endpoint = ep;
      o.zcl_basic_cmd.src_endpoint = COORD_EP;
      o.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      int v = c["v"] | 2;
      o.on_off_cmd_id = v == 0 ? 0x00 : v == 1 ? 0x01 : 0x02;   // Aus / Ein / Umschalten
      esp_zb_zcl_on_off_cmd_req(&o);
    }
  } else if (!strcmp(cmd, "level")) {
    uint16_t nwk;
    uint8_t ep;
    if (target(c, nwk, ep)) {
      esp_zb_zcl_move_to_level_cmd_t m = {};
      m.zcl_basic_cmd.dst_addr_u.addr_short = nwk;
      m.zcl_basic_cmd.dst_endpoint = ep;
      m.zcl_basic_cmd.src_endpoint = COORD_EP;
      m.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      m.level = (uint8_t)constrain((int)(c["v"] | 254), 0, 254);
      m.transition_time = c["tt"] | 5;
      esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&m);
    }
  } else if (!strcmp(cmd, "cmd")) {
    uint16_t nwk;
    uint8_t ep;
    if (target(c, nwk, ep)) {
      static uint8_t pl[96];
      const char *hex = c["pl"] | "";
      size_t n = 0;
      for (; hex[0] && hex[1] && n < sizeof(pl); hex += 2) {
        char t[3] = {hex[0], hex[1], 0};
        pl[n++] = (uint8_t)strtol(t, nullptr, 16);
      }
      esp_zb_zcl_custom_cluster_cmd_t x = {};
      x.zcl_basic_cmd.dst_addr_u.addr_short = nwk;
      x.zcl_basic_cmd.dst_endpoint = ep;
      x.zcl_basic_cmd.src_endpoint = COORD_EP;
      x.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      x.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
      x.cluster_id = c["cl"] | 0;
      uint16_t mf = c["mf"] | 0;
      if (mf) {
        x.manuf_specific = 1;
        x.manuf_code = mf;
      }
      x.direction = (c["dir"] | 0) ? ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI : ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
      x.custom_cmd_id = c["id"] | 0;
      x.data.type = ESP_ZB_ZCL_ATTR_TYPE_SET;   // Rohdaten unveraendert senden
      x.data.size = n;
      x.data.value = n ? pl : nullptr;
      esp_zb_zcl_custom_cluster_cmd_req(&x);
    }
  } else if (!strcmp(cmd, "reset_net")) {
    LOGZ("Zigbee-Netz wird zurueckgesetzt – alle Geraete muessen neu angelernt werden");
    s_devs.clear();
    save_devs();
    esp_zb_lock_release();
    delay(200);
    esp_zb_factory_reset();   // loescht das Netz und startet neu
    return;
  } else {
    send_msg("err", "unbekannter Befehl \"%s\"", cmd);
  }
  esp_zb_lock_release();
}

// Zeilen von UART (Panel) und USB (Test) sammeln
struct LineBuf {
  char b[ZB_LINK_MAX_LINE];
  size_t n = 0;
  void feed(int ch) {
    if (ch == '\n' || ch == '\r') {
      if (n) {
        b[n] = 0;
        n = 0;
        if (b[0] == '{') handle(b);
      }
    } else if (n < sizeof(b) - 1) {
      b[n++] = (char)ch;
    }
  }
};
LineBuf s_link_in, s_usb_in;

void zb_task(void *) {
  esp_zb_platform_config_t pc = {};
  pc.radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;
  pc.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
  ESP_ERROR_CHECK(esp_zb_platform_config(&pc));

  esp_zb_cfg_t cfg = {};
  cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR;
  cfg.install_code_policy = false;
  cfg.nwk_cfg.zczr_cfg.max_children = ZB_MAX_DEVICES;
  esp_zb_init(&cfg);

  // Eigener Endpunkt: Basic als Server, die ueblichen Cluster als Client (dort kommen Meldungen an)
  esp_zb_basic_cluster_cfg_t bc = {};
  bc.zcl_version = 3;
  bc.power_source = 1;   // Netzbetrieb
  esp_zb_cluster_list_t *cl = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cl, esp_zb_basic_cluster_create(&bc), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  const uint8_t CLI = ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE;
  esp_zb_cluster_list_add_identify_cluster(cl, esp_zb_zcl_attr_list_create(0x0003), CLI);
  esp_zb_cluster_list_add_power_config_cluster(cl, esp_zb_zcl_attr_list_create(0x0001), CLI);
  esp_zb_cluster_list_add_on_off_cluster(cl, esp_zb_zcl_attr_list_create(0x0006), CLI);
  esp_zb_cluster_list_add_level_cluster(cl, esp_zb_zcl_attr_list_create(0x0008), CLI);
  esp_zb_cluster_list_add_color_control_cluster(cl, esp_zb_zcl_attr_list_create(0x0300), CLI);
  esp_zb_cluster_list_add_temperature_meas_cluster(cl, esp_zb_zcl_attr_list_create(0x0402), CLI);
  esp_zb_cluster_list_add_pressure_meas_cluster(cl, esp_zb_zcl_attr_list_create(0x0403), CLI);
  esp_zb_cluster_list_add_humidity_meas_cluster(cl, esp_zb_zcl_attr_list_create(0x0405), CLI);
  esp_zb_cluster_list_add_illuminance_meas_cluster(cl, esp_zb_zcl_attr_list_create(0x0400), CLI);
  esp_zb_cluster_list_add_occupancy_sensing_cluster(cl, esp_zb_zcl_attr_list_create(0x0406), CLI);
  esp_zb_cluster_list_add_ias_zone_cluster(cl, esp_zb_zcl_attr_list_create(0x0500), CLI);
  esp_zb_cluster_list_add_metering_cluster(cl, esp_zb_zcl_attr_list_create(0x0702), CLI);
  esp_zb_cluster_list_add_electrical_meas_cluster(cl, esp_zb_zcl_attr_list_create(0x0B04), CLI);
  esp_zb_cluster_list_add_custom_cluster(cl, esp_zb_zcl_attr_list_create(0xEF00), CLI);   // Tuya
  esp_zb_ep_list_t *eps = esp_zb_ep_list_create();
  esp_zb_endpoint_config_t ec = {};
  ec.endpoint = COORD_EP;
  ec.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  ec.app_device_id = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID;
  ec.app_device_version = 0;
  esp_zb_ep_list_add_ep(eps, cl, ec);
  esp_zb_device_register(eps);
  esp_zb_core_action_handler_register(action_cb);
  esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_stack_main_loop();   // kehrt nicht zurueck
}

}  // namespace

void setup() {
  Serial.begin(115200);   // USB des Moduls (Test, Log)
  Link.setRxBufferSize(ZB_LINK_MAX_LINE * 2);
  Link.begin(ZB_LINK_BAUD, SERIAL_8N1, ZB_H2_RX, ZB_H2_TX);
  s_out_mx = xSemaphoreCreateMutex();
  pinMode(ZB_H2_LED, OUTPUT);
  digitalWrite(ZB_H2_LED, LOW);
  load_devs();
  JsonDocument j;
  j["t"] = "hello";
  j["fw"] = SOC_VERSION;
  j["tx"] = ZB_H2_TX;
  j["rx"] = ZB_H2_RX;
  send(j);
  xTaskCreate(zb_task, "zigbee", 8192, nullptr, 5, nullptr);
}

void loop() {
  while (Link.available()) s_link_in.feed(Link.read());
  while (Serial.available()) s_usb_in.feed(Serial.read());
  // LED: blinkt, solange das Anlernen offen ist; leuchtet, wenn das Netz bereit ist
  bool open = s_open && (int32_t)(s_open_until - millis()) > 0;
  digitalWrite(ZB_H2_LED, open ? ((millis() / 250) & 1) : s_ready ? HIGH : LOW);
  // Lebenszeichen alle 30 s, damit das Panel ein neu gestartetes Modul bemerkt
  static uint32_t last = 0;
  if (millis() - last > 30000) {
    last = millis();
    if (esp_zb_lock_acquire(pdMS_TO_TICKS(100))) {
      send_info();
      esp_zb_lock_release();
    }
  }
  delay(5);
}

#endif   // ESP32-H2 + ZIGBEE_MODE_ZCZR
