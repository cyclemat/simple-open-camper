// Zigbee-Modul: Umrechnung wie Zigbee2MQTT – wird in zb_h2.cpp eingebunden (gleicher anonymer Namensraum).
//   Standard-Cluster  -> benannte Werte ("temperature", "state", …)
//   Tuya-Datenpunkte  -> Werte laut Treiberliste (Name, Faktor, Auswahl)
//   IAS-Meldungen     -> contact / occupancy / water_leak / smoke … (laut Treiber)
//   Einstellen ("set")-> Tuya-Datenpunkt, Befehl oder Attribut; occupancy_timeout als Zeitgeber im Modul
// Ausgabe ans Panel: {"t":"state","ieee":…,"p":{name:wert,…}} (wie die Zustandsmeldung von Zigbee2MQTT)

// Zustandsmeldung senden und die Werte beim Geraet merken (Einstellungsseite zeigt den aktuellen Stand)
void send_state(Dev *d, JsonDocument &j) {
  if (d) {
    for (JsonPairConst kv : j["p"].as<JsonObjectConst>()) {
      std::string v;
      serializeJson(kv.value(), v);
      bool hit = false;
      for (auto &x : d->last)
        if (x.first == kv.key().c_str()) {
          x.second = v;
          hit = true;
        }
      if (!hit && d->last.size() < 64) d->last.push_back({kv.key().c_str(), v});
    }
  }
  send(j);
}

uint32_t s_utc = 0, s_utc_ms = 0;   // Uhrzeit vom Panel ({"c":"time"}), fuer den Tuya-Zeitabgleich
int32_t s_tz = 0;                  // Abstand zur UTC in Sekunden

// ---------------------------------------------------------------- Treiber lesen
bool drv_record(uint32_t rec, ZbdRecord &r) {
  if (!s_drv_ok || !rec || rec + sizeof(r) > s_drvh.total) return false;
  memcpy(&r, s_drv + rec, sizeof(r));
  return true;
}
// alle Werte (Exposes) einer Definition: f(Expose, Abstand der Auswahltexte)
template <class F>
void drv_each_expose(uint32_t rec, F f) {
  ZbdRecord r;
  if (!drv_record(rec, r) || r.exp_off == ZBD_NONE) return;
  uint32_t p = r.exp_off;
  for (int i = 0; i < r.n_exposes && p + sizeof(ZbdExpose) <= s_drvh.total; i++) {
    ZbdExpose e;
    memcpy(&e, s_drv + p, sizeof(e));
    if (f(e, p + (uint32_t)sizeof(e))) return;
    p += sizeof(e) + 4 * e.n_values;
  }
}
// hat der Treiber einen Wert dieses Namens? (ohne Treiber: alles erlaubt)
bool drv_has(uint32_t rec, const char *prop) {
  if (!rec) return true;
  bool found = false;
  drv_each_expose(rec, [&](const ZbdExpose &e, uint32_t) {
    const char *n = drv_str(e.property);
    found = n && !strcmp(n, prop);
    return found;
  });
  return found;
}
// Tuya-Datenpunkt suchen (nach Nummer oder Name)
bool drv_dp(uint32_t rec, int dp, const char *name, ZbdDp &out, uint32_t &map_off) {
  ZbdRecord r;
  if (!drv_record(rec, r) || r.dp_off == ZBD_NONE) return false;
  uint32_t p = r.dp_off;
  for (int i = 0; i < r.n_dps && p + sizeof(ZbdDp) <= s_drvh.total; i++) {
    ZbdDp d;
    memcpy(&d, s_drv + p, sizeof(d));
    const char *n = drv_str(d.name);
    if ((dp >= 0 && d.dp == dp) || (name && n && !strcmp(n, name))) {
      out = d;
      map_off = p + sizeof(d);
      return true;
    }
    p += sizeof(d) + 8 * d.n_map;
  }
  return false;
}
const char *dp_text(uint32_t map_off, int n, int32_t raw) {
  for (int i = 0; i < n; i++) {
    ZbdDpMap m;
    memcpy(&m, s_drv + map_off + 8 * i, sizeof(m));
    if (m.raw == raw) return drv_str(m.text);
  }
  return nullptr;
}
bool dp_raw(uint32_t map_off, int n, const char *text, int32_t &raw) {
  for (int i = 0; i < n; i++) {
    ZbdDpMap m;
    memcpy(&m, s_drv + map_off + 8 * i, sizeof(m));
    const char *t = drv_str(m.text);
    if (t && !strcasecmp(t, text)) {
      raw = m.raw;
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------- Namen mit Endpunkt ("state_l2" bei Doppelschaltern)
std::string prop_name(const Dev *d, uint8_t ep, uint16_t cl, const char *base) {
  if (!d) return base;
  std::vector<uint8_t> eps;
  for (auto &e : d->eps)
    if (std::find(e.in.begin(), e.in.end(), cl) != e.in.end()) eps.push_back(e.ep);
  if (eps.size() <= 1) return base;
  std::sort(eps.begin(), eps.end());
  size_t idx = std::find(eps.begin(), eps.end(), ep) - eps.begin();
  // Endpunkt-Namen aus dem Treiber (in der Reihenfolge der Werte), sonst l1, l2 …
  std::vector<std::string> names;
  drv_each_expose(d->rec, [&](const ZbdExpose &e, uint32_t) {
    const char *n = drv_str(e.property), *en = drv_str(e.endpoint);
    if (n && en && !strcmp(n, base) && std::find(names.begin(), names.end(), en) == names.end()) names.push_back(en);
    return false;
  });
  std::string sfx = idx < names.size() ? names[idx] : "l" + std::to_string(idx + 1);
  return std::string(base) + "_" + sfx;
}
// "state_l2" -> Endpunkt des 2. Endpunkts mit diesem Cluster (sonst erster mit dem Cluster)
uint8_t ep_for(const Dev *d, uint16_t cl, const std::string &prop) {
  std::vector<uint8_t> eps;
  for (auto &e : d->eps)
    if (std::find(e.in.begin(), e.in.end(), cl) != e.in.end()) eps.push_back(e.ep);
  std::sort(eps.begin(), eps.end());
  if (eps.empty()) return d->eps.empty() ? 1 : d->eps[0].ep;
  size_t us = prop.rfind('_');
  if (us != std::string::npos && eps.size() > 1) {
    std::string sfx = prop.substr(us + 1);
    int n = -1;
    if (sfx.size() > 1 && sfx[0] == 'l' && isdigit((unsigned char)sfx[1])) n = atoi(sfx.c_str() + 1) - 1;
    if (n >= 0 && n < (int)eps.size()) return eps[n];
  }
  return eps[0];
}
std::string base_name(const std::string &prop) {
  size_t us = prop.rfind('_');
  if (us != std::string::npos && us + 2 <= prop.size() && prop[us + 1] == 'l' && isdigit((unsigned char)prop[us + 2]))
    return prop.substr(0, us);
  return prop;
}

// ---------------------------------------------------------------- Zahl aus einem Attribut
bool attr_num(const esp_zb_zcl_attribute_t &a, double &v) {
  const uint8_t *p = (const uint8_t *)a.data.value;
  if (!p) return false;
  auto le = [&](int n) {
    uint64_t x = 0;
    for (int i = n - 1; i >= 0; i--) x = (x << 8) | p[i];
    return x;
  };
  switch ((uint8_t)a.data.type) {
    case 0x10: case 0x08: case 0x18: case 0x20: case 0x30: v = p[0]; return true;
    case 0x09: case 0x19: case 0x21: case 0x31: v = (double)le(2); return true;
    case 0x22: v = (double)le(3); return true;
    case 0x0b: case 0x1b: case 0x23: v = (double)le(4); return true;
    case 0x25: v = (double)le(6); return true;   // uint48 (Energiezaehler)
    case 0x28: v = (int8_t)p[0]; return true;
    case 0x29: v = (int16_t)le(2); return true;
    case 0x2b: v = (int32_t)le(4); return true;
    case 0x39: { float f; memcpy(&f, p, 4); v = f; return true; }
    default: return false;
  }
}
double round_to(double v, int dec) {
  double f = pow(10, dec);
  return round(v * f) / f;
}

// Bewegung: Nachlaufzeit im Modul (wie occupancy_timeout bei Zigbee2MQTT)
void occupancy_seen(Dev *d, const std::string &name, bool on) {
  if (on && d && d->occ_timeout) {
    d->occ_until = millis() + d->occ_timeout * 1000UL;
    d->occ_prop = name;
  }
}

// Standard-Cluster -> Werte; liefert false, wenn nichts bekannt war
bool conv_attr(Dev *d, uint8_t ep, uint16_t cl, const esp_zb_zcl_attribute_t &a, JsonObject p) {
  double v;
  bool num = attr_num(a, v);
  uint16_t at = a.id;
  auto put = [&](const char *base, double x, int dec) {
    if (d && !drv_has(d->rec, base)) return;
    p[prop_name(d, ep, cl, base)] = round_to(x, dec);
  };
  if (!num) return false;
  switch (cl) {
    case 0x0006:
      if (at != 0x0000) return false;
      if (!d || drv_has(d->rec, "state")) p[prop_name(d, ep, cl, "state")] = v ? "ON" : "OFF";
      return true;
    case 0x0008:
      if (at != 0x0000) return false;
      put("brightness", v, 0);
      return true;
    case 0x0300:
      if (at == 0x0007) put("color_temp", v, 0);
      else if (at == 0x0003) put("color_x", v / 65535.0, 4);
      else if (at == 0x0004) put("color_y", v / 65535.0, 4);
      else if (at == 0x0000) put("hue", v * 360.0 / 254.0, 0);
      else if (at == 0x0001) put("saturation", v * 100.0 / 254.0, 0);
      else return false;
      return true;
    case 0x0402: if (at != 0) return false; put("temperature", v / 100.0, 2); return true;
    case 0x0405: if (at != 0) return false; put("humidity", v / 100.0, 2); return true;
    case 0x0403: if (at != 0) return false; put("pressure", v, 1); return true;
    case 0x0400:
      if (at != 0) return false;
      put("illuminance", v > 0 ? pow(10, (v - 1) / 10000.0) : 0, 0);
      return true;
    case 0x0406: {
      if (at != 0) return false;
      bool on = ((int)v) & 1;
      std::string n = prop_name(d, ep, cl, "occupancy");
      p[n] = on;
      occupancy_seen(d, n, on);
      return true;
    }
    case 0x0001:
      if (at == 0x0021) put("battery", v / 2.0, 0);
      else if (at == 0x0020) put("voltage", v * 100.0, 0);   // mV wie Zigbee2MQTT
      else return false;
      return true;
    case 0x0B04:
      if (at == 0x050B) put("power", v, 1);
      else if (at == 0x0505) put("voltage", v, 1);
      else if (at == 0x0508) put("current", v / 1000.0, 3);
      else return false;
      return true;
    case 0x0702:
      if (at != 0x0000) return false;
      put("energy", v / 1000.0, 3);   // ueblicher Teiler; geraeteeigene Teiler folgen mit den Treibern
      return true;
    case 0x0102:
      if (at != 0x0008) return false;
      put("position", 100 - v, 0);
      return true;
    case 0x0201:
      if (at == 0x0000) put("local_temperature", v / 100.0, 2);
      else if (at == 0x0012) put("current_heating_setpoint", v / 100.0, 2);
      else return false;
      return true;
    case 0x000C:
      if (at != 0x0055) return false;
      put("present_value", v, 2);
      return true;
    default:
      return false;
  }
}

// IAS-Zone (Kontakt, Bewegung, Wasser, Rauch …): Bit 0 = Alarm, Bit 2 = Sabotage, Bit 3 = Batterie schwach
void conv_zone(Dev *d, uint8_t ep, uint16_t st, JsonObject p) {
  bool alarm = st & 1;
  bool any = false;
  static const char *KINDS[] = {"contact", "occupancy", "water_leak", "smoke", "gas", "carbon_monoxide", "vibration",
                                "presence", "alarm"};
  for (const char *k : KINDS) {
    if (!d || !d->rec || !drv_has(d->rec, k)) continue;
    std::string n = prop_name(d, ep, 0x0500, k);
    bool val = !strcmp(k, "contact") ? !alarm : alarm;   // Kontakt: true = geschlossen (wie Zigbee2MQTT)
    p[n] = val;
    if (!strcmp(k, "occupancy")) occupancy_seen(d, n, val);
    any = true;
  }
  if (!any) p["alarm"] = alarm;
  if (!d || !d->rec || drv_has(d->rec, "tamper")) p["tamper"] = (bool)(st & 4);
  if (!d || !d->rec || drv_has(d->rec, "battery_low")) p["battery_low"] = (bool)(st & 8);
}

// ---------------------------------------------------------------- Tuya (Cluster 0xEF00)
uint16_t s_tuya_seq = 0;

void tuya_send(uint16_t nwk, uint8_t ep, uint8_t cmd, const uint8_t *pl, size_t n) {
  esp_zb_zcl_custom_cluster_cmd_t x = {};
  x.zcl_basic_cmd.dst_addr_u.addr_short = nwk;
  x.zcl_basic_cmd.dst_endpoint = ep;
  x.zcl_basic_cmd.src_endpoint = COORD_EP;
  x.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  x.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  x.cluster_id = 0xEF00;
  x.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  x.custom_cmd_id = cmd;
  x.data.type = ESP_ZB_ZCL_ATTR_TYPE_SET;
  x.data.size = n;
  x.data.value = n ? (void *)pl : nullptr;
  esp_zb_zcl_custom_cluster_cmd_req(&x);
}
// alle Datenpunkte abfragen (nach dem Anlernen)
void tuya_query(const Dev &d) {
  uint8_t ep = ep_for(&d, 0xEF00, "");
  tuya_send(d.nwk, ep, 0x03, nullptr, 0);
}
// Uhrzeit an das Geraet (Anfrage 0x24): Laenge (u16 LE) + UTC + Ortszeit (je u32 BE)
void tuya_time(const Dev *d, uint8_t ep) {
  if (!d || !s_utc) return;
  uint32_t now = s_utc + (millis() - s_utc_ms) / 1000;
  uint32_t loc = now + s_tz;
  uint8_t b[10] = {8, 0, (uint8_t)(now >> 24), (uint8_t)(now >> 16), (uint8_t)(now >> 8), (uint8_t)now,
                   (uint8_t)(loc >> 24), (uint8_t)(loc >> 16), (uint8_t)(loc >> 8), (uint8_t)loc};
  tuya_send(d->nwk, ep, 0x24, b, sizeof(b));
}

// Datenpunkte einer Tuya-Meldung umrechnen
void tuya_in(Dev *d, uint8_t ep, uint8_t cmd, const uint8_t *b, uint16_t n, JsonObject p) {
  if (cmd == 0x24) {
    tuya_time(d, ep);
    return;
  }
  if (!(cmd == 0x01 || cmd == 0x02 || cmd == 0x06)) return;
  size_t i = 2;   // Folgenummer ueberspringen
  while (i + 4 <= n) {
    uint8_t dp = b[i], type = b[i + 1];
    uint16_t len = (b[i + 2] << 8) | b[i + 3];
    i += 4;
    if (i + len > n) break;
    const uint8_t *v = b + i;
    i += len;
    bool isnum = type == 0x01 || type == 0x02 || type == 0x04 || type == 0x05;
    int32_t raw = 0;
    if (isnum)
      for (int k = 0; k < len && k < 4; k++) raw = (raw << 8) | v[k];
    auto hex = [&]() {
      std::string s;
      char h[3];
      for (int k = 0; k < len; k++) {
        snprintf(h, sizeof(h), "%02x", v[k]);
        s += h;
      }
      return s;
    };
    ZbdDp dpi;
    uint32_t mo;
    if (d && drv_dp(d->rec, dp, nullptr, dpi, mo) && drv_str(dpi.name)) {
      std::string name = drv_str(dpi.name);
      const char *t;
      switch (dpi.kind) {
        case ZBD_DP_NUMBER: p[name] = isnum ? round_to(raw * dpi.factor, 3) : 0; break;
        case ZBD_DP_BOOL:
          t = dp_text(mo, dpi.n_map, raw);
          p[name] = t ? !strcmp(t, "true") : raw != 0;
          break;
        case ZBD_DP_ENUM:
          t = dp_text(mo, dpi.n_map, raw);
          if (t) p[name] = t;
          else p[name] = raw;
          break;
        default:
          if (isnum) p[name] = raw;
          else if (type == 0x03) p[name] = std::string((const char *)v, len);
          else p[name] = hex();
          break;
      }
      if (name == "occupancy" || name == "presence") occupancy_seen(d, name, p[name].as<bool>());
    } else {
      std::string name = "dp_" + std::to_string(dp);
      if (isnum) p[name] = raw;
      else p[name] = hex();
    }
  }
}

// Tuya-Datenpunkt schreiben: Datentyp aus der Treiberliste (ZbdDp.reserved), sonst aus der Art abgeleitet
bool tuya_set(Dev *d, const char *name, JsonVariantConst val, std::string &err) {
  ZbdDp dpi;
  uint32_t mo;
  if (!drv_dp(d->rec, -1, name, dpi, mo)) return false;
  uint8_t type = dpi.reserved ? dpi.reserved
                              : dpi.kind == ZBD_DP_NUMBER ? 0x02 : dpi.kind == ZBD_DP_BOOL ? 0x01 : 0x04;
  int32_t raw = 0;
  if (dpi.kind == ZBD_DP_NUMBER) {
    if (!val.is<double>()) { err = "Zahl erwartet"; return true; }
    raw = (int32_t)lround(val.as<double>() / (dpi.factor ? dpi.factor : 1));
  } else if (dpi.kind == ZBD_DP_BOOL || dpi.kind == ZBD_DP_ENUM) {
    std::string t = val.is<bool>() ? (val.as<bool>() ? "true" : "false")
                    : val.is<const char *>() ? val.as<const char *>() : std::to_string(val.as<long>());
    if (t == "ON" || t == "on") t = dpi.kind == ZBD_DP_BOOL ? "true" : t;
    if (t == "OFF" || t == "off") t = dpi.kind == ZBD_DP_BOOL ? "false" : t;
    if (!dp_raw(mo, dpi.n_map, t.c_str(), raw)) { err = "unbekannter Wert \"" + t + "\""; return true; }
  } else {
    if (!val.is<long>()) { err = "Datenpunkt unbekannter Art"; return true; }
    raw = val.as<long>();
  }
  uint8_t b[12];
  size_t n = 0;
  s_tuya_seq++;
  b[n++] = s_tuya_seq & 0xFF;   // Folgenummer
  b[n++] = s_tuya_seq >> 8;
  b[n++] = dpi.dp;
  b[n++] = type;
  if (type == 0x02) {
    b[n++] = 0; b[n++] = 4;
    b[n++] = raw >> 24; b[n++] = raw >> 16; b[n++] = raw >> 8; b[n++] = raw;
  } else {
    b[n++] = 0; b[n++] = 1;
    b[n++] = (uint8_t)raw;
  }
  tuya_send(d->nwk, ep_for(d, 0xEF00, ""), 0x00, b, n);
  return true;
}

// ---------------------------------------------------------------- Einstellen (Standardgeraete)
void write_attr(const Dev *d, uint8_t ep, uint16_t cl, uint16_t at, uint8_t type, void *val, uint16_t size) {
  esp_zb_zcl_attribute_t a = {};
  a.id = at;
  a.data.type = (esp_zb_zcl_attr_type_t)type;
  a.data.size = size;
  a.data.value = val;
  esp_zb_zcl_write_attr_cmd_t w = {};
  w.zcl_basic_cmd.dst_addr_u.addr_short = d->nwk;
  w.zcl_basic_cmd.dst_endpoint = ep;
  w.zcl_basic_cmd.src_endpoint = COORD_EP;
  w.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  w.clusterID = cl;
  w.attr_number = 1;
  w.attr_field = &a;
  esp_zb_zcl_write_attr_cmd_req(&w);
}

// ein Wert setzen; leerer Fehlertext = erledigt
std::string set_one(Dev *d, const std::string &prop, JsonVariantConst val) {
  std::string err;
  if (d->rec && tuya_set(d, prop.c_str(), val, err)) return err;   // Tuya-Datenpunkt mit diesem Namen
  std::string base = base_name(prop);
  if (base == "occupancy_timeout") {
    d->occ_timeout = (uint16_t)constrain((int)(val | 0), 0, 3600);
    save_devs();
    return "";
  }
  if (base == "state") {
    std::string s = val.is<const char *>() ? val.as<const char *>() : (val.as<bool>() ? "ON" : "OFF");
    esp_zb_zcl_on_off_cmd_t o = {};
    o.zcl_basic_cmd.dst_addr_u.addr_short = d->nwk;
    o.zcl_basic_cmd.dst_endpoint = ep_for(d, 0x0006, prop);
    o.zcl_basic_cmd.src_endpoint = COORD_EP;
    o.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    o.on_off_cmd_id = (s == "ON" || s == "on") ? 0x01 : (s == "TOGGLE" || s == "toggle") ? 0x02 : 0x00;
    esp_zb_zcl_on_off_cmd_req(&o);
    return "";
  }
  if (base == "brightness") {
    esp_zb_zcl_move_to_level_cmd_t m = {};
    m.zcl_basic_cmd.dst_addr_u.addr_short = d->nwk;
    m.zcl_basic_cmd.dst_endpoint = ep_for(d, 0x0008, prop);
    m.zcl_basic_cmd.src_endpoint = COORD_EP;
    m.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    m.level = (uint8_t)constrain((int)(val | 0), 0, 254);
    m.transition_time = 5;
    esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&m);
    return "";
  }
  if (base == "color_temp") {
    esp_zb_zcl_color_move_to_color_temperature_cmd_t c = {};
    c.zcl_basic_cmd.dst_addr_u.addr_short = d->nwk;
    c.zcl_basic_cmd.dst_endpoint = ep_for(d, 0x0300, prop);
    c.zcl_basic_cmd.src_endpoint = COORD_EP;
    c.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    c.color_temperature = (uint16_t)constrain((int)(val | 0), 50, 1000);
    c.transition_time = 5;
    esp_zb_zcl_color_move_to_color_temperature_cmd_req(&c);
    return "";
  }
  if (base == "position") {
    uint8_t pct = (uint8_t)(100 - constrain((int)(val | 0), 0, 100));
    esp_zb_zcl_window_covering_cluster_send_cmd_req_t w = {};
    w.zcl_basic_cmd.dst_addr_u.addr_short = d->nwk;
    w.zcl_basic_cmd.dst_endpoint = ep_for(d, 0x0102, prop);
    w.zcl_basic_cmd.src_endpoint = COORD_EP;
    w.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    w.cmd_id = ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE;
    w.value = &pct;
    esp_zb_zcl_window_covering_cluster_send_cmd_req(&w);
    return "";
  }
  if (base == "current_heating_setpoint" || base == "occupied_heating_setpoint") {
    int16_t t = (int16_t)lround((val | 0.0) * 100);
    write_attr(d, ep_for(d, 0x0201, prop), 0x0201, 0x0012, 0x29, &t, 2);
    return "";
  }
  return "\"" + prop + "\" laesst sich (noch) nicht einstellen";
}

// Nachlaufzeit abgelaufen: Bewegung zuruecksetzen (aus loop(), unter esp_zb_lock)
void occupancy_tick() {
  for (auto &d : s_devs) {
    if (!d.occ_until || (int32_t)(millis() - d.occ_until) < 0) continue;
    d.occ_until = 0;
    JsonDocument j;
    j["t"] = "state";
    j["ieee"] = ieee_str(d.ieee);
    j["p"][d.occ_prop.empty() ? "occupancy" : d.occ_prop] = false;
    send_state(&d, j);
  }
}

// ---------------------------------------------------------------- Einstellungsseite ("config")
// je einstellbarem Wert eine Zeile {"t":"cfg",…}, am Ende {"t":"cfg_end"} – das Panel haelt die Liste nur, solange
// die Seite offen ist (spart RAM im Panel)
void send_config(Dev *d) {
  std::string ieee = ieee_str(d->ieee);
  int n = 0;
  auto cur = [&](const char *name, JsonDocument &j) {
    for (auto &x : d->last)
      if (x.first == name) {
        JsonDocument v;
        if (!deserializeJson(v, x.second)) j["cur"] = v.as<JsonVariantConst>();
      }
  };
  bool occ = false;
  drv_each_expose(d->rec, [&](const ZbdExpose &e, uint32_t vals) {
    const char *name = drv_str(e.property);
    if (!name) return false;
    if (!strcmp(name, "occupancy")) occ = true;
    if (!(e.access & ZBD_A_SET)) return false;
    JsonDocument j;
    j["t"] = "cfg";
    j["ieee"] = ieee;
    j["n"] = name;
    j["ty"] = e.type;
    j["g"] = e.group;
    if (const char *u = drv_str(e.unit)) j["u"] = u;
    if (e.type == ZBD_T_NUMERIC) {
      j["mi"] = e.min;
      j["ma"] = e.max;
      if (e.step) j["st"] = e.step;
    }
    if (e.n_values) {
      JsonArray v = j["v"].to<JsonArray>();
      for (int i = 0; i < e.n_values; i++) {
        uint32_t o;
        memcpy(&o, s_drv + vals + 4 * i, 4);
        if (const char *t = drv_str(o)) v.add(t);
      }
    }
    cur(name, j);
    send(j);
    n++;
    return false;
  });
  if (occ || !d->rec) {   // Nachlaufzeit fuer Bewegungsmelder (im Modul, wie occupancy_timeout bei Zigbee2MQTT)
    bool has = false;
    for (auto &e : d->eps) has |= std::find(e.in.begin(), e.in.end(), 0x0406) != e.in.end() ||
                                  std::find(e.in.begin(), e.in.end(), 0x0500) != e.in.end();
    if (occ || has) {
      JsonDocument j;
      j["t"] = "cfg";
      j["ieee"] = ieee;
      j["n"] = "occupancy_timeout";
      j["ty"] = ZBD_T_NUMERIC;
      j["u"] = "s";
      j["mi"] = 0;
      j["ma"] = 3600;
      j["st"] = 1;
      j["cur"] = d->occ_timeout;
      send(j);
      n++;
    }
  }
  JsonDocument e;
  e["t"] = "cfg_end";
  e["ieee"] = ieee;
  e["n"] = n;
  send(e);
}
