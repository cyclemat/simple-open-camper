// Einstellungen am Panel: WLAN, MQTT, Anzeige, Geraete (Umbenennen), Info
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../app_config.h"
#include "../board.h"
#include "../layout.h"
#include "../net.h"
#include "../settings.h"
#include "../z2m.h"
#include "../truma.h"
#include "../victron.h"
#include "../cerbo.h"
#include "../gx_find.h"
#include "../panelsat.h"
#include "../truma_cpplus.h"
#include "../truma_inetx.h"
#include "../hymer.h"
#include "../web_server.h"
#include "../alde.h"
#include "../power.h"
#include "../backup.h"
#include "../shelly.h"
#include "../wled.h"
#include "../sdcard.h"
#include <Arduino.h>
#include <functional>
#include <vector>
#include "esp_heap_caps.h"

static lv_obj_t *s_scr = nullptr, *s_content = nullptr;
static lv_obj_t *s_nav_items[SEC_COUNT];
static int s_sec = SEC_WIFI;
static Settings s_draft;

// dynamische Elemente der aktuellen Seite
static lv_obj_t *s_wifi_status = nullptr, *s_wifi_ip = nullptr, *s_scan_box = nullptr;
static std::vector<WifiNet> s_scan;
static lv_obj_t *s_mqtt_vals[5];
static lv_obj_t *s_mqtt_status = nullptr;
static lv_obj_t *s_bl_val = nullptr, *s_dim_val = nullptr;
static lv_obj_t *s_info[10];
static lv_obj_t *s_info_url = nullptr, *s_info_url2 = nullptr;
static std::vector<lv_obj_t *> s_vic_vals;
static uint32_t s_vic_sig = 0;
static std::vector<lv_obj_t *> s_sat_vals;   // Satelliten in "Geraete"
static uint32_t s_sat_sig = 0;
static std::vector<lv_obj_t *> s_gx_vals;     // Cerbo GX: Status + fuenf Werte
static uint32_t s_gx_sig = 0;
static lv_obj_t *s_clock_val = nullptr;
static lv_obj_t *s_web_val = nullptr;   // Netzwerk-Seite: Zustand des Webinterface

bool ui_settings_is_open() { return s_scr != nullptr; }
static void show_section(int sec);

static void reset_dynamic() {
  s_wifi_status = s_wifi_ip = s_scan_box = nullptr;
  s_mqtt_status = nullptr;
  s_bl_val = s_dim_val = nullptr;
  s_clock_val = nullptr;
  s_web_val = nullptr;
  s_info_url = s_info_url2 = nullptr;
  for (auto &p : s_mqtt_vals) p = nullptr;
  for (auto &p : s_info) p = nullptr;
  s_vic_vals.clear();
  s_gx_vals.clear();
  s_sat_vals.clear();
}

static lv_obj_t *heading(const char *title, const char *hint) {
  lv_obj_t *t = ui_label(s_content, title, F_TITLE, C_TEXT);
  (void)t;
  if (hint) {
    lv_obj_t *h = ui_label(s_content, hint, F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  return t;
}

static lv_obj_t *button_row() {
  lv_obj_t *row = lv_obj_create(s_content);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(row, 6, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
  return row;
}

static lv_obj_t *group() {
  lv_obj_t *g = lv_obj_create(s_content);
  ui_plain(g);
  lv_obj_set_size(g, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_COLUMN);
  return g;
}

// ---------------------------------------------------------------- WLAN
static void wifi_connect(const std::string &ssid, const std::string &pass) {
  Settings st = settings_copy();
  st.wifi_ssid = ssid.c_str();
  st.wifi_pass = pass.c_str();
  settings_apply(st);
  net_reconnect();
  ui_toast((tr("Verbinde mit ") + ssid + " …").c_str());
}

static void ask_password(const std::string &ssid) {
  InputOptions o;
  o.title = tr("Passwort für ") + ssid;
  o.hint = "WLAN-Passwort eingeben. Mit dem Auge-Symbol wird es sichtbar.";
  o.password = true;
  o.max_len = 64;
  o.on_ok = [ssid](const std::string &pw, bool) { wifi_connect(ssid, pw); };
  ui_input_open(o);
}

static void scan_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_scan.size()) return;
  WifiNet n = s_scan[i];
  if (n.secure) ask_password(n.ssid.c_str());
  else wifi_connect(n.ssid.c_str(), "");
}

static void fill_scan() {
  if (!s_scan_box) return;
  lv_obj_clean(s_scan_box);
  if (s_scan.empty()) {
    lv_obj_t *l = ui_label(s_scan_box,
                           net_scan_failed() ? "Die Suche hat nicht geklappt. Bitte noch einmal auf „Netzwerke suchen“ tippen."
                                             : "Keine Netze gefunden. Das Panel unterstützt nur 2,4 GHz.",
                           F_BODY, C_TEXT2);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    return;
  }
  for (size_t i = 0; i < s_scan.size() && i < 20; i++) {
    const WifiNet &n = s_scan[i];
    const char *q = n.rssi > -60 ? "stark" : n.rssi > -72 ? "mittel" : "schwach";
    char v[48];
    snprintf(v, sizeof(v), "%s, %s", q, n.secure ? "gesichert" : "offen");
    ui_row(s_scan_box, n.ssid.c_str(), v, scan_row_cb, (void *)(intptr_t)i);
  }
}

static void scan_cb(lv_event_t *) {
  net_scan_start();
  if (!s_scan_box) return;
  lv_obj_clean(s_scan_box);
  lv_obj_t *row = lv_obj_create(s_scan_box);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), 64);
  lv_obj_t *sp = lv_spinner_create(row, 1000, 60);
  lv_obj_set_size(sp, 36, 36);
  lv_obj_set_style_arc_width(sp, 4, 0);
  lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sp, C_AMBER, LV_PART_INDICATOR);
  lv_obj_align(sp, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t *l = ui_label(row, "Suche nach Netzen …", F_BODY, C_TEXT2);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 56, 0);
}

static void hidden_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Netzname (SSID)";
  o.hint = "Name des verborgenen Netzes, genau wie im Router eingetragen.";
  o.max_len = 32;
  o.on_ok = [](const std::string &ssid, bool) {
    if (!ssid.empty()) ask_password(ssid);
  };
  ui_input_open(o);
}

static lv_obj_t *subheading(const char *t) {
  lv_obj_t *l = ui_label(s_content, t, F_BODY, C_TEXT2);
  lv_obj_set_style_pad_top(l, 18, 0);
  return l;
}

static void restart_soon() {
  ui_toast("Panel startet neu …");
  lv_timer_create([](lv_timer_t *) { ESP.restart(); }, 1200, nullptr);
}

static const char *MODE_TITLES[4] = {"Netzwerk", "Router", "Kombi", "WLAN aus"};
static const char *MODE_TEXT[4] = {"Panel und Hub hängen beide im Heim-WLAN.",
                                   "Panel öffnet ein eigenes WLAN für den Hub. Kein Heimnetz nötig.",
                                   "Eigenes WLAN für den Hub plus Heim-WLAN mit Internet-Weiterleitung.",
                                   "Kein WLAN: mehr Speicher, Bluetooth läuft ungestört. Kein MQTT, kein Editor."};

static void mode_cb(lv_event_t *e) {
  int m = (int)(intptr_t)lv_event_get_user_data(e);
  if (m == g_settings.net_mode) return;
  static int pending;
  pending = m;
  char t[160];
  snprintf(t, sizeof(t), tr("Das Panel wechselt in den %s-Modus und startet dafür neu."), tr(MODE_TITLES[m]));
  ui_confirm("Betriebsart wechseln?", t, "Wechseln und neu starten", false, [] {
    Settings st = settings_copy();
    st.net_mode = pending;
    settings_apply(st);
    restart_soon();
  });
}

static void ap_edit_cb(lv_event_t *e) {
  int f = (int)(intptr_t)lv_event_get_user_data(e);
  InputOptions o;
  if (f == 0) {
    o.title = "Name des Panel-WLANs";
    o.hint = "Unter diesem Namen findet der Hub das Panel. Änderung gilt nach einem Neustart.";
    o.initial = g_settings.ap_ssid.c_str();
    o.max_len = 32;
  } else if (f == 1) {
    o.title = "Passwort des Panel-WLANs";
    o.hint = "8 bis 63 Zeichen. Änderung gilt nach einem Neustart – der Hub muss dann neu verbunden werden.";
    o.initial = g_settings.ap_pass.c_str();
    o.max_len = 63;
  } else {
    o.title = "Funkkanal (1–13)";
    o.hint = "Möglichst weit weg vom Zigbee-Kanal: bei Zigbee 11 eignet sich WLAN 13, bei Zigbee 25 WLAN 1.";
    o.initial = std::to_string(g_settings.ap_channel);
    o.numeric = true;
    o.max_len = 2;
  }
  o.on_ok = [f](const std::string &t, bool) {
    Settings st = settings_copy();
    String v = t.c_str();
    v.trim();
    if (f == 0) {
      if (v.isEmpty()) return;
      st.ap_ssid = v;
    } else if (f == 1) {
      if (v.length() < 8) {
        ui_toast("Das Passwort braucht mindestens 8 Zeichen.");
        return;
      }
      st.ap_pass = v;
    } else {
      int c = v.toInt();
      if (c < 1 || c > 13) {
        ui_toast("Bitte einen Kanal zwischen 1 und 13 wählen.");
        return;
      }
      st.ap_channel = c;
    }
    settings_apply(st);
    restart_soon();
  };
  ui_input_open(o);
}

// Webinterface: immer an / bei Bedarf
static lv_obj_t *dropdown_row(const char *title, const char *opts, int sel, lv_event_cb_t cb);
static void web_mode_cb(lv_event_t *e) {
  Settings st = settings_copy();
  st.web_mode = lv_dropdown_get_selected(lv_event_get_target(e)) ? 1 : 0;
  settings_apply(st);
  if (st.web_mode == 0) web_start();
  ui_settings_refresh();
}
static void web_now_cb(lv_event_t *) {
  if (web_running()) web_stop();
  else web_start();
  ui_settings_refresh();
}
static std::string web_text() {
  if (net_mode() == NET_OFF) return tr("Aus (WLAN ausgeschaltet)");
  if (!web_running()) return tr("Aus – zum Bearbeiten freigeben");
  uint32_t s = web_idle_left_s();
  if (!s) return tr("Läuft");
  char b[64];
  snprintf(b, sizeof(b), tr("Läuft, aus in %lu min ohne Zugriff"), (unsigned long)((s + 59) / 60));
  return b;
}

static void build_web() {
  s_web_val = nullptr;
  if (net_mode() == NET_OFF) return;
  subheading("Webinterface (Editor)");
  dropdown_row("Webinterface", "Immer an\nNur bei Bedarf", g_settings.web_mode, web_mode_cb);
  lv_obj_t *g = group();
  s_web_val = ui_row(g, "Zustand", web_text().c_str(), nullptr, nullptr);
  if (g_settings.web_mode) {
    lv_obj_t *row = button_row();
    ui_button(row, ICON_EYE, web_running() ? "Webinterface beenden" : "Webinterface freigeben", !web_running(), web_now_cb,
              nullptr);
  }
  lv_obj_t *h = ui_label(s_content,
                         "„Nur bei Bedarf“ spart internen Speicher: Der Editor ist erst nach „freigeben“ erreichbar und "
                         "schaltet sich nach 15 Minuten ohne Zugriff wieder ab.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
}

static void build_wifi() {
  heading("Netzwerk", nullptr);
  // Betriebsart
  lv_obj_t *modes = lv_obj_create(s_content);
  ui_plain(modes);
  lv_obj_set_size(modes, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(modes, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(modes, 12, 0);
  for (int m = 0; m < 4; m++) {
    bool act = m == g_settings.net_mode;
    lv_obj_t *c = lv_obj_create(modes);
    ui_plain(c);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_style_radius(c, 16, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, act ? C_FIELD : C_CARD, 0);
    lv_obj_set_style_bg_color(c, C_LINE, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, act ? C_AMBER : C_LINE, 0);
    lv_obj_set_style_pad_all(c, 16, 0);
    lv_obj_set_style_pad_row(c, 6, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    ui_label(c, MODE_TITLES[m], F_BODY, act ? C_AMBER : C_TEXT);
    lv_obj_t *d = ui_label(c, MODE_TEXT[m], F_SMALL, C_TEXT2);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(d, lv_pct(100));
    lv_obj_add_event_cb(c, mode_cb, LV_EVENT_CLICKED, (void *)(intptr_t)m);
  }
  if (net_mode() != g_settings.net_mode) {
    ui_label(s_content, "Neue Betriebsart ist gespeichert und gilt nach dem Neustart.", F_SMALL, C_AMBER);
  }

  if (g_settings.net_mode == NET_AP || g_settings.net_mode == NET_APSTA) {
    subheading("Panel-WLAN");
    lv_obj_t *g = group();
    char ch[40];
    snprintf(ch, sizeof(ch), "%u%s", g_settings.ap_channel,
             g_settings.net_mode == NET_APSTA ? tr(" (folgt dem Heim-WLAN)") : "");
    ui_row(g, "Name", g_settings.ap_ssid.c_str(), ap_edit_cb, (void *)(intptr_t)0);
    ui_row(g, "Passwort", g_settings.ap_pass.c_str(), ap_edit_cb, (void *)(intptr_t)1);
    ui_row(g, "Kanal", ch, ap_edit_cb, (void *)(intptr_t)2);
    lv_obj_t *h = ui_label(s_content,
                           "Am SMHUB Nano unter Einstellungen > WLAN dieses Netz wählen (2,4 GHz). Der Editor ist dann "
                           "unter http://192.168.4.1/ erreichbar, wenn dein Handy ebenfalls verbunden ist.",
                           F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  if (g_settings.net_mode == NET_STA || g_settings.net_mode == NET_APSTA) {
    subheading("Heim-WLAN");
    lv_obj_t *card = group();
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    s_wifi_status = ui_label(card, net_status_text(), F_BODY, C_TEXT);
    s_wifi_ip = ui_label(card, "", F_SMALL, C_TEXT2);
    lv_obj_t *row = button_row();
    ui_button(row, ICON_REFRESH, "Netzwerke suchen", true, scan_cb, nullptr);
    ui_button(row, ICON_PLUS, "Verborgenes Netz", false, hidden_cb, nullptr);
    s_scan_box = group();
    if (!s_scan.empty()) fill_scan();
  } else {
    s_wifi_status = ui_label(s_content, net_status_text(), F_BODY, C_TEXT2);
    s_wifi_ip = ui_label(s_content, "", F_SMALL, C_TEXT2);
  }
  build_web();
}

// ---------------------------------------------------------------- MQTT
static const char *MQTT_TITLES[5] = {"Server", "Port", "Benutzer", "Passwort", "Basis-Topic"};

static std::string mqtt_value(int i) {
  switch (i) {
    case 0: return s_draft.mqtt_host.isEmpty() ? "nicht gesetzt" : s_draft.mqtt_host.c_str();
    case 1: return std::to_string(s_draft.mqtt_port);
    case 2: return s_draft.mqtt_user.isEmpty() ? "ohne" : s_draft.mqtt_user.c_str();
    case 3: return s_draft.mqtt_pass.isEmpty() ? "ohne" : "••••••••";
    default: return s_draft.mqtt_base.c_str();
  }
}

static void mqtt_row_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  InputOptions o;
  o.title = MQTT_TITLES[i];
  switch (i) {
    case 0:
      o.hint = "IP-Adresse oder Hostname des Brokers, z. B. 192.168.1.10 oder homeassistant.local";
      o.initial = s_draft.mqtt_host.c_str();
      break;
    case 1:
      o.hint = "Standard ist 1883.";
      o.initial = std::to_string(s_draft.mqtt_port);
      o.numeric = true;
      o.max_len = 5;
      break;
    case 2:
      o.hint = "Leer lassen, wenn der Broker ohne Anmeldung arbeitet.";
      o.initial = s_draft.mqtt_user.c_str();
      break;
    case 3:
      o.hint = "Leer lassen, wenn der Broker ohne Anmeldung arbeitet.";
      o.initial = s_draft.mqtt_pass.c_str();
      o.password = true;
      break;
    default:
      o.hint = "Wie in Zigbee2MQTT unter mqtt.base_topic – meistens zigbee2mqtt.";
      o.initial = s_draft.mqtt_base.c_str();
      break;
  }
  o.on_ok = [i](const std::string &t, bool) {
    String v = t.c_str();
    v.trim();
    switch (i) {
      case 0: s_draft.mqtt_host = v; break;
      case 1: s_draft.mqtt_port = constrain(v.toInt(), 1, 65535); break;
      case 2: s_draft.mqtt_user = v; break;
      case 3: s_draft.mqtt_pass = t.c_str(); break;
      default: s_draft.mqtt_base = v.isEmpty() ? String(DEFAULT_MQTT_BASE) : v; break;
    }
    if (s_mqtt_vals[i]) lv_label_set_text(s_mqtt_vals[i], mqtt_value(i).c_str());
  };
  ui_input_open(o);
}

static void mqtt_save_cb(lv_event_t *) {
  Settings st = settings_copy();
  st.mqtt_host = s_draft.mqtt_host;
  st.mqtt_port = s_draft.mqtt_port;
  st.mqtt_user = s_draft.mqtt_user;
  st.mqtt_pass = s_draft.mqtt_pass;
  st.mqtt_base = s_draft.mqtt_base;
  st.mqtt_auto = s_draft.mqtt_auto;
  settings_apply(st);
  z2m_restart();
  ui_toast("Gespeichert – verbinde mit dem Broker …");
}

static void mqtt_rebuild(void *) { show_section(s_sec); }
static void mqtt_on_cb(lv_event_t *e) {
  Settings st = settings_copy();
  st.mqtt_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  settings_apply(st);   // startet bzw. beendet den MQTT-Client
  // Seite gleich neu aufbauen (Eingabefelder ein-/ausblenden); asynchron, weil der Schalter selbst auf der Seite liegt
  lv_async_call(mqtt_rebuild, nullptr);
}

static void auto_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  s_draft.mqtt_auto = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (s_mqtt_vals[0]) lv_label_set_text(s_mqtt_vals[0], s_draft.mqtt_auto ? "automatisch" : mqtt_value(0).c_str());
}

static void build_mqtt() {
  heading("MQTT-Broker", "Derselbe Broker, den auch Zigbee2MQTT benutzt – beim SMHUB Nano der eingebaute Mosquitto. "
                         "Dort unter MQTT „Allow External“ einschalten, damit das Panel sich verbinden darf.");
  s_draft = settings_copy();
  {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_t *t = ui_label(r, "MQTT verwenden", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_draft.mqtt_on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, mqtt_on_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  }
  if (!s_draft.mqtt_on) {
    lv_obj_t *h = ui_label(s_content,
                           "MQTT ist aus: keine Zigbee-Geräte, kein Cerbo über den Broker. Bluetooth-Geräte und "
                           "Satelliten per UDP/Bluetooth laufen weiter. Spart Speicher und Funkzeit.",
                           F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
    return;
  }
  if (s_draft.net_mode == NET_AP || s_draft.net_mode == NET_APSTA) {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_t *t = ui_label(r, "Broker im Panel-WLAN automatisch finden", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_draft.mqtt_auto) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, auto_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  }
  lv_obj_t *g = group();
  for (int i = 0; i < 5; i++) s_mqtt_vals[i] = ui_row(g, MQTT_TITLES[i], mqtt_value(i).c_str(), mqtt_row_cb, (void *)(intptr_t)i);
  if ((s_draft.net_mode == NET_AP || s_draft.net_mode == NET_APSTA) && s_draft.mqtt_auto) lv_label_set_text(s_mqtt_vals[0], "automatisch");
  s_mqtt_status = ui_label(s_content, z2m_status_text(), F_BODY, C_TEXT2);
  lv_obj_t *row = button_row();
  ui_button(row, ICON_CHECK, "Speichern und verbinden", true, mqtt_save_cb, nullptr);
}

// ---------------------------------------------------------------- Anzeige
static const uint16_t DIM_VALUES[] = {0, 30, 60, 120, 300, 600, 1800};

static void bl_cb(lv_event_t *e) {
  lv_obj_t *s = lv_event_get_target(e);
  int v = lv_slider_get_value(s);
  board_set_backlight(v);
  if (s_bl_val) {
    char b[16];
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", v);
    lv_label_set_text(s_bl_val, b);
  }
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    Settings st = settings_copy();
    st.brightness = v;
    settings_apply(st);
  }
}

static void dim_lvl_text(int v) {
  if (!s_dim_val) return;
  char b[16];
  if (v == 0) snprintf(b, sizeof(b), "Display aus");
  else snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", v);
  lv_label_set_text(s_dim_val, b);
}

static void dim_lvl_cb(lv_event_t *e) {
  int v = lv_slider_get_value(lv_event_get_target(e));
  dim_lvl_text(v);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    Settings st = settings_copy();
    st.dim_level = v;
    settings_apply(st);
    board_dim(v);  // kurze Vorschau
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) { board_wake(); lv_timer_del(tm); }, 1200, nullptr);
    (void)t;
  }
}

static void dim_after_cb(lv_event_t *e) {
  int i = lv_dropdown_get_selected(lv_event_get_target(e));
  Settings st = settings_copy();
  st.dim_after = DIM_VALUES[i];
  settings_apply(st);
}

static void hostname_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Gerätename im Netzwerk";
  o.hint = "Erreichbar als <name>.local. Erlaubt sind a–z, 0–9 und Bindestrich. Gilt nach einem Neustart.";
  o.initial = g_settings.hostname.c_str();
  o.max_len = 32;
  o.on_ok = [](const std::string &t, bool) {
    String h;
    for (char c : t) {
      if (c >= 'A' && c <= 'Z') c += 32;
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') h += c;
    }
    if (h.isEmpty()) return;
    Settings st = settings_copy();
    st.hostname = h;
    settings_apply(st);
    ui_settings_refresh();
    ui_toast("Wird nach dem nächsten Neustart übernommen.");
  };
  ui_input_open(o);
}

static lv_obj_t *slider_block(const char *title, lv_obj_t **val, int min, int max, int cur, lv_event_cb_t cb) {
  lv_obj_t *box = group();
  lv_obj_set_style_pad_ver(box, 12, 0);
  lv_obj_set_style_pad_hor(box, 14, 0);
  lv_obj_set_style_pad_row(box, 16, 0);
  lv_obj_t *row = lv_obj_create(box);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), 30);
  lv_obj_t *t = ui_label(row, title, F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
  *val = ui_label(row, "", F_BODY, C_TEXT2);
  lv_obj_align(*val, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_t *s = lv_slider_create(box);
  lv_obj_set_width(s, lv_pct(100));
  ui_style_slider(s, C_AMBER);
  lv_slider_set_range(s, min, max);
  lv_slider_set_value(s, cur, LV_ANIM_OFF);
  lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(s, cb, LV_EVENT_RELEASED, nullptr);
  return s;
}

// Ein Text je Bildschirmschoner (Reihenfolge wie SAVER_* in settings.h)
static const char *SAVER_HINT[SAVER_DUEL + 1] = {
    "Das Dashboard bleibt gedimmt sichtbar. Ein LCD kann bei dauerhaft gleichem Bild einen Abdruck behalten.",
    "Bild schwarz, Beleuchtung aus. Schont das Display am meisten.",
    "Uhrzeit und Datum, gedimmt. Die Uhr springt jede Minute an eine neue Stelle.",
    "Ein Abzeichen mit Uhrzeit hüpft über den Bildschirm und wechselt an jedem Rand die Farbe. "
    "Ein eigenes Logo lädst du im Browser-Editor hoch.",
    "Zwei Schläger spielen von selbst Ball gegeneinander, oben steht der Punktestand."};

static void saver_mode_cb(lv_event_t *e) {
  Settings st = settings_copy();
  st.saver = lv_dropdown_get_selected(lv_event_get_target(e));
  settings_apply(st);
  ui_settings_refresh();
}

static void saver_rc_cb(lv_event_t *e) {
  Settings st = settings_copy();
  st.saver_recolor = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  settings_apply(st);
}

static void saver_preview_cb(lv_event_t *) { ui_saver_start(); }

static lv_obj_t *dropdown_row(const char *title, const char *opts, int sel, lv_event_cb_t cb) {
  lv_obj_t *row = lv_obj_create(s_content);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), 72);
  lv_obj_t *t = ui_label(row, title, F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t *dd = lv_dropdown_create(row);
  lv_dropdown_set_options(dd, tr_lines(opts).c_str());
  lv_dropdown_set_selected(dd, sel);
  lv_obj_set_width(dd, 280);
  lv_obj_set_style_text_font(dd, F_BODY, 0);
  lv_obj_set_style_bg_color(dd, C_FIELD, 0);
  lv_obj_set_style_border_width(dd, 0, 0);
  lv_obj_set_style_radius(dd, 12, 0);
  lv_obj_set_style_pad_ver(dd, 14, 0);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), F_BODY, 0);
  lv_obj_set_style_bg_color(lv_dropdown_get_list(dd), C_CARD, 0);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return dd;
}

// ---- Uhrzeit von Hand (ohne Internet)
static std::string clock_text() {
  if (!net_time_valid()) return "nicht gestellt";
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  char b[48];
  strftime(b, sizeof(b), "%d.%m.%Y, %H:%M", &tm);
  std::string s = b;
  const char *src = net_time_source();
  if (*src) s += std::string(" (") + src + ")";
  return s;
}

static lv_obj_t *s_cd_ov = nullptr;
static lv_obj_t *s_cd_r[5];
static const int CLOCK_YEAR0 = 2024;

static lv_obj_t *clock_roller(lv_obj_t *parent, const std::string &opts, int sel, int w) {
  lv_obj_t *r = lv_roller_create(parent);
  lv_roller_set_options(r, tr_lines(opts.c_str()).c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(r, 3);
  lv_roller_set_selected(r, sel, LV_ANIM_OFF);
  lv_obj_set_width(r, w);
  lv_obj_set_style_text_font(r, F_TITLE, 0);
  lv_obj_set_style_bg_color(r, C_FIELD, 0);
  lv_obj_set_style_text_color(r, C_TEXT2, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_bg_color(r, C_AMBER, LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, C_INK, LV_PART_SELECTED);
  return r;
}

static void clock_close() {
  if (s_cd_ov) lv_obj_del_async(s_cd_ov);   // wird aus einem Knopf darin aufgerufen
  s_cd_ov = nullptr;
}
static void clock_cancel_cb(lv_event_t *) { clock_close(); }
static void clock_ok_cb(lv_event_t *) {
  struct tm tm = {};
  tm.tm_mday = lv_roller_get_selected(s_cd_r[0]) + 1;
  tm.tm_mon = lv_roller_get_selected(s_cd_r[1]);
  tm.tm_year = CLOCK_YEAR0 + lv_roller_get_selected(s_cd_r[2]) - 1900;
  tm.tm_hour = lv_roller_get_selected(s_cd_r[3]);
  tm.tm_min = lv_roller_get_selected(s_cd_r[4]);
  tm.tm_isdst = -1;   // Sommerzeit ergibt sich aus der Zeitzone
  int day = tm.tm_mday;
  time_t t = mktime(&tm);   // Ortszeit -> UTC
  if (tm.tm_mday != day) {
    ui_toast("Diesen Tag gibt es in dem Monat nicht.");
    return;
  }
  net_set_time_manual(t);
  clock_close();
  ui_toast("Uhrzeit gestellt.");
  if (s_clock_val) lv_label_set_text(s_clock_val, clock_text().c_str());
}

static void clock_open_cb(lv_event_t *) {
  clock_close();
  time_t now = net_time_valid() ? time(nullptr) : 0;
  struct tm tm;
  localtime_r(&now, &tm);
  if (!now) { tm.tm_mday = 1; tm.tm_mon = 0; tm.tm_year = 126; tm.tm_hour = 12; tm.tm_min = 0; }
  s_cd_ov = lv_obj_create(lv_layer_top());
  ui_plain(s_cd_ov);
  lv_obj_set_size(s_cd_ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(s_cd_ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_cd_ov, LV_OPA_60, 0);
  lv_obj_add_flag(s_cd_ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *card = lv_obj_create(s_cd_ov);
  ui_plain(card);
  lv_obj_set_size(card, 820, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 26, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 28, 0);
  lv_obj_set_style_pad_row(card, 18, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);
  ui_label(card, "Datum und Uhrzeit", F_TITLE, C_TEXT);
  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 12, 0);
  auto range = [](int from, int to) {
    std::string s;
    char b[8];
    for (int i = from; i <= to; i++) {
      snprintf(b, sizeof(b), "%02d", i);
      s += b;
      if (i < to) s += "\n";
    }
    return s;
  };
  s_cd_r[0] = clock_roller(row, range(1, 31), tm.tm_mday - 1, 90);
  s_cd_r[1] = clock_roller(row, "Jan.\nFeb.\nMärz\nApril\nMai\nJuni\nJuli\nAug.\nSep.\nOkt.\nNov.\nDez.", tm.tm_mon, 130);
  int ysel = tm.tm_year + 1900 - CLOCK_YEAR0;
  s_cd_r[2] = clock_roller(row, range(CLOCK_YEAR0, CLOCK_YEAR0 + 25), ysel < 0 ? 0 : ysel > 25 ? 25 : ysel, 120);
  lv_obj_t *gap = ui_label(row, "", F_TITLE, C_TEXT);
  lv_obj_set_width(gap, 30);
  s_cd_r[3] = clock_roller(row, range(0, 23), tm.tm_hour, 90);
  ui_label(row, ":", F_TITLE, C_TEXT);
  s_cd_r[4] = clock_roller(row, range(0, 59), tm.tm_min, 90);
  lv_obj_t *br = lv_obj_create(card);
  ui_plain(br);
  lv_obj_set_size(br, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(br, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(br, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(br, 12, 0);
  ui_button(br, nullptr, "Abbrechen", false, clock_cancel_cb, nullptr);
  ui_button(br, ICON_CHECK, "Übernehmen", true, clock_ok_cb, nullptr);
}

static void build_display() {
  heading("Anzeige", nullptr);
  slider_block("Helligkeit", &s_bl_val, 5, 100, g_settings.brightness, bl_cb);
  char b[16];
  snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g_settings.brightness);
  lv_label_set_text(s_bl_val, b);

  subheading("Ruhezustand");
  int sel = 0;
  for (int i = 0; i < 7; i++)
    if (DIM_VALUES[i] == g_settings.dim_after) sel = i;
  dropdown_row("Starten nach", "Nie\n30 Sekunden\n1 Minute\n2 Minuten\n5 Minuten\n10 Minuten\n30 Minuten", sel,
               dim_after_cb);
  int mode = g_settings.saver;
  if (mode < 0 || mode > SAVER_DUEL) mode = SAVER_DIM;
  dropdown_row("Anzeige", "Abdunkeln\nSchwarz\nDunkle Uhr\nHüpfendes Logo\nBall-Duell", mode, saver_mode_cb);
  lv_obj_t *h = ui_label(s_content, SAVER_HINT[mode], F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));

  if (mode != SAVER_BLACK) {
    slider_block(mode == SAVER_DIM ? "Helligkeit im Ruhezustand" : "Helligkeit von Uhr und Logo", &s_dim_val,
                 mode == SAVER_DIM ? 0 : 6, 50, LV_MAX(g_settings.dim_level, mode == SAVER_DIM ? 0 : 6), dim_lvl_cb);
    dim_lvl_text(LV_MAX(g_settings.dim_level, mode == SAVER_DIM ? 0 : 6));
  }
  if (mode == SAVER_BOUNCE) {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_t *t = ui_label(r, "Eigenes Logo beim Abprallen umfärben", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_settings.saver_recolor) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, saver_rc_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  }
  lv_obj_t *row = button_row();
  ui_button(row, ICON_EYE, "Vorschau", false, saver_preview_cb, nullptr);

  subheading("Uhrzeit");
  lv_obj_t *cg = group();
  s_clock_val = ui_row(cg, "Datum und Uhrzeit", clock_text().c_str(), clock_open_cb, nullptr);
  lv_obj_t *ch = ui_label(s_content,
                          board_rtc_present()
                              ? "Mit Internet stellt sich die Uhr selbst. Ohne Internet hier von Hand stellen – der Uhrenchip "
                                "der Platine behält die Zeit auch ohne Strom."
                              : "Mit Internet stellt sich die Uhr selbst, ebenso beim Öffnen des Editors im Browser. Ohne beides "
                                "hier von Hand stellen. Ohne Uhrenchip geht die Zeit nach einem Stromausfall verloren.",
                          F_SMALL, C_TEXT2);
  lv_label_set_long_mode(ch, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ch, lv_pct(100));

  subheading("Netzwerkname");
  lv_obj_t *g = group();
  ui_row(g, "Gerätename im Netzwerk", g_settings.hostname.c_str(), hostname_cb, nullptr);
}

// ---------------------------------------------------------------- Geraete
void ui_rename_device(const std::string &key) {
  ControlView v;
  if (!z2m_get(key, v)) return;
  Layout lay = layout_get();
  InputOptions o;
  o.title = "Gerät umbenennen";
  o.hint = tr("Name in Zigbee2MQTT: ") + v.name +
           (v.ep.empty() ? "" : tr(" (Kanal ") + v.ep + tr("). Das Umbenennen in Zigbee2MQTT betrifft das ganze Gerät."));
  o.initial = layout_label(lay, v);
  o.max_len = 48;
  o.check_label = "Auch in Zigbee2MQTT umbenennen";
  if (shelly_is_key(key)) {   // Shelly: nur der Name auf dem Panel
    o.hint = tr("Shelly ") + v.model + " (" + v.ieee + tr("). Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  } else if (wled_is_key(key)) {   // WLED: ebenso
    o.hint = tr("WLED „") + v.name + "“ (" + v.description + tr("). Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  }
  std::string ieee = v.ieee, ep = v.ep, z2m_name = v.name;
  o.on_ok = [key, ieee, ep, z2m_name](const std::string &txt, bool also_z2m) {
    std::string t = txt;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    std::string def = z2m_name + (ep.empty() ? "" : " " + ep);
    if (also_z2m && !t.empty()) {
      std::string err = z2m_rename(ieee, t);
      if (!err.empty()) {
        ui_toast(err.c_str());
        return;
      }
      // Bei Geraeten ohne Kanaele genuegt danach der Zigbee2MQTT-Name
      layout_set_name(key, ep.empty() ? "" : t);
      ui_toast("Umbenennung an Zigbee2MQTT gesendet.");
    } else {
      layout_set_name(key, (t.empty() || t == def) ? "" : t);
      ui_toast("Name gespeichert.");
    }
    ui_settings_refresh();
  };
  ui_input_open(o);
}

static std::vector<std::string> s_dev_keys;

static void dev_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i < s_dev_keys.size()) ui_rename_device(s_dev_keys[i]);
}

static void arrange_missing_cb(lv_event_t *) {
  auto all = ui_arrange_items();
  if (all.empty()) {
    ui_toast("Noch keine Geräte von Zigbee2MQTT empfangen.");
    return;
  }
  layout_auto_arrange(all, true);
  ui_toast("Fehlende Geräte wurden ergänzt.");
}

static void arrange_all_cb(lv_event_t *) {
  ui_confirm("Alles neu anordnen?",
             "Alle Kacheln werden neu verteilt. Eigene Positionen und Größen gehen dabei verloren, Namen bleiben erhalten.",
             "Neu anordnen", true, [] {
               auto all = ui_arrange_items();
               layout_auto_arrange(all, false);
               ui_toast("Kacheln wurden neu verteilt.");
             });
}

static void build_sats();

static void build_devices() {
  heading("Geräte", "Tippe auf ein Gerät, um seinen Namen zu ändern. Anordnen, Icons und Größen gehen bequemer im Browser-Editor.");
  lv_obj_t *row = button_row();
  ui_button(row, ICON_WAND, "Fehlende ergänzen", true, arrange_missing_cb, nullptr);
  ui_button(row, ICON_GRID, "Alles neu anordnen", false, arrange_all_cb, nullptr);
  build_sats();
  if (panelsat_enabled()) subheading("Zigbee2MQTT");
  auto all = z2m_all();
  Layout lay = layout_get();
  s_dev_keys.clear();
  lv_obj_t *g = group();
  if (all.empty()) {
    lv_obj_t *l = ui_label(g, z2m_connected() ? z2m_diag_text() : "Keine Verbindung zum MQTT-Broker.", F_BODY, C_TEXT2);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    return;
  }
  for (size_t i = 0; i < all.size() && i < 80; i++) {
    const ControlView &v = all[i];
    s_dev_keys.push_back(v.key);
    std::string label = layout_label(lay, v);
    std::string def = v.name + (v.ep.empty() ? "" : " " + v.ep);
    std::string val = label != def ? def : v.model;
    ui_row(g, label.c_str(), val.c_str(), dev_row_cb, (void *)(intptr_t)i);
  }
}

// ---------------------------------------------------------------- Gasflaschen (Truma LevelControl)
static const uint16_t GAS_INTERVALS[] = {5, 10, 15, 30, 60};
static std::vector<TrumaFound> s_gas_found;

static void gas_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto list = truma_list();
  if (i < list.size()) ui_gas_open(list[i].key);
}

static void gas_found_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_gas_found.size()) return;
  TrumaFound f = s_gas_found[i];
  if (f.configured) {
    ui_toast("Dieser Sensor ist schon eingerichtet.");
    return;
  }
  InputOptions o;
  o.title = "Name der Gasflasche";
  o.hint = tr("Bluetooth-Adresse ") + f.mac + tr(". Zum Beispiel „Flasche links“ oder „Reserve“.");
  o.initial = tr("Gasflasche ") + std::to_string(truma_list().size() + 1);
  o.max_len = 32;
  o.on_ok = [f](const std::string &name, bool) {
    std::string err = truma_add(f.mac, f.addr_type, name);
    ui_toast(err.empty() ? "Eingerichtet. Die erste Messung folgt, sobald sich der Sensor meldet." : err.c_str());
    ui_settings_refresh();
  };
  ui_input_open(o);
}

static void gas_scan_cb(lv_event_t *) {
  if (truma_list().size() >= TRUMA_MAX_SENSORS) {
    ui_toast("Es sind schon vier Gasflaschen eingerichtet.");
    return;
  }
  truma_scan_start();
  ui_toast("Suche läuft. Der Sensor sendet nur alle paar Minuten – bitte etwas Geduld.");
}

static void gas_measure_cb(lv_event_t *) {
  truma_measure("");
  ui_toast("Messung angefordert.");
}

static void gas_interval_cb(lv_event_t *e) {
  truma_set_interval(GAS_INTERVALS[lv_dropdown_get_selected(lv_event_get_target(e))]);
}

// ---- Mopeka
static std::vector<MopekaFound> s_mop_found;

static void mop_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto list = mopeka_list();
  if (i < list.size()) ui_gas_open(list[i].key);
}

static void mop_found_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_mop_found.size()) return;
  MopekaFound f = s_mop_found[i];
  if (f.configured) {
    ui_toast("Dieser Sensor ist schon eingerichtet.");
    return;
  }
  bool gas = f.model & 0x100 || (f.model != 0x04 && f.model != 0x05 && f.model != 0x0A && f.model != 0x0B && f.model != 0x12);
  InputOptions o;
  o.title = gas ? "Name der Gasflasche" : "Name des Tanks";
  o.hint = tr("Mopeka ") + f.model_text + ", " + f.mac + tr(". Den Flaschentyp wählst du danach in der Detailansicht.");
  o.initial = (gas ? tr("Gasflasche ") : tr("Tank ")) + std::to_string(ui_gas_list().size() + 1);
  o.max_len = 32;
  o.on_ok = [f](const std::string &name, bool) {
    std::string err = mopeka_add(f.mac, name, 0);
    if (!err.empty()) {
      ui_toast(err.c_str());
    } else {
      ui_toast("Eingerichtet. Antippen, um den Flaschentyp zu wählen (vorgegeben: 11 kg).");
      ui_settings_refresh();
    }
  };
  ui_input_open(o);
}

static void mop_scan_cb(lv_event_t *) {
  if (mopeka_list().size() >= MOPEKA_MAX_SENSORS) {
    ui_toast("Es sind schon vier Mopeka-Sensoren eingerichtet.");
    return;
  }
  mopeka_discovery_start();
  ui_toast("Suche läuft. Am Sensor kurz die Sync-Taste drücken, dann ist er leicht zu erkennen.");
}

static std::vector<lv_obj_t *> s_mop_vals;
static uint32_t s_mop_sig = 0;

static std::string mop_value(const MopekaInfo &m) {
  GasView v;
  ui_gas_get(m.key, v);
  std::string val = m.has ? std::to_string(m.percent) + "\xE2\x80\xAF%" + (v.amount.empty() ? "" : ", " + v.amount) : m.status;
  if (m.has && m.status != "OK") val += std::string(", ") + tr(m.status);
  return val;
}

// Aufbau der Mopeka-Liste: aendert er sich, wird die Seite neu gebaut, sonst nur die Werte erneuert
static uint32_t mop_structure_sig() {
  uint32_t h = mopeka_discovery_running() ? 7 : 3;
  for (auto &m : mopeka_list()) {
    for (char c : m.mac) h = h * 31 + c;
    for (char c : m.name) h = h * 31 + c;
  }
  for (auto &f : mopeka_found()) {
    for (char c : f.mac) h = h * 31 + c;
    h = h * 31 + f.button * 2 + f.configured;
  }
  return h;
}

static void build_mopeka(const Layout &lay) {
  s_mop_vals.clear();
  s_mop_sig = mop_structure_sig();
  subheading("Mopeka");
  lv_obj_t *h = ui_label(s_content,
                         "Pro Check, Pro Plus, Pro Universal, H2O, TD40 und die älteren Standard-Sensoren. Sie senden ihre "
                         "Werte laufend – das Panel hört nur mit, die Mopeka-App bleibt nutzbar.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
  auto list = mopeka_list();
  lv_obj_t *g = group();
  if (list.empty()) ui_label(g, "Noch kein Mopeka-Sensor eingerichtet.", F_BODY, C_TEXT2);
  for (size_t i = 0; i < list.size(); i++) {
    GasView v;
    ui_gas_get(list[i].key, v);
    s_mop_vals.push_back(ui_row(g, ui_gas_label(lay, v).c_str(), mop_value(list[i]).c_str(), mop_row_cb, (void *)(intptr_t)i));
  }
  bool running = mopeka_discovery_running();
  lv_obj_t *row = button_row();
  ui_button(row, ICON_SEARCH, running ? "Suche läuft …" : "Mopeka suchen", !running, mop_scan_cb, nullptr);

  s_mop_found = mopeka_found();
  if (running || !s_mop_found.empty()) {
    subheading(running ? "Gefundene Mopeka-Sensoren (Suche läuft …)" : "Gefundene Mopeka-Sensoren");
    lv_obj_t *fg = group();
    if (s_mop_found.empty()) ui_label(fg, "Noch nichts gefunden. Sync-Taste am Sensor drücken.", F_BODY, C_TEXT2);
    for (size_t i = 0; i < s_mop_found.size(); i++) {
      const MopekaFound &f = s_mop_found[i];
      const char *q = f.rssi > -65 ? "stark" : f.rssi > -80 ? "mittel" : "schwach";
      char v[80];
      snprintf(v, sizeof(v), "%s%s%s", f.button ? tr("Taste gedrückt, ") : "", f.configured ? tr("eingerichtet, ") : "", q);
      ui_row(fg, (f.model_text + " " + f.mac).c_str(), v, mop_found_cb, (void *)(intptr_t)i);
    }
  }
}

// ---- Truma-Heizung (CP Plus / iNet X – je nach Firmware-Schalter)
static std::vector<std::pair<std::string, lv_obj_t *>> s_heat_vals;
static uint32_t s_heat_sig = 0;

static uint32_t heat_structure_sig() { return inetx_paired() * 2 + inetx_pairing() + 7; }

// ---- Hymer Connect (SCU per Bluetooth)
static lv_obj_t *s_hymer_val = nullptr;
static void hymer_pair_cb(lv_event_t *) {
  hymer_pair_start();
  ui_toast("Jetzt am Bedienteil im Fahrzeug „CONNECTION“ drücken – du hast 3 Minuten.");
}
static void hymer_forget_cb(lv_event_t *) {
  ui_confirm("Kopplung lösen?", "Das Panel trennt sich von der Hymer-SCU. Zum erneuten Verbinden musst du neu koppeln.", "Lösen", true,
             [] { hymer_forget(); });
}

static void heat_row_cb(lv_event_t *e) {
  const char *key = (const char *)lv_event_get_user_data(e);
  ui_heat_open(key);
}
static void inetx_pair_cb(lv_event_t *) {
  inetx_pair_start();
  ui_toast("Jetzt am iNet X „Gerät hinzufügen“ öffnen – du hast 3 Minuten.");
}
static void inetx_forget_cb(lv_event_t *) {
  ui_confirm("Kopplung lösen?", "Das Panel trennt sich vom iNet X. Zum erneuten Verbinden musst du neu koppeln.", "Lösen", true,
             [] { inetx_forget(); });
}
static void cpplus_clock_cb(lv_event_t *) {
  cpplus_sync_clock();
  ui_toast(time(nullptr) > 1700000000 ? "Die Uhrzeit geht beim nächsten Abruf ans CP Plus." : "Das Panel kennt die Uhrzeit noch nicht.");
}

static void build_heat() {
  s_heat_vals.clear();
  s_heat_sig = heat_structure_sig();
  if (!cpplus_enabled() && !alde_enabled() && !(inetx_enabled() && ble_proto_on(BLE_P_INETX))) return;
  subheading("Heizung");
  lv_obj_t *g = group();
  for (auto &h : ui_heat_list()) {
    bool cp = h.key == TRUMA_HEAT_KEY_CPPLUS, al = h.key == TRUMA_HEAT_KEY_ALDE;
    const char *key = cp ? TRUMA_HEAT_KEY_CPPLUS : al ? TRUMA_HEAT_KEY_ALDE : TRUMA_HEAT_KEY_INETX;
    std::string title = cp ? "Truma CP Plus (LIN-Bus)" : al ? h.source + tr(" (LIN-Bus)") : "Truma iNet X (Bluetooth)";
    lv_obj_t *v = ui_row(g, title.c_str(), h.status.c_str(), heat_row_cb, (void *)key);
    s_heat_vals.push_back({h.key, v});
  }
  lv_obj_t *row = button_row();
  if (cpplus_enabled()) ui_button(row, ICON_CLOCK, "Uhr ans CP Plus", false, cpplus_clock_cb, nullptr);
  if (inetx_enabled() && ble_proto_on(BLE_P_INETX)) {
    if (inetx_pairing()) ui_button(row, ICON_BLUETOOTH, "Kopplung läuft …", false, nullptr, nullptr);
    else ui_button(row, ICON_BLUETOOTH, inetx_paired() ? "iNet X neu koppeln" : "iNet X koppeln", !inetx_paired(), inetx_pair_cb, nullptr);
    if (inetx_paired()) ui_button(row, ICON_CLOSE, "Kopplung lösen", false, inetx_forget_cb, nullptr);
  }
  std::string hint;
  if (cpplus_enabled()) hint += tr("CP Plus: Nach dem Anschließen am CP Plus einmal „PR SET“ > Init ausführen, damit es das Panel als iNet Box anmeldet. ");
  if (alde_enabled()) hint += tr("Alde: LIN-Leitung an die gelbe RJ12-Buchse (Pin 4, Masse Pin 2). Modell, Pins und Baudrate stehen in platformio.ini unter [alde]. ");
  if (inetx_enabled() && ble_proto_on(BLE_P_INETX)) hint += "iNet X: Erst hier „Koppeln“ tippen, dann am Bedienteil „Gerät hinzufügen“ öffnen. Das iNet X merkt sich höchstens vier Geräte.";
  lv_obj_t *h = ui_label(s_content, hint.c_str(), F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
}

static std::vector<std::pair<uint8_t, lv_obj_t *>> s_hymer_rows;   // Punktnummer -> Wert-Label
static uint32_t s_hymer_sig = 0;

static std::string hymer_value_text(const SatPoint &p) {
  if (p.kind == SP_SWITCH || (p.kind == SP_INPUT && p.cls == SC_ONOFF)) return tr(p.on ? "An" : "Aus");
  if (p.kind == SP_INPUT) return tr(p.on ? "zu" : "offen");
  if (isnan(p.value)) return "–";
  char b[32];
  float a = fabsf(p.value);
  snprintf(b, sizeof(b), "%.*f %s", p.unit == "%" || a >= 100 ? 0 : a >= 10 ? 1 : 2, p.value, p.unit.c_str());
  std::string r = b;
  for (auto &c : r)
    if (c == '.') c = ',';
  return r;
}

// Aufbau der Seite aendert sich (Kopplung, neue Werte): dann neu zeichnen, sonst nur Texte
static uint32_t hymer_structure_sig() {
  uint32_t sig = 3 + ble_proto_on(BLE_P_HYMER) * 2 + hymer_paired() * 4 + hymer_pairing() * 8;
  for (auto &p : hymer_points()) sig = sig * 31 + p.idx;
  if (!hymer_paired())
    for (auto &c : hymer_candidates()) {   // Liste der gefundenen Geraete: neu zeichnen, wenn eins dazukommt
      sig = sig * 31 + c.score;
      for (char ch : c.mac + c.name) sig = sig * 31 + (uint8_t)ch;
    }
  return sig;
}

void ui_settings_tick_heat() {
  if (!s_scr || s_sec != SEC_GAS || ui_input_is_open()) return;
  if (heat_structure_sig() != s_heat_sig) {
    show_section(SEC_GAS);
    return;
  }
  for (auto &kv : s_heat_vals) {
    TrumaHeatInfo h;
    if (ui_heat_get(kv.first, h)) lv_label_set_text(kv.second, h.status.c_str());
  }
}

// ---- Auswahl der Bluetooth-Protokolle
static void proto_rebuild(void *) { show_section(s_sec); }
static void proto_cb(lv_event_t *e) {
  uint8_t p = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  ble_proto_set(p, on);
  if (p == BLE_P_INETX && !on) inetx_disconnect();
  if (p == BLE_P_HYMER && !on) hymer_disconnect();
  lv_async_call(proto_rebuild, nullptr);   // die Seite enthaelt den Schalter selbst
}
static void proto_switch(const char *title, const char *hint, uint8_t p) {
  lv_obj_t *r = lv_obj_create(s_content);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), 64);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, C_LINE, 0);
  lv_obj_t *t = ui_label(r, title, F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_obj_t *h = ui_label(r, hint, F_SMALL, C_TEXT2);
  lv_obj_align(h, LV_ALIGN_TOP_LEFT, 8, 36);
  lv_obj_t *sw = lv_switch_create(r);
  lv_obj_set_size(sw, 64, 34);
  lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (ble_proto_on(p)) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(sw, proto_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)p);
}

// ---------------------------------------------------------------- Satelliten
static uint32_t sat_struct_sig() {
  uint32_t h = 17;
  for (auto &n : panelsat_nodes()) {
    for (char c : n.id) h = h * 31 + c;
    h = h * 31 + (uint32_t)n.devices;
  }
  return h;
}

static std::string sat_summary(const SatNode &n) {
  std::string via;
  if (n.via & 1) via += "MQTT";
  if (n.via & 2) via += via.empty() ? "UDP" : ", UDP";
  if (n.via & 4) via += via.empty() ? "Bluetooth" : ", Bluetooth";
  char b[96];
  snprintf(b, sizeof(b), tr("%s · %u Gerät(e) · %s"), n.online ? tr("online") : tr("offline"), (unsigned)n.devices, via.c_str());
  return b;
}

static void sat_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto nodes = panelsat_nodes();
  if (i >= nodes.size()) return;
  const SatNode &n = nodes[i];
  if (n.ip.empty()) {
    ui_toast("Der Satellit hat keine IP-Adresse gemeldet – Einrichten über sein eigenes WLAN „SOC-Satellite-…“.");
    return;
  }
  char b[128];
  snprintf(b, sizeof(b), tr("Einrichten im Browser: http://%s/"), n.ip.c_str());
  ui_toast(b);
}

static void sat_pw_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Bluetooth-Passwort der Satelliten";
  o.hint = "Gilt für alle Satelliten. Am Satelliten dasselbe Passwort in seiner Weboberfläche eintragen – "
           "nur dann nimmt das Panel ihn per Bluetooth auf.";
  o.initial = panelsat_password();
  o.max_len = 32;
  o.on_ok = [](const std::string &t, bool) {
    panelsat_set_password(t);
    ui_toast("Passwort gespeichert.");
    show_section(SEC_DEVICES);
  };
  ui_input_open(o);
}

static void build_sats() {
  if (!panelsat_enabled()) return;
  subheading("Satelliten");
  lv_obj_t *h = ui_label(s_content,
                         "Eigene ESP32 mit Bluetooth-Sensoren (Truma LevelControl, Mopeka, Victron). Sie senden per MQTT, "
                         "UDP oder Bluetooth – ihre Geräte erscheinen hier und im Editor wie eigene.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
  proto_switch("Satelliten per Bluetooth", "Werte direkt per Funk empfangen, ohne WLAN", BLE_P_SAT);
  auto nodes = panelsat_nodes();
  lv_obj_t *g = group();
  if (ble_proto_on(BLE_P_SAT)) ui_row(g, "Bluetooth-Passwort", panelsat_password().c_str(), sat_pw_cb, nullptr);
  if (nodes.empty()) ui_label(g, "Noch kein Satellit gehört.", F_BODY, C_TEXT2);
  for (size_t i = 0; i < nodes.size(); i++)
    s_sat_vals.push_back(ui_row(g, nodes[i].name.c_str(), sat_summary(nodes[i]).c_str(), sat_row_cb, (void *)(intptr_t)i));
  s_sat_sig = sat_struct_sig();
}

void ui_settings_tick_sat() {
  if (!s_scr || s_sec != SEC_DEVICES || ui_input_is_open() || !panelsat_enabled()) return;
  if (sat_struct_sig() != s_sat_sig) {
    show_section(SEC_DEVICES);
    return;
  }
  auto nodes = panelsat_nodes();
  for (size_t i = 0; i < nodes.size() && i < s_sat_vals.size(); i++) lv_label_set_text(s_sat_vals[i], sat_summary(nodes[i]).c_str());
}

static void build_truma_lc(const Layout &lay) {
  subheading("Truma LevelControl");
  lv_obj_t *th = ui_label(s_content,
                          "Der Sensor erlaubt nur eine Verbindung gleichzeitig: Ist gerade die Truma-App, Home Assistant "
                          "oder der Victron GX verbunden, misst das Panel beim nächsten Durchgang.",
                          F_SMALL, C_TEXT2);
  lv_label_set_long_mode(th, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(th, lv_pct(100));
  auto list = truma_list();
  lv_obj_t *g = group();
  if (list.empty()) ui_label(g, "Noch keine Gasflasche eingerichtet.", F_BODY, C_TEXT2);
  for (size_t i = 0; i < list.size(); i++) {
    GasView t;
    ui_gas_get(list[i].key, t);
    std::string v = t.has ? std::to_string(t.percent) + "\xE2\x80\xAF%, " + t.amount + ", " + ui_age_text(t.age_s) : t.status;
    ui_row(g, ui_gas_label(lay, t).c_str(), v.c_str(), gas_row_cb, (void *)(intptr_t)i);
  }
  lv_obj_t *row = button_row();
  ui_button(row, ICON_SEARCH, "Truma suchen", true, gas_scan_cb, nullptr);
  if (!list.empty()) ui_button(row, ICON_REFRESH, "Alle jetzt messen", false, gas_measure_cb, nullptr);

  s_gas_found = truma_scan_results();
  if (truma_scan_running() || !s_gas_found.empty()) {
    subheading(truma_scan_running() ? "Gefundene Sensoren (Suche läuft …)" : "Gefundene Sensoren");
    lv_obj_t *fg = group();
    if (s_gas_found.empty()) ui_label(fg, "Noch nichts gefunden. Der Sensor muss in Reichweite sein.", F_BODY, C_TEXT2);
    for (size_t i = 0; i < s_gas_found.size(); i++) {
      const TrumaFound &f = s_gas_found[i];
      const char *q = f.rssi > -65 ? "stark" : f.rssi > -80 ? "mittel" : "schwach";
      char v[64];
      snprintf(v, sizeof(v), "%s%s", f.configured ? tr("eingerichtet, ") : "", q);
      ui_row(fg, (tr("LevelControl ") + f.mac).c_str(), v, gas_found_cb, (void *)(intptr_t)i);
    }
  }
  if (!list.empty()) {
    subheading("Messen");
    int sel = 1;
    for (int i = 0; i < 5; i++)
      if (GAS_INTERVALS[i] == truma_interval()) sel = i;
    dropdown_row("Abfrage alle", "5 Minuten\n10 Minuten\n15 Minuten\n30 Minuten\n60 Minuten", sel, gas_interval_cb);
    lv_obj_t *h = ui_label(s_content, "Häufigeres Messen kostet die Batterie des Sensors mehr.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
}

static void hymer_switch_cb(lv_event_t *e) {
  uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (!hymer_switch(idx, 2)) ui_toast("Hymer ist gerade nicht verbunden.");
}

static std::vector<std::string> s_hymer_cands;   // Adressen der angezeigten Geraete (Index = user_data)
static void hymer_cand_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_hymer_cands.size()) return;
  hymer_pair_with(s_hymer_cands[i]);
  ui_toast("Jetzt am Bedienteil im Fahrzeug „CONNECTION“ drücken – du hast 3 Minuten.");
}

static void build_hymer() {
  s_hymer_val = nullptr;
  s_hymer_rows.clear();
  s_hymer_sig = hymer_structure_sig();
  heading("Hymer Connect", "Licht, Batterie, Tanks und Schalter direkt von der SCU im Fahrzeug (Hymer, Bürstner, Dethleffs, "
                           "Eriba, LMC …) – per Bluetooth, ohne Cloud.");
  proto_switch("Hymer Connect", "SCU im Fahrzeug suchen und verbinden", BLE_P_HYMER);
  if (!ble_proto_on(BLE_P_HYMER)) return;
  subheading("Verbindung");
  lv_obj_t *g = group();
  s_hymer_val = ui_row(g, "SCU (Bluetooth)", hymer_status().c_str(), nullptr, nullptr);
  lv_obj_t *row = button_row();
  if (hymer_pairing()) ui_button(row, ICON_BLUETOOTH, "Kopplung läuft …", false, nullptr, nullptr);
  else ui_button(row, ICON_BLUETOOTH, hymer_paired() ? "Neu koppeln" : "Hymer koppeln", !hymer_paired(), hymer_pair_cb, nullptr);
  if (hymer_paired()) ui_button(row, ICON_CLOSE, "Kopplung lösen", false, hymer_forget_cb, nullptr);
  lv_obj_t *h = ui_label(s_content,
                         "Erst hier „Hymer koppeln“ tippen, dann am Bedienteil im Fahrzeug „CONNECTION“ drücken (3 Minuten Zeit). "
                         "Die Werte stehen danach im Editor wie die eines Satelliten namens „Hymer“ und lassen sich aufs "
                         "Panel ziehen.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
  if (!hymer_paired()) {
    // Gefundene Geraete: antippen = genau mit diesem koppeln
    hymer_browse();
    s_hymer_cands.clear();
    subheading("Gefundene Geräte");
    auto cands = hymer_candidates();
    if (cands.empty()) {
      ui_label(s_content, "Suche läuft … Die SCU muss in Reichweite und eingeschaltet sein.", F_SMALL, C_TEXT2);
    } else {
      lv_obj_t *cg = group();
      for (auto &c : cands) {
        char val[48];
        snprintf(val, sizeof(val), "%d dBm%s", c.rssi, c.score >= 2 ? " · Hymer" : "");
        std::string title = c.name.empty() ? c.mac : c.name + "  (" + c.mac + ")";
        ui_row(cg, title.c_str(), val, hymer_cand_cb, (void *)(intptr_t)s_hymer_cands.size());
        s_hymer_cands.push_back(c.mac);
      }
      lv_obj_t *h3 = ui_label(s_content, "Gerät antippen, dann am Bedienteil „CONNECTION“ drücken.", F_SMALL, C_TEXT2);
      lv_label_set_long_mode(h3, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(h3, lv_pct(100));
    }
  }
  auto pts = hymer_points();
  if (pts.empty()) return;
  subheading("Werte");
  lv_obj_t *vg = group();
  for (auto &p : pts) {
    bool sw = p.kind == SP_SWITCH;
    lv_obj_t *v = ui_row(vg, p.name.c_str(), hymer_value_text(p).c_str(), sw ? hymer_switch_cb : nullptr,
                         (void *)(intptr_t)p.idx);
    s_hymer_rows.push_back({p.idx, v});
  }
  lv_obj_t *h2 = ui_label(s_content, "Schalter und Lichter: Zeile antippen zum Umschalten.", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h2, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h2, lv_pct(100));
}

void ui_settings_tick_hymer() {
  if (!s_scr || s_sec != SEC_HYMER || ui_input_is_open()) return;
  if (hymer_structure_sig() != s_hymer_sig) {
    show_section(SEC_HYMER);
    return;
  }
  if (s_hymer_val) lv_label_set_text(s_hymer_val, hymer_status().c_str());
  if (!hymer_paired()) hymer_browse();   // solange die Seite offen ist, weiter nach Geraeten suchen
  auto pts = hymer_points();
  for (auto &r : s_hymer_rows)
    for (auto &p : pts)
      if (p.idx == r.first) lv_label_set_text(r.second, hymer_value_text(p).c_str());
}

static void build_gas() {
  heading("Gas und Heizung", "Füllstand und Heizung per Bluetooth. Wähle, welche Geräte das Panel auswerten soll – was aus ist, "
                             "wird nicht gesucht und belastet Funk und WLAN nicht.");
  proto_switch("Truma LevelControl", "Gasflaschen, bis zu vier", BLE_P_TRUMA);
  proto_switch("Mopeka", "Gas- und Tanksensoren, bis zu vier", BLE_P_MOPEKA);
  if (inetx_enabled()) proto_switch("Truma iNet X", "Heizung, Boiler und Klima über das Bedienteil", BLE_P_INETX);
  Layout lay = layout_get();
  if (ble_proto_on(BLE_P_TRUMA)) build_truma_lc(lay);
  if (ble_proto_on(BLE_P_MOPEKA)) build_mopeka(lay);
  build_heat();
}

void ui_settings_tick_gas() {
  if (!s_scr || s_sec != SEC_GAS || ui_input_is_open()) return;
  show_section(SEC_GAS);
}

void ui_settings_tick_mopeka() {
  if (!s_scr || s_sec != SEC_GAS || ui_input_is_open() || !ble_proto_on(BLE_P_MOPEKA)) return;
  if (mop_structure_sig() != s_mop_sig) {
    show_section(SEC_GAS);
    return;
  }
  auto list = mopeka_list();
  for (size_t i = 0; i < list.size() && i < s_mop_vals.size(); i++) lv_label_set_text(s_mop_vals[i], mop_value(list[i]).c_str());
}

// ---------------------------------------------------------------- Victron (Instant Readout)
static std::vector<VictronFound> s_vic_found;

// Aendert sich, wenn die Seite neu aufgebaut werden muss (Geraete, Suchergebnisse, Suche an/aus)
static uint32_t vic_struct_sig() {
  uint32_t h = victron_discovery_running() ? 7 : 3;
  auto mix = [&h](uint32_t x) { h = h * 31 + x; };
  for (auto &v : victron_list()) {
    for (char c : v.mac) mix(c);
    for (char c : v.name) mix(c);
    mix(v.kind);
  }
  for (auto &f : victron_found()) {
    for (char c : f.mac) mix(c);
    mix(f.configured);
    mix(f.name.size());
  }
  return h;
}

static void vic_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto list = victron_list();
  if (i < list.size()) ui_vic_open(list[i].key);
}

static void vic_found_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_vic_found.size()) return;
  VictronFound f = s_vic_found[i];
  if (f.configured) {
    ui_toast("Dieses Gerät ist schon eingerichtet.");
    return;
  }
  InputOptions o;
  o.title = tr("Schlüssel für ") + (f.name.empty() ? std::string(victron_kind_text(victron_kind_of_record(f.record))) : f.name);
  o.hint = "32 Zeichen aus VictronConnect: Einstellungen > Produktinfo > Instant Readout > Details anzeigen.";
  o.max_len = 47;
  o.on_ok = [f](const std::string &key, bool) {
    std::string err = victron_add(f.mac, key, f.name);
    ui_toast(err.empty() ? "Eingerichtet. Die Werte erscheinen in wenigen Sekunden." : err.c_str());
  };
  ui_input_open(o);
}

static void vic_scan_cb(lv_event_t *) {
  if (victron_list().size() >= VICTRON_MAX_DEVICES) {
    ui_toast("Es sind schon sechs Victron-Geräte eingerichtet.");
    return;
  }
  victron_discovery_start();
  ui_toast("Suche läuft. Victron-Geräte erscheinen meist nach wenigen Sekunden.");
  // Seite baut ui_settings_tick_victron neu auf (nicht hier: der Knopf wuerde im eigenen Ereignis geloescht)
}

// ---------------------------------------------------------------- Cerbo GX (MQTT)
static uint32_t gx_struct_sig() {
  return ((cerbo_configured() ? 5 : 3) * 31 + (cerbo_connected() ? 1 : 0)) * 2 + (gx_find_running() ? 1 : 0);
}

static void gx_find_cb(lv_event_t *) {
  if (gx_find_start()) ui_toast("Suche den Cerbo GX im Netz … (bis etwa 20 Sekunden)");
  else ui_toast(gx_find_running() ? "Suche läuft schon." : gx_find_status().c_str());
}

static void gx_host_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Adresse des Cerbo GX";
  o.hint = "IP-Adresse oder Name, z. B. 192.168.1.50. Leer = Cerbo GX aus. "
           "Am GX: Einstellungen > Dienste > MQTT einschalten.";
  o.initial = cerbo_config().host;
  o.max_len = 64;
  o.on_ok = [](const std::string &t, bool) {
    CerboConfig c = cerbo_config();
    c.host = t;
    while (!c.host.empty() && c.host.back() == ' ') c.host.pop_back();
    cerbo_set_config(c);
    ui_toast(c.host.empty() ? "Cerbo GX ausgeschaltet." : "Gespeichert, verbinde …");
    show_section(SEC_VICTRON);
  };
  ui_input_open(o);
}

static void gx_pass_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Passwort des Cerbo GX";
  o.hint = "Nur nötig, wenn am GX ein Passwort für das lokale Netz gesetzt ist. Leer = kein Passwort.";
  o.password = true;
  o.max_len = 64;
  o.on_ok = [](const std::string &t, bool) {
    CerboConfig c = cerbo_config();
    c.pass = t;
    cerbo_set_config(c);
    ui_toast("Gespeichert, verbinde neu …");
    show_section(SEC_VICTRON);
  };
  ui_input_open(o);
}

static void gx_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto list = cerbo_list();
  if (i < list.size()) ui_vic_open(list[i].key);
}

static void build_cerbo() {
  subheading("Cerbo GX (MQTT)");
  lv_obj_t *g = group();
  CerboConfig c = cerbo_config();
  ui_row(g, "Adresse", c.host.empty() ? "nicht gesetzt" : c.host.c_str(), gx_host_cb, nullptr);
  {
    std::string fs = gx_find_running() ? gx_find_status() : std::string("im Netz suchen und eintragen");
    ui_row(g, "Automatisch suchen", fs.c_str(), gx_find_running() ? nullptr : gx_find_cb, nullptr);
  }
  if (!c.host.empty()) {
    ui_row(g, "Passwort", c.pass.empty() ? "keins" : "••••••", gx_pass_cb, nullptr);
    s_gx_vals.push_back(ui_row(g, "Status", cerbo_status().c_str(), nullptr, nullptr, false));
    Layout lay = layout_get();
    auto list = cerbo_list();
    for (size_t i = 0; i < list.size(); i++)
      s_gx_vals.push_back(ui_row(g, ui_vic_label(lay, list[i]).c_str(), ui_vic_summary(list[i]).c_str(), gx_row_cb, (void *)(intptr_t)i));
  } else {
    lv_obj_t *h = ui_label(g, "Solar, Netz, Ladegerät, Batterie und DC-System vom Cerbo GX (Venus OS). "
                              "Am GX unter Einstellungen > Dienste > MQTT einschalten, dann „Automatisch suchen“ "
                              "oder die IP-Adresse von Hand eintragen. "
                              "Die Kacheln setzt du danach im Editor.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  s_gx_sig = gx_struct_sig();
}

static void build_victron() {
  heading("Victron",
          "SmartSolar, SmartShunt, BMV und Orion per Bluetooth („Instant Readout“). Das Panel hört nur mit – "
          "VictronConnect und ein GX bleiben ungestört. Pro Gerät brauchst du den Schlüssel aus VictronConnect: "
          "Einstellungen > Produktinfo > Instant Readout > Details anzeigen.");
  if (cerbo_enabled()) {
    build_cerbo();
    subheading("Bluetooth (Instant Readout)");
  }
  proto_switch("Victron auswerten", "Aus = das Panel sucht und lauscht nicht nach Victron-Geräten", BLE_P_VICTRON);
  if (!ble_proto_on(BLE_P_VICTRON)) {
    s_vic_sig = 0;
    return;
  }
  auto list = victron_list();
  Layout lay = layout_get();
  lv_obj_t *g = group();
  if (list.empty()) ui_label(g, "Noch kein Victron-Gerät eingerichtet.", F_BODY, C_TEXT2);
  for (size_t i = 0; i < list.size(); i++)
    s_vic_vals.push_back(ui_row(g, ui_vic_label(lay, list[i]).c_str(), ui_vic_summary(list[i]).c_str(), vic_row_cb, (void *)(intptr_t)i));
  lv_obj_t *row = button_row();
  bool running = victron_discovery_running();
  ui_button(row, ICON_SEARCH, running ? "Suche läuft …" : "Geräte suchen", true, vic_scan_cb, nullptr);

  s_vic_found = victron_found();
  if (running || !s_vic_found.empty()) {
    subheading(running ? "Gefundene Geräte (Suche läuft …)" : "Gefundene Geräte");
    lv_obj_t *fg = group();
    if (s_vic_found.empty()) ui_label(fg, "Noch nichts gefunden. Ist Bluetooth am Gerät eingeschaltet?", F_BODY, C_TEXT2);
    for (size_t i = 0; i < s_vic_found.size(); i++) {
      const VictronFound &f = s_vic_found[i];
      std::string title = f.name.empty() ? std::string(victron_kind_text(victron_kind_of_record(f.record))) + " " + f.mac : f.name;
      const char *q = f.rssi > -65 ? "stark" : f.rssi > -80 ? "mittel" : "schwach";
      char v[64];
      snprintf(v, sizeof(v), "%s%s", f.configured ? tr("eingerichtet, ") : "", q);
      ui_row(fg, title.c_str(), v, vic_found_cb, (void *)(intptr_t)i);
    }
  }
  s_vic_sig = vic_struct_sig();
}

void ui_settings_tick_victron() {
  if (!s_scr || s_sec != SEC_VICTRON || ui_input_is_open()) return;
  if (cerbo_enabled()) {
    if (gx_struct_sig() != s_gx_sig) {
      show_section(SEC_VICTRON);
      return;
    }
    if (!s_gx_vals.empty()) {
      lv_label_set_text(s_gx_vals[0], cerbo_status().c_str());
      auto gl = cerbo_list();
      for (size_t i = 0; i < gl.size() && i + 1 < s_gx_vals.size(); i++) lv_label_set_text(s_gx_vals[i + 1], ui_vic_summary(gl[i]).c_str());
    }
  }
  if (!ble_proto_on(BLE_P_VICTRON)) return;
  if (vic_struct_sig() != s_vic_sig) {
    show_section(SEC_VICTRON);
    return;
  }
  auto list = victron_list();
  for (size_t i = 0; i < list.size() && i < s_vic_vals.size(); i++)
    lv_label_set_text(s_vic_vals[i], ui_vic_summary(list[i]).c_str());
}

// ---------------------------------------------------------------- Info
static const char *INFO_TITLES[] = {"Firmware", "IP-Adresse", "WLAN-Signal", "Zigbee2MQTT", "Geräteliste",
                                    "Touch", "Akku", "Freier Speicher", "Laufzeit", "Panel-WLAN"};
static const int INFO_N = 10;

static void info_fill() {
  if (!s_info[0]) return;
  char b[96];
  String ip = net_editor_ip();
  if (s_info_url) {
    snprintf(b, sizeof(b), "http://%s/", ip.length() ? ip.c_str() : "…");
    lv_label_set_text(s_info_url, b);
    snprintf(b, sizeof(b), tr("oder http://%s.local/"), g_settings.hostname.c_str());
    lv_label_set_text(s_info_url2, b);
  }
  lv_label_set_text(s_info[0], FW_VERSION);
  lv_label_set_text(s_info[1], ip.length() ? ip.c_str() : "nicht verbunden");
  if (net_connected()) snprintf(b, sizeof(b), "%d dBm", net_rssi());
  else snprintf(b, sizeof(b), "–");
  lv_label_set_text(s_info[2], b);
  snprintf(b, sizeof(b), "%s%s", z2m_status_text(), z2m_connected() && !z2m_bridge_online() ? tr(" (Bridge offline)") : "");
  lv_label_set_text(s_info[3], b);
  lv_label_set_text(s_info[4], z2m_diag_text());
  lv_label_set_text(s_info[5], board_touch_info());
  PowerInfo pw = power_get();
  if (pw.has_battery) snprintf(b, sizeof(b), "%d %%, %.2f V, %s", pw.percent, pw.volt, tr(power_src_text(pw.src)));
  else snprintf(b, sizeof(b), "%s", power_src_text(pw.src));
  lv_label_set_text(s_info[6], b);
  snprintf(b, sizeof(b), "%u KB intern, %u KB PSRAM", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  lv_label_set_text(s_info[7], b);
  uint32_t s = millis() / 1000;
  snprintf(b, sizeof(b), "%u d %02u:%02u", (unsigned)(s / 86400), (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60));
  lv_label_set_text(s_info[8], b);
  if (net_ap_active()) snprintf(b, sizeof(b), "%s, %d verbunden", g_settings.ap_ssid.c_str(), net_ap_clients());
  else snprintf(b, sizeof(b), "aus");
  lv_label_set_text(s_info[9], b);
}

static void refresh_cb(lv_event_t *) {
  z2m_refresh_states();
  ui_toast("Zustände werden neu abgefragt.");
}

static void reboot_cb(lv_event_t *) {
  ui_confirm("Panel neu starten?", "Das dauert etwa zehn Sekunden.", "Neu starten", false, [] {
    delay(200);
    ESP.restart();
  });
}

static void build_info() {
  heading("Info", nullptr);
  lv_obj_t *card = group();
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_pad_all(card, 20, 0);
  lv_obj_set_style_pad_row(card, 6, 0);
  ui_label(card, "Layout-Editor im Browser", F_SMALL, C_TEXT2);
  s_info_url = ui_label(card, "", F_TITLE, C_AMBER);
  s_info_url2 = ui_label(card, "", F_SMALL, C_TEXT2);
  lv_obj_t *g = group();
  for (int i = 0; i < INFO_N; i++) s_info[i] = ui_row(g, INFO_TITLES[i], "", nullptr, nullptr, false);
  lv_obj_t *row = button_row();
  ui_button(row, ICON_REFRESH, "Zustände neu abfragen", false, refresh_cb, nullptr);
  ui_button(row, ICON_POWER, "Neu starten", false, reboot_cb, nullptr);
  info_fill();
}

// ---------------------------------------------------------------- Sicherung (SD-Karte)
static std::vector<BackupInfo> s_bk;

static std::string size_text(uint64_t b) {
  char t[32];
  if (b >= 1000000000ull) snprintf(t, sizeof(t), "%.1f GB", b / 1e9);
  else snprintf(t, sizeof(t), "%u MB", (unsigned)(b / 1000000ull));
  for (char *p = t; *p; p++)
    if (*p == '.') *p = ',';
  return t;
}

// Kurz warten, damit Hinweis/Dialog-Schliessen gezeichnet wird, dann die (blockierende) Arbeit erledigen
static void later(std::function<void()> fn) {
  auto *f = new std::function<void()>(fn);
  lv_timer_t *t = lv_timer_create(
      [](lv_timer_t *t) {
        auto *f = (std::function<void()> *)t->user_data;
        (*f)();
        delete f;
        lv_timer_del(t);
      },
      120, f);
  (void)t;   // loescht sich im Callback selbst
}

static void bk_create_cb(lv_event_t *) {
  ui_toast("Sicherung läuft …");
  later([] {
    std::string name, msg;
    if (backup_create(name, msg)) ui_toast("Sicherung liegt auf der SD-Karte.");
    else ui_toast(msg.c_str());
    if (s_scr && s_sec == SEC_BACKUP) show_section(SEC_BACKUP);
  });
}

static void bk_restore_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_bk.size()) return;
  if (!s_bk[i].complete) {
    ui_toast("Diese Sicherung ist unvollständig.");
    return;
  }
  std::string name = s_bk[i].name;
  std::string text = tr("Layout, Einstellungen und Hintergründe werden durch den Stand vom ") + s_bk[i].title +
                     tr(" ersetzt. Danach startet das Panel neu.");
  ui_confirm("Sicherung einspielen?", text.c_str(), "Einspielen", true, [name] {
    ui_toast("Sicherung wird eingespielt …");
    later([name] {
      std::string msg;
      if (!backup_restore(name, msg)) {
        ui_toast(msg.c_str());
        return;
      }
      delay(300);
      ESP.restart();
    });
  });
}

static void bk_delete_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_bk.size()) return;
  std::string name = s_bk[i].name;
  std::string text = tr("Die Sicherung vom ") + s_bk[i].title + tr(" wird von der SD-Karte entfernt.");
  ui_confirm("Sicherung löschen?", text.c_str(), "Löschen", true, [name] {
    std::string msg;
    if (!backup_delete(name, msg)) ui_toast(msg.c_str());
    if (s_scr && s_sec == SEC_BACKUP) show_section(SEC_BACKUP);
  });
}

static void build_backup() {
  heading("Sicherung",
          "Sichert Layout, Namen, alle Einstellungen, Gasflaschen, Victron-Geräte, Logo und Hintergründe auf die "
          "microSD-Karte. Achtung: WLAN- und MQTT-Passwörter stehen dort lesbar.");
  lv_obj_t *g = group();
  if (!sd_ok()) {
    ui_row(g, "SD-Karte", sd_status(), nullptr, nullptr, false);
    lv_obj_t *l = ui_label(s_content, "Karte (FAT32 oder exFAT) einstecken und das Panel neu starten.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    s_bk.clear();
    return;
  }
  std::string cap = size_text(sd_free()) + tr(" frei von ") + size_text(sd_total());
  ui_row(g, "SD-Karte", cap.c_str(), nullptr, nullptr, false);
  lv_obj_t *row = button_row();
  ui_button(row, ICON_CHECK, "Jetzt sichern", true, bk_create_cb, nullptr);

  ui_label(s_content, "Tippen zum Einspielen", F_SMALL, C_TEXT2);
  s_bk = backup_list();
  lv_obj_t *list = group();
  if (s_bk.empty()) {
    ui_label(list, "Noch keine Sicherung auf der Karte.", F_BODY, C_TEXT2);
    return;
  }
  for (size_t i = 0; i < s_bk.size() && i < 30; i++) {
    const BackupInfo &b = s_bk[i];
    char v[64];
    if (b.complete) snprintf(v, sizeof(v), b.bg == 1 ? tr("%d Hintergrund") : tr("%d Hintergründe"), b.bg);
    else snprintf(v, sizeof(v), "unvollständig");
    lv_obj_t *r = nullptr;
    lv_obj_t *val = ui_row(list, b.title.c_str(), v, bk_restore_cb, (void *)(intptr_t)i, false, &r);
    lv_obj_set_width(val, 200);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -72, 0);
    lv_obj_t *del = ui_icon_button(r, ICON_CLOSE, bk_delete_cb, (void *)(intptr_t)i, 48);
    lv_obj_align(del, LV_ALIGN_RIGHT_MID, -6, 0);
  }
}

// ---------------------------------------------------------------- Rahmen
static const char *NAV_ICONS[SEC_COUNT] = {ICON_WIFI, ICON_LINK, ICON_MONITOR, ICON_LIST, ICON_GAS, ICON_SUN, ICON_HOUSE, ICON_LOCK, ICON_INFO};
static const char *NAV_TITLES[SEC_COUNT] = {"Netzwerk", "MQTT", "Anzeige", "Geräte", "Gas/Heizung", "Victron", "Hymer", "Sicherung", "Info"};

static void show_section(int sec) {
  s_sec = sec;
  for (int i = 0; i < SEC_COUNT; i++) {
    bool a = i == sec;
    lv_obj_set_style_bg_opa(s_nav_items[i], a ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_nav_items[i], a ? 4 : 0, 0);
    lv_obj_t *ic = lv_obj_get_child(s_nav_items[i], 0);
    lv_obj_t *tx = lv_obj_get_child(s_nav_items[i], 1);
    lv_obj_set_style_text_color(ic, a ? C_AMBER : C_TEXT2, 0);
    lv_obj_set_style_text_color(tx, a ? C_TEXT : C_TEXT2, 0);
  }
  lv_obj_clean(s_content);
  reset_dynamic();
  lv_obj_scroll_to_y(s_content, 0, LV_ANIM_OFF);
  switch (sec) {
    case SEC_WIFI: build_wifi(); break;
    case SEC_MQTT: build_mqtt(); break;
    case SEC_DISPLAY: build_display(); break;
    case SEC_DEVICES: build_devices(); break;
    case SEC_GAS: build_gas(); break;
    case SEC_VICTRON: build_victron(); break;
    case SEC_HYMER: build_hymer(); break;
    case SEC_BACKUP: build_backup(); break;
    default: build_info(); break;
  }
}

static void nav_cb(lv_event_t *e) { show_section((int)(intptr_t)lv_event_get_user_data(e)); }
static void back_cb(lv_event_t *) { ui_settings_close(); }

void ui_settings_open(int section) {
  ui_detail_close();
  if (s_scr) {
    show_section(section);
    return;
  }
  s_scr = lv_obj_create(nullptr);
  ui_plain(s_scr);
  lv_obj_set_style_bg_color(s_scr, C_BG, 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

  lv_obj_t *nav = lv_obj_create(s_scr);
  ui_plain(nav);
  lv_obj_set_size(nav, 260, LV_VER_RES);
  lv_obj_set_style_bg_color(nav, C_BAR, 0);
  lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_width(nav, 1, 0);
  lv_obj_set_style_border_color(nav, C_LINE, 0);
  lv_obj_set_style_pad_all(nav, 12, 0);
  lv_obj_set_style_pad_row(nav, 4, 0);
  lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *back = lv_obj_create(nav);
  ui_plain(back);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(back, lv_pct(100), 52);
  lv_obj_set_style_radius(back, 12, 0);
  lv_obj_set_style_bg_color(back, C_FIELD, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_t *bi = ui_icon(back, ICON_LEFT, F_ICON_S, C_TEXT);
  lv_obj_align(bi, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_t *bt = ui_label(back, "Zurück", F_BODY, C_TEXT);
  lv_obj_align(bt, LV_ALIGN_LEFT_MID, 46, 0);
  lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *sp = lv_obj_create(nav);
  ui_plain(sp);
  lv_obj_set_size(sp, 10, 6);

  for (int i = 0; i < SEC_COUNT; i++) {
    lv_obj_t *it = lv_obj_create(nav);
    ui_plain(it);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(it, lv_pct(100), 50);   // 9 Punkte muessen in 600 px passen
    lv_obj_set_style_radius(it, 12, 0);
    lv_obj_set_style_bg_color(it, C_FIELD, 0);
    lv_obj_set_style_border_side(it, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(it, C_AMBER, 0);
    lv_obj_t *ic = ui_icon(it, NAV_ICONS[i], F_ICON_S, C_TEXT2);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t *tx = ui_label(it, NAV_TITLES[i], F_BODY, C_TEXT2);
    lv_obj_align(tx, LV_ALIGN_LEFT_MID, 52, 0);
    lv_obj_add_event_cb(it, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    s_nav_items[i] = it;
    if (i == SEC_HYMER && !hymer_enabled()) lv_obj_add_flag(it, LV_OBJ_FLAG_HIDDEN);   // nicht eingebaut
  }

  s_content = lv_obj_create(s_scr);
  ui_plain(s_content);
  lv_obj_set_pos(s_content, 260, 0);
  lv_obj_set_size(s_content, LV_HOR_RES - 260, LV_VER_RES);
  lv_obj_set_style_pad_all(s_content, 32, 0);
  lv_obj_set_style_pad_row(s_content, 14, 0);
  lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(s_content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_ACTIVE);

  lv_scr_load(s_scr);
  show_section(section);
}

void ui_settings_close() {
  if (!s_scr) return;
  lv_obj_t *old = s_scr;
  s_scr = nullptr;
  s_content = nullptr;
  reset_dynamic();
  lv_scr_load(ui_main_screen());
  lv_obj_del_async(old);
}

void ui_settings_refresh() {
  if (!s_scr) return;
  if (s_sec == SEC_DEVICES || s_sec == SEC_DISPLAY || s_sec == SEC_INFO) show_section(s_sec);
}

void ui_settings_tick() {
  if (!s_scr) return;
  if (s_wifi_status) {
    lv_label_set_text(s_wifi_status, net_status_text());
    String ip = net_ip();
    String txt = ip.length() ? String(tr("IP-Adresse ")) + ip : String("");
    if (net_ap_active()) txt += String(txt.length() ? ", " : "") + "Panel-WLAN 192.168.4.1";
    lv_label_set_text(s_wifi_ip, txt.c_str());
  }
  if (s_scan_box && net_scan_take(s_scan)) fill_scan();
  if (s_web_val) lv_label_set_text(s_web_val, web_text().c_str());
  if (s_mqtt_status) lv_label_set_text(s_mqtt_status, z2m_status_text());
  if (s_clock_val) lv_label_set_text(s_clock_val, clock_text().c_str());
  info_fill();
  ui_settings_tick_victron();  // Ende der Suche, Alter der Werte
}
