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
#include "../bms.h"
#include "../mopeka.h"

static void offer_tile(const std::string &key, const std::string &name);   // nach dem Hinzufuegen Kachel anbieten
static std::string lower_mac(std::string s);
#include "../panelsat.h"
#include "../truma_cpplus.h"
#include "../truma_inetx.h"
#include "../hymer.h"
#include "../hobby.h"
#include "../votronic.h"
#include "../web_server.h"
#include "../alde.h"
#include "../power.h"
#include "../backup.h"
#include "../shelly.h"
#include "../wled.h"
#include "../sdcard.h"
#include "../panelsat_pair.h"
#include "ui_remote.h"
#include "../zb_direct.h"
#include "../rules.h"
#include "../mqtt_broker.h"
#include "../app.h"
#include <Arduino.h>
#include <functional>
#include <vector>
#include "esp_heap_caps.h"
#include "../dbglog.h"
#include "../tailscale.h"
#include "../shelly_ble.h"
#include "../mqtt_disco.h"
#include "../sdcard.h"

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
static lv_obj_t *s_dbg_state = nullptr;   // Info: Stand des Debug-Logs
static lv_obj_t *s_ts_val = nullptr;      // Info: Tailscale
static std::vector<lv_obj_t *> s_vic_vals;
static uint32_t s_vic_sig = 0;
static std::vector<lv_obj_t *> s_sat_vals;   // Satelliten in "Geraete"
static std::vector<lv_obj_t *> s_rd_vals;    // Zusatzdisplays in "Geraete"
static lv_obj_t *s_pair_status = nullptr;     // Stand der Satelliten-Einrichtung in "Geraete"
static uint32_t s_sat_sig = 0;
static std::vector<lv_obj_t *> s_gx_vals;     // Cerbo GX: Status + fuenf Werte
static uint32_t s_gx_sig = 0;
static lv_obj_t *s_clock_val = nullptr;
static lv_obj_t *s_web_val = nullptr;   // Netzwerk-Seite: Zustand des Webinterface

bool ui_settings_is_open() { return s_scr != nullptr; }
static void show_section(int sec);
static void info_fill();

static void reset_dynamic() {
  s_wifi_status = s_wifi_ip = s_scan_box = nullptr;
  s_mqtt_status = nullptr;
  s_bl_val = s_dim_val = nullptr;
  s_clock_val = nullptr;
  s_web_val = nullptr;
  s_info_url = s_info_url2 = nullptr;
  s_dbg_state = nullptr;
  s_ts_val = nullptr;
  for (auto &p : s_mqtt_vals) p = nullptr;
  for (auto &p : s_info) p = nullptr;
  s_vic_vals.clear();
  s_gx_vals.clear();
  s_sat_vals.clear();
  s_rd_vals.clear();
  s_pair_status = nullptr;
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
#if SOC_VICTRON_EDITION
static const char *MODE_TEXT[4] = {"Das Panel hängt im WLAN – z. B. dort, wo auch der Cerbo GX ist.",
                                   "Das Panel öffnet ein eigenes WLAN – Editor per Handy, kein Heimnetz nötig.",
                                   "Eigenes WLAN plus Camp-WLAN mit Internet-Weiterleitung.",
                                   "Kein WLAN: mehr Speicher, Bluetooth läuft ungestört. Kein Cerbo, kein Editor."};
#else
static const char *MODE_TEXT[4] = {"Panel und Hub hängen beide im Camp-WLAN.",
                                   "Panel öffnet ein eigenes WLAN für den Hub. Kein Heimnetz nötig.",
                                   "Eigenes WLAN für den Hub plus Camp-WLAN mit Internet-Weiterleitung.",
                                   "Kein WLAN: mehr Speicher, Bluetooth läuft ungestört. Kein MQTT, kein Editor."};
#endif

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
#if SOC_VICTRON_EDITION
    o.hint = "Unter diesem Namen findest du das Panel im WLAN. Änderung gilt nach einem Neustart.";
#else
    o.hint = "Unter diesem Namen findet der Hub das Panel. Änderung gilt nach einem Neustart.";
#endif
    o.initial = g_settings.ap_ssid.c_str();
    o.max_len = 32;
  } else if (f == 1) {
    o.title = "Passwort des Panel-WLANs";
#if SOC_VICTRON_EDITION
    o.hint = "8 bis 63 Zeichen. Änderung gilt nach einem Neustart.";
#else
    o.hint = "8 bis 63 Zeichen. Änderung gilt nach einem Neustart – der Hub muss dann neu verbunden werden.";
#endif
    o.initial = g_settings.ap_pass.c_str();
    o.max_len = 63;
  } else {
    o.title = "Funkkanal (1–13)";
#if SOC_VICTRON_EDITION
    o.hint = "Funkkanal des Panel-WLANs (1–13).";
#else
    o.hint = "Möglichst weit weg vom Zigbee-Kanal: bei Zigbee 11 eignet sich WLAN 13, bei Zigbee 25 WLAN 1.";
#endif
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
static void web_pw_reset_cb(lv_event_t *) {
  ui_confirm("Zugangspasswort zurücksetzen?",
             "Das Passwort für Editor und Weboberfläche ist danach wieder „panelsat“. Beim nächsten Öffnen im Browser "
             "muss ein neues festgelegt werden. Angemeldete Browser werden abgemeldet.",
             "Zurücksetzen", true, [] {
               web_password_reset();
               ui_toast("Zurückgesetzt – im Browser ein neues Passwort festlegen.");
               ui_settings_refresh();
             });
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
  bool pw = web_password_set();
  ui_row(g, "Zugangspasswort", pw ? "festgelegt – antippen zum Zurücksetzen" : "noch nicht festgelegt (beim ersten Öffnen im Browser)",
         pw ? web_pw_reset_cb : nullptr, nullptr);
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
#if WITH_TAILSCALE
  // Fernzugriff: Adresse im Tailnet (einschalten/anmelden im Browser-Editor unter Netzwerk)
  subheading("Tailscale (Beta)");
  lv_obj_t *tg = group();
  s_ts_val = ui_row(tg, "Adresse im Tailnet", "", nullptr, nullptr, false);
  info_fill();
#endif
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
             g_settings.net_mode == NET_APSTA ? tr(" (folgt dem Camp-WLAN)") : "");
    ui_row(g, "Name", g_settings.ap_ssid.c_str(), ap_edit_cb, (void *)(intptr_t)0);
    ui_row(g, "Passwort", g_settings.ap_pass.c_str(), ap_edit_cb, (void *)(intptr_t)1);
    ui_row(g, "Kanal", ch, ap_edit_cb, (void *)(intptr_t)2);
#if SOC_VICTRON_EDITION
    lv_obj_t *h = ui_label(s_content, "Mit dem Handy dieses WLAN wählen – der Editor ist dann unter http://192.168.4.1/ erreichbar.",
                           F_SMALL, C_TEXT2);
#else
    lv_obj_t *h = ui_label(s_content,
                           "Am SMHUB Nano unter Einstellungen > WLAN dieses Netz wählen (2,4 GHz). Der Editor ist dann "
                           "unter http://192.168.4.1/ erreichbar, wenn dein Handy ebenfalls verbunden ist.",
                           F_SMALL, C_TEXT2);
#endif
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  if (g_settings.net_mode == NET_STA || g_settings.net_mode == NET_APSTA) {
    subheading("Camp-WLAN");
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
#if !SOC_VICTRON_EDITION
      o.hint = "Wie in Zigbee2MQTT unter mqtt.base_topic – meistens zigbee2mqtt.";
#endif
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
#if WITH_BROKER
// Eingebauter Broker: Panel ist selbst der MQTT-Server (Port 1883) und verbindet sich mit sich selbst
static void broker_on_cb(lv_event_t *e) {
  Settings st = settings_copy();
  st.broker_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  if (st.broker_on) st.mqtt_on = true;
  settings_apply(st);
  broker_apply();
  z2m_restart();
  ui_toast(st.broker_on ? "Eingebauter Broker an (Port 1883)." : "Eingebauter Broker aus.");
  lv_async_call(mqtt_rebuild, nullptr);
}
#endif
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

#if WITH_MQTT_DISCO
// ESPHome, Tasmota & Co.: Geraete aus MQTT-Discovery (gilt sofort, nicht ueber "Speichern")
static void mqd_sw_cb(lv_event_t *e) {
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  mqd_set(on, mqd_prefix());
  ui_toast(on ? "Geräte aus MQTT-Discovery werden gesucht." : "MQTT-Discovery ist aus.");
}
static void mqd_section() {
  subheading("ESPHome, Tasmota & Co.");
  lv_obj_t *r = lv_obj_create(s_content);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), 64);
  lv_obj_t *t = ui_label(r, "Geräte aus MQTT-Discovery anzeigen", F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t *sw = lv_switch_create(r);
  lv_obj_set_size(sw, 64, 34);
  lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (mqd_enabled()) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(sw, mqd_sw_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  std::string hint = tr("Geräte, die sich am Broker per Home-Assistant-Discovery melden (ESPHome mit „mqtt:“, Tasmota …), "
                        "erscheinen wie Satelliten in der Geräteliste – eigene Gruppe je Gerät.");
  if (mqd_enabled()) {
    MqdInfo mi = mqd_info();
    char b[96];
    snprintf(b, sizeof(b), tr(" Gefunden: %u Werte von %u Geräten."), (unsigned)mi.entities, (unsigned)mi.devices);
    hint += b;
  }
  lv_obj_t *h = ui_label(s_content, hint.c_str(), F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
}
#else
static void mqd_section() {}
#endif

static void build_mqtt() {
#if SOC_VICTRON_EDITION
  heading("Shelly (MQTT)", "Shelly-Schalter und -Dimmer über einen MQTT-Broker. In der Shelly-App bzw. Weboberfläche "
                           "MQTT einschalten und denselben Broker eintragen – die Shellys erscheinen dann als Geräte.");
#else
  heading("MQTT-Broker", "Derselbe Broker, den auch Zigbee2MQTT benutzt – beim SMHUB Nano der eingebaute Mosquitto. "
                         "Dort unter MQTT „Allow External“ einschalten, damit das Panel sich verbinden darf.");
#endif
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
#if WITH_BROKER
  {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_t *t = ui_label(r, "Eingebauter Broker (dieses Panel)", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_draft.broker_on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, broker_on_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    if (s_draft.broker_on) {
      String ip = net_ip();
      char b[200];
#if SOC_VICTRON_EDITION
      snprintf(b, sizeof(b), tr("Shellys mit diesem Panel verbinden: Server %s, Port 1883%s."), ip.length() ? ip.c_str() : "…",
               s_draft.broker_user.isEmpty() ? "" : tr(", Benutzer und Passwort wie im Editor"));
#else
      snprintf(b, sizeof(b), tr("Geräte mit diesem Panel verbinden: Server %s, Port 1883%s."), ip.length() ? ip.c_str() : "…",
               s_draft.broker_user.isEmpty() ? "" : tr(", Benutzer und Passwort wie im Editor"));
#endif
      lv_obj_t *h = ui_label(s_content, b, F_SMALL, C_TEXT2);
      lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(h, lv_pct(100));
      s_mqtt_status = ui_label(s_content, z2m_status_text(), F_BODY, C_TEXT2);
      mqd_section();
      return;   // eigene Broker-Felder gelten solange nicht
    }
  }
#endif
  if (!s_draft.mqtt_on) {
#if SOC_VICTRON_EDITION
    lv_obj_t *h = ui_label(s_content, "MQTT ist aus: keine Shellys. Victron, Cerbo GX, Gas und Akkus laufen weiter.",
                           F_SMALL, C_TEXT2);
#else
    lv_obj_t *h = ui_label(s_content,
                           "MQTT ist aus: keine Zigbee-Geräte, kein Cerbo über den Broker. Bluetooth-Geräte und "
                           "Satelliten per UDP/Bluetooth laufen weiter. Spart Speicher und Funkzeit.",
                           F_SMALL, C_TEXT2);
#endif
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
    return;
  }
  if (!SOC_VICTRON_EDITION && (s_draft.net_mode == NET_AP || s_draft.net_mode == NET_APSTA)) {
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
  for (int i = 0; i < (SOC_VICTRON_EDITION ? 4 : 5); i++)   // Edition: kein Basis-Topic (gehoert zu Zigbee2MQTT)
    s_mqtt_vals[i] = ui_row(g, MQTT_TITLES[i], mqtt_value(i).c_str(), mqtt_row_cb, (void *)(intptr_t)i);
  if (!SOC_VICTRON_EDITION && (s_draft.net_mode == NET_AP || s_draft.net_mode == NET_APSTA) && s_draft.mqtt_auto)
    lv_label_set_text(s_mqtt_vals[0], "automatisch");
  s_mqtt_status = ui_label(s_content, z2m_status_text(), F_BODY, C_TEXT2);
  lv_obj_t *row = button_row();
  ui_button(row, ICON_CHECK, "Speichern und verbinden", true, mqtt_save_cb, nullptr);
  mqd_section();
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
#if !SOC_VICTRON_EDITION
  o.hint = tr("Name in Zigbee2MQTT: ") + v.name + tr(". Der Name gilt nur für dieses Panel.");
#endif
  o.initial = layout_label(lay, v);
  o.max_len = 48;
  if (shelly_is_key(key)) {   // Shelly: nur der Name auf dem Panel
    o.hint = tr("Shelly ") + v.model + " (" + v.ieee + tr("). Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  } else if (shb_is_key(key)) {   // Shelly per Bluetooth: ebenso
    o.hint = tr("Shelly per Bluetooth ") + v.model + tr(". Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  } else if (wled_is_key(key)) {   // WLED: ebenso
    o.hint = tr("WLED „") + v.name + "“ (" + v.description + tr("). Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  }
#if WITH_ZB_DIRECT
  else if (zbd_is_key(key)) {   // Zigbee direkt (H2-Modul): Name nur auf dem Panel
    o.hint = tr("Zigbee-Gerät ") + v.vendor + " " + v.model + tr(". Der Name gilt nur für dieses Panel.");
    o.check_label.clear();
  }
#endif
  // Umbenennen gilt immer nur fuer dieses Panel – Zigbee2MQTT/Home Assistant bleiben unberuehrt (Wunsch 2026-11-24)
  std::string ep = v.ep, z2m_name = v.name;
  o.on_ok = [key, ep, z2m_name](const std::string &txt, bool) {
    std::string t = txt;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    std::string def = z2m_name + (ep.empty() ? "" : " " + ep);
    layout_set_name(key, (t.empty() || t == def) ? "" : t);
    ui_toast("Name gespeichert.");
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
#if SOC_VICTRON_EDITION
    ui_toast("Noch keine Geräte mit Werten.");
#else
    ui_toast("Noch keine Geräte von Zigbee2MQTT empfangen.");
#endif
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
  auto all = z2m_all();
  Layout lay = layout_get();
  s_dev_keys.clear();
  if (all.empty()) {
#if SOC_VICTRON_EDITION
    subheading("Shelly");
    lv_obj_t *l = ui_label(group(), g_settings.mqtt_on ? (z2m_connected() ? "Noch keine Shellys gefunden." : "Keine Verbindung zum MQTT-Broker.")
                                                       : "Shellys: unter „Shelly (MQTT)“ einschalten.", F_BODY, C_TEXT2);
#else
    subheading("Zigbee2MQTT");
    lv_obj_t *l = ui_label(group(), z2m_connected() ? z2m_diag_text() : "Keine Verbindung zum MQTT-Broker.", F_BODY, C_TEXT2);
#endif
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    return;
  }
  // Nach Herkunft gruppiert: Zigbee2MQTT, eingebautes Zigbee, Shelly (MQTT), Shelly per Bluetooth, WLED
  enum { G_Z2M, G_ZBD, G_SHELLY, G_SHB, G_WLED, G_N };
  static const char *const G_TITLE[G_N] = {"Zigbee2MQTT", "Zigbee (eingebaut)", "Shelly (MQTT)", "Shelly (Bluetooth)", "WLED"};
  auto group_of = [](const std::string &k) {
    if (zbd_is_key(k)) return (int)G_ZBD;
    if (shb_is_key(k)) return (int)G_SHB;
    if (shelly_is_key(k)) return (int)G_SHELLY;
    if (wled_is_key(k)) return (int)G_WLED;
    return (int)G_Z2M;
  };
  size_t shown = 0;
  for (int gi = 0; gi < G_N; gi++) {
    lv_obj_t *g = nullptr;
    for (size_t i = 0; i < all.size() && shown < 80; i++) {
      const ControlView &v = all[i];
      if (group_of(v.key) != gi) continue;
      if (!g) {
        subheading(SOC_VICTRON_EDITION && gi == G_SHELLY ? "Shelly" : G_TITLE[gi]);
        g = group();
      }
      size_t idx = s_dev_keys.size();
      s_dev_keys.push_back(v.key);
      std::string label = layout_label(lay, v);
      std::string def = v.name + (v.ep.empty() ? "" : " " + v.ep);
      std::string val = label != def ? def : v.model;
      ui_row(g, label.c_str(), val.c_str(), dev_row_cb, (void *)(intptr_t)idx);
      shown++;
    }
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
    if (err.empty()) offer_tile(TRUMA_KEY_PREFIX + lower_mac(f.mac), name);
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
      offer_tile(MOPEKA_KEY_PREFIX + lower_mac(f.mac), name);
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

#if WITH_HYMER
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

#endif
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

#if WITH_SATELLITES
// ---------------------------------------------------------------- Satelliten
static uint32_t sat_struct_sig() {
  uint32_t h = 17;
  for (auto &c : satpair_candidates())   // Satelliten im Einrichtungsmodus (verschwinden nach 15 s ohne Signal)
    for (char ch : c.mac) h = h * 31 + ch;
  h = h * 31 + (uint32_t)satpair_state();
  h = h * 31 + (uint32_t)satpair_searching();
  for (auto &n : panelsat_nodes()) {
    for (char c : n.id) h = h * 31 + c;
    h = h * 31 + (uint32_t)n.devices;
  }
  for (auto &d : ui_remote_list())   // Zusatzdisplays: neue/entfernte bauen die Seite neu
    for (char c : d.id) h = h * 31 + c;
  return h;
}

static std::string sat_summary(const SatNode &n) {
  std::string via;
  if (n.via & 1) via += "MQTT";
  if (n.via & 2) via += via.empty() ? "UDP" : ", UDP";
  if (n.via & 4) via += via.empty() ? "Bluetooth" : ", Bluetooth";
  char b[96];
  snprintf(b, sizeof(b), tr("%s · %u Gerät(e) · %s"), n.online ? tr("online") : tr("offline"), (unsigned)n.devices, via.c_str());
  std::string s = b;
  if (!n.ip.empty()) s += " · " + n.ip;   // Adresse gleich sichtbar (Weboberflaeche des Satelliten)
  return s;
}

// Satellit im Einrichtungsmodus antippen: WLAN-Daten und Passwort uebertragen (panelsat_pair.cpp)
static std::vector<std::string> s_pair_macs;
static void sat_pair_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_pair_macs.size()) return;
  std::string err = satpair_start(s_pair_macs[i]);
  if (!err.empty()) ui_toast(err.c_str());
  else show_section(SEC_DEVICES);
}

static void sat_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto nodes = panelsat_nodes();
  if (i >= nodes.size()) return;
  const SatNode &n = nodes[i];
  // Antippen: wo seine Weboberflaeche liegt, und "Entfernen" (aus der Liste nehmen)
  std::string txt;
  if (!n.ip.empty()) txt = std::string(tr("Weboberfläche: http://")) + n.ip + "/";
  else if (n.via & 4) txt = tr("Per Bluetooth verbunden (ohne IP). Seine Weboberfläche: WLAN „SOC-Satellite-…“, dann 4.3.2.1.");
  else txt = tr("Keine IP gemeldet – Weboberfläche über sein eigenes WLAN „SOC-Satellite-…“.");
  txt += tr("\n\nEntfernen nimmt ihn aus der Liste, seine Kacheln bleiben leer. Sendet er weiter, taucht er wieder auf – "
            "dann am Satelliten die Sendewege abschalten oder ein anderes Bluetooth-Passwort setzen.");
  std::string id = n.id;
  ui_confirm(n.name.c_str(), txt.c_str(), "Entfernen", true, [id] {
    panelsat_forget(id);
    show_section(SEC_DEVICES);
  });
}

// Zweizeiliger Eintrag: Name oben, Stand darunter (einzeilig ueberlappten lange Namen und lange Texte)
static lv_obj_t *row2(lv_obj_t *g, const char *title, const char *sub, lv_event_cb_t cb, void *ud) {
  lv_obj_t *r = lv_obj_create(g);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(r, 10, 0);
  lv_obj_set_style_pad_hor(r, 12, 0);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, C_LINE, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(r, 2, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) {
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(r, C_FIELD, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, ud);
  }
  lv_obj_t *t = ui_label(r, title, F_BODY, C_TEXT);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  lv_obj_set_width(t, lv_pct(100));
  lv_obj_t *v = ui_label(r, sub, F_SMALL, C_TEXT2);
  lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(v, lv_pct(100));
  return v;
}

// Zusatzdisplays (ui_remote.cpp): antippen zeigt Adresse und "Entfernen"
static std::vector<std::string> s_rd_ids;
static std::string rd_summary(const RdListItem &d) {
  std::string via = std::string(d.udp ? "UDP" : "") + (d.udp && d.mqtt ? ", " : "") + (d.mqtt ? "MQTT" : "");
  char b[96];
  snprintf(b, sizeof(b), tr("%s · %u Kachel(n)%s%s"), d.online ? tr("verbunden") : tr("nicht verbunden"), (unsigned)d.tiles,
           via.empty() ? "" : " · ", via.c_str());
  std::string s = b;
  if (!d.ip.empty()) s += " · " + d.ip;
  return s;
}
static void rd_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  auto l = ui_remote_list();
  if (i >= l.size()) return;
  std::string txt = l[i].ip.empty() ? std::string(tr("Noch keine IP gemeldet.")) : std::string(tr("Weboberfläche: http://")) + l[i].ip + "/";
  txt += tr("\n\nWelche Kacheln es zeigt: im Editor dieses Panels (Reiter Zusatzdisplays) oder in seiner eigenen "
            "Weboberfläche. Entfernen nimmt es aus der Liste – solange es läuft, meldet es sich wieder an.");
  std::string id = l[i].id;
  ui_confirm(l[i].name.c_str(), txt.c_str(), "Entfernen", true, [id] {
    ui_remote_forget(id);
    show_section(SEC_DEVICES);
  });
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

static void sat_search_cb(lv_event_t *) {
  if (g_settings.net_mode == NET_OFF) {
    ui_toast("Satelliten brauchen WLAN – am Panel ist „WLAN aus“ eingestellt.");
    return;
  }
  satpair_search_start();
  show_section(SEC_DEVICES);
}

static std::string sat_search_text() {
  uint32_t left = satpair_search_left();
  char b[120];
  snprintf(b, sizeof(b), tr("Suche läuft – noch %u:%02u. Am Satelliten die BOOT-Taste 3 s halten (oder 3× schnell aus/ein)."),
           (unsigned)(left / 60), (unsigned)(left % 60));
  return b;
}

static void build_sats() {
  if (!panelsat_enabled()) return;
  subheading("Satelliten");
  lv_obj_t *h = ui_label(s_content,
                         "Eigene ESP32 mit Sensoren und Relais im Fahrzeug. Sie senden per UDP im WLAN direkt ans Panel "
                         "(oder über einen MQTT-Broker) – ihre Geräte erscheinen hier und im Editor wie eigene.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
  auto nodes = panelsat_nodes();
  lv_obj_t *g = group();
  ui_row(g, "Satelliten-Passwort", panelsat_password().c_str(), sat_pw_cb, nullptr);
  if (nodes.empty()) ui_label(g, "Noch kein Satellit gehört.", F_BODY, C_TEXT2);
  for (size_t i = 0; i < nodes.size(); i++)
    s_sat_vals.push_back(row2(g, nodes[i].name.c_str(), sat_summary(nodes[i]).c_str(), sat_row_cb, (void *)(intptr_t)i));
  // Zusatzdisplays (kleine Displays mit Kacheln dieses Panels) – auch hier sichtbar
  auto rds = ui_remote_list();
  s_rd_ids.clear();
  s_rd_vals.clear();
  if (!rds.empty()) {
    subheading("Zusatzdisplays");
    lv_obj_t *rg = group();
    for (size_t i = 0; i < rds.size(); i++) {
      s_rd_vals.push_back(row2(rg, rds[i].name.c_str(), rd_summary(rds[i]).c_str(), rd_row_cb, (void *)(intptr_t)i));
      s_rd_ids.push_back(rds[i].id);
    }
  }
  // Neuen Satelliten hinzufuegen: 3 Minuten per Bluetooth suchen; am Satelliten BOOT 3 s halten -> erscheint hier,
  // antippen uebertraegt WLAN-Zugang (je nach Betriebsart) und Passwort, danach spricht er per UDP
  s_pair_macs.clear();
  s_pair_status = nullptr;
  bool searching = satpair_searching();
  int st = satpair_state();
  if (!searching) {
    lv_obj_t *row = button_row();
    ui_button(row, ICON_PLUS, "Neuen Satelliten hinzufügen", true, sat_search_cb, nullptr);
  }
  if (searching || st != 0) {
    lv_obj_t *pg = group();
    for (auto &c : satpair_candidates()) {
      char v[40];
      snprintf(v, sizeof(v), tr("%d dBm · antippen zum Einrichten"), c.rssi);
      row2(pg, c.name.c_str(), v, sat_pair_cb, (void *)(intptr_t)s_pair_macs.size());
      s_pair_macs.push_back(c.mac);
    }
    std::string t = st != 0 ? satpair_status() : sat_search_text();
    s_pair_status = row2(pg, st != 0 ? "Stand" : "Suche", t.c_str(), nullptr, nullptr);
  }
  if (g_settings.net_mode == NET_OFF) {
    lv_obj_t *ph = ui_label(s_content, "Am Panel ist „WLAN aus“ eingestellt – Satelliten brauchen WLAN.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(ph, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ph, lv_pct(100));
  }
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
  auto rds = ui_remote_list();
  for (size_t i = 0; i < rds.size() && i < s_rd_vals.size(); i++) lv_label_set_text(s_rd_vals[i], rd_summary(rds[i]).c_str());
  if (s_pair_status)
    lv_label_set_text(s_pair_status, (satpair_state() != 0 ? satpair_status() : sat_search_text()).c_str());
}

#else
static void build_sats() {}
void ui_settings_tick_sat() {}
#endif
static void build_truma_lc(const Layout &lay) {
  subheading("Truma LevelControl");
  lv_obj_t *th = ui_label(s_content,
#if SOC_VICTRON_EDITION
                          "Der Sensor erlaubt nur eine Verbindung gleichzeitig: Ist gerade die Truma-App oder der Victron "
                          "GX verbunden, misst das Panel beim nächsten Durchgang.",
#else
                          "Der Sensor erlaubt nur eine Verbindung gleichzeitig: Ist gerade die Truma-App, Home Assistant "
                          "oder der Victron GX verbunden, misst das Panel beim nächsten Durchgang.",
#endif
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

#if WITH_HYMER
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

#else
static void build_hymer() {}
void ui_settings_tick_hymer() {}
#endif
#if WITH_HOBBY
// ---------------------------------------------------------------- Hobby (HobbyConnect-Box per Bluetooth, CI-BUS per Kabel)
// Teil der Seite "Fahrzeug" (SEC_HYMER), unter Hymer Connect
static lv_obj_t *s_hb_ble_val = nullptr, *s_hb_ser_val = nullptr;
static std::vector<std::pair<uint8_t, lv_obj_t *>> s_hb_rows;
static std::vector<HobbyCand> s_hb_cands;
static uint32_t s_hb_sig = 0;

static std::string hb_value_text(const SatPoint &p) {
  if (p.kind == SP_SWITCH || (p.kind == SP_INPUT && p.cls == SC_ONOFF)) return tr(p.on ? "An" : "Aus");
  if (p.kind == SP_INPUT) return tr(p.on ? "zu" : "offen");
  if (isnan(p.value)) return "–";
  char b[32];
  float a = fabsf(p.value);
  snprintf(b, sizeof(b), "%.*f %s", p.unit == "%" || p.unit.empty() || a >= 100 ? 0 : 1, p.value, p.unit.c_str());
  std::string r = b;
#if UI_DECIMAL_COMMA
  for (auto &c : r)
    if (c == '.') c = ',';
#endif
  return r;
}

static uint32_t hobby_structure_sig() {
  HobbyCfg c = hobby_cfg();
  uint32_t sig = 7 + c.ble_on * 2 + c.uart_on * 4 + c.uart * 8 + (c.rx + 2) * 64 + (c.tx + 2) * 8192;
  for (char ch : c.mac) sig = sig * 31 + (uint8_t)ch;
  for (auto &p : hobby_points()) sig = sig * 31 + p.idx;
  if (c.ble_on && c.mac.empty())
    for (auto &x : hobby_candidates())
      for (char ch : x.mac) sig = sig * 31 + (uint8_t)ch;
  return sig;
}

static void hb_rebuild(void *) {
  if (!s_scr || s_sec != SEC_HYMER) return;
  lv_coord_t y = lv_obj_get_scroll_y(s_content);
  show_section(SEC_HYMER);
  lv_obj_update_layout(s_content);
  lv_obj_scroll_to_y(s_content, y, LV_ANIM_OFF);
}

static void hb_cfg_change(const std::function<void(HobbyCfg &)> &f) {
  HobbyCfg c = hobby_cfg();
  f(c);
  hobby_set_cfg(c);
  lv_async_call(hb_rebuild, nullptr);
}

static void hb_sw_cb(lv_event_t *e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  hb_cfg_change([&](HobbyCfg &c) {
    if (which == 0) c.ble_on = on;
    else c.uart_on = on;
  });
}

static void hb_switch_row(const char *title, const char *hint, bool on, int which) {
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
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(sw, hb_sw_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)which);
}

static void hb_cand_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_hb_cands.size()) return;
  HobbyCand x = s_hb_cands[i];
  hb_cfg_change([&](HobbyCfg &c) {
    c.mac = x.mac;
    c.mac_type = x.type;
  });
  ui_toast("Box gewählt – verbinde …");
}

static void hb_forget_cb(lv_event_t *) {
  ui_confirm("Andere Box wählen", "Die gemerkte HobbyConnect-Box vergessen und neu suchen?", "Vergessen", false,
             [] { hb_cfg_change([](HobbyCfg &c) { c.mac.clear(); }); });
}

static void hb_uart_cb(lv_event_t *) {
  hb_cfg_change([](HobbyCfg &c) {
    int n = hobby_uart_count();
    c.uart = c.uart + 1 >= n ? 1 : c.uart + 1;   // UART0 = Konsole/USB, nicht anbieten
  });
}

static void hb_pin_cb(lv_event_t *e) {
  bool rx = (intptr_t)lv_event_get_user_data(e) == 0;
  HobbyCfg c = hobby_cfg();
  InputOptions o;
  o.title = rx ? "RX-Pin (GPIO)" : "TX-Pin (GPIO)";
  o.hint = rx ? "GPIO, an dem die Sendeleitung des RS232-Wandlers (TXD) hängt; leer = keiner"
              : "GPIO zur Empfangsleitung des RS232-Wandlers (RXD); leer = keiner";
  int8_t cur = rx ? c.rx : c.tx;
  o.initial = cur >= 0 ? std::to_string(cur) : "";
  o.numeric = true;
  o.max_len = 2;
  o.on_ok = [rx](const std::string &t, bool) {
    int v = t.empty() ? -1 : atoi(t.c_str());
    if (v < -1 || v > 54) v = -1;
    hb_cfg_change([&](HobbyCfg &c) {
      if (rx) c.rx = (int8_t)v;
      else c.tx = (int8_t)v;
    });
  };
  ui_input_open(o);
}

static void hb_point_cb(lv_event_t *e) {
  uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  for (auto &p : hobby_points())
    if (p.idx == idx && !hobby_switch(idx, !p.on)) ui_toast("Gerade nicht schaltbar (nicht verbunden; Hauptschalter nur über das Kabel).");
}

static void hb_tiles_cb(lv_event_t *) {
  std::vector<ControlView> add;
  VictronInfo v;
  if (hobby_battery(v)) {
    ControlView cv;
    cv.key = v.key;
    cv.name = v.name;
    add.push_back(cv);
  }
  for (auto &p : panelsat_point_list())
    if (p.sat == HOBBY_SAT_ID) {
      ControlView cv;
      cv.key = p.key;
      cv.name = p.name;
      add.push_back(cv);
    }
  if (add.empty()) {
    ui_toast("Noch keine Werte – erst verbinden.");
    return;
  }
  layout_auto_arrange(add, true);
  ui_toast("Kacheln angelegt (vorhandene bleiben).");
}

static void build_hobby() {
  s_hb_ble_val = s_hb_ser_val = nullptr;
  s_hb_rows.clear();
  s_hb_sig = hobby_structure_sig();
  HobbyCfg c = hobby_cfg();
  heading("Hobby", "Wohnwagen und Reisemobile von Hobby: Licht, Bordbatterie, Wasser, Heizung, Kühlschrank und Klima – "
                   "per Bluetooth über die HobbyConnect-Box oder per Kabel direkt am Steuergerät (LSG, CI-BUS).");
  hb_switch_row("HobbyConnect-Box", "Box im Fahrzeug per Bluetooth verbinden", c.ble_on, 0);
  if (c.ble_on) {
    lv_obj_t *g = group();
    s_hb_ble_val = ui_row(g, "Box (Bluetooth)", hobby_status().c_str(), nullptr, nullptr, false);
    if (!c.mac.empty()) {
      ui_row(g, "Adresse", c.mac.c_str(), hb_forget_cb, nullptr);
    } else {
      hobby_search(60000);
      s_hb_cands = hobby_candidates();
      if (s_hb_cands.empty()) {
        lv_obj_t *h = ui_label(s_content, "Suche läuft … Die Box muss eingeschaltet und in Reichweite sein; die Hobby-App "
                                          "auf dem Handy vorher trennen.", F_SMALL, C_TEXT2);
        lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(h, lv_pct(100));
      } else {
        lv_obj_t *cg = group();
        for (size_t i = 0; i < s_hb_cands.size(); i++) {
          auto &x = s_hb_cands[i];
          char val[24];
          snprintf(val, sizeof(val), "%d dBm", x.rssi);
          std::string t = (x.name.empty() ? std::string("HobbyConnect") : x.name) + "  (" + x.mac + ")";
          ui_row(cg, t.c_str(), val, hb_cand_cb, (void *)(intptr_t)i);
        }
      }
    }
  }
  hb_switch_row("CI-BUS am LSG (Kabel)", "Ersetzt die Box: RS232-Wandler an einer seriellen Schnittstelle", c.uart_on, 1);
  if (c.uart_on) {
    lv_obj_t *g = group();
    s_hb_ser_val = ui_row(g, "Status", hobby_uart_status().c_str(), nullptr, nullptr, false);
    char b[16];
    snprintf(b, sizeof(b), "UART%d", c.uart);
    ui_row(g, "Schnittstelle", b, hb_uart_cb, nullptr);
    std::string rx = c.rx >= 0 ? "GPIO " + std::to_string(c.rx) : std::string(tr("nicht gesetzt"));
    std::string tx = c.tx >= 0 ? "GPIO " + std::to_string(c.tx) : std::string(tr("nicht gesetzt"));
    ui_row(g, "RX-Pin", rx.c_str(), hb_pin_cb, (void *)0);
    ui_row(g, "TX-Pin", tx.c_str(), hb_pin_cb, (void *)1);
    lv_obj_t *h = ui_label(s_content,
                           "Das LSG spricht RS232 (±12 V): nur über einen RS232-TTL-Wandler (z. B. MAX3232, 3,3 V) "
                           "anschließen, 115200 Baud. Waveshare 7\": UART-Stiftleiste (GPIO 15/16). Die Nummern der Werte "
                           "holt das Panel selbst vom LSG.",
                           F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
#if WITH_ZB_DIRECT
    if (c.uart == (int)ZBD_UART && zbd_enabled()) {
      char w[96];
      snprintf(w, sizeof(w), tr("Achtung: UART%d benutzt das Zigbee-Modul – bitte eine andere wählen."), (int)ZBD_UART);
      lv_obj_t *wl = ui_label(s_content, "", F_SMALL, C_RED);
      (lv_label_set_text)(wl, w);
    }
#endif
  }
  auto pts = hobby_points();
  if (pts.empty()) return;
  subheading("Werte");
  lv_obj_t *vg = group();
  for (auto &p : pts) {
    bool sw = p.kind == SP_SWITCH;
    lv_obj_t *v = ui_row(vg, p.name.c_str(), hb_value_text(p).c_str(), sw ? hb_point_cb : nullptr, (void *)(intptr_t)p.idx, sw);
    s_hb_rows.push_back({p.idx, v});
  }
  lv_obj_t *row = button_row();
  ui_button(row, ICON_PLUS, "Kacheln anlegen", true, hb_tiles_cb, nullptr);
  lv_obj_t *h2 = ui_label(s_content, "Schalter und Lichter: Zeile antippen. Im Editor stehen die Werte in der "
                                     "Geräteliste unter „Hobby“, die Bordbatterie als Batteriewächter.", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h2, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h2, lv_pct(100));
}

void ui_settings_tick_hobby() {
  if (!s_scr || s_sec != SEC_HYMER || ui_input_is_open()) return;
  if (hobby_structure_sig() != s_hb_sig) {
    hb_rebuild(nullptr);
    return;
  }
  HobbyCfg c = hobby_cfg();
  if (c.ble_on && c.mac.empty()) hobby_search(60000);   // solange die Seite offen ist, weiter suchen
  if (s_hb_ble_val) lv_label_set_text(s_hb_ble_val, hobby_status().c_str());
  if (s_hb_ser_val) lv_label_set_text(s_hb_ser_val, hobby_uart_status().c_str());
  auto pts = hobby_points();
  for (auto &r : s_hb_rows)
    for (auto &p : pts)
      if (p.idx == r.first) lv_label_set_text(r.second, hb_value_text(p).c_str());
}
#else
static void build_hobby() {}
void ui_settings_tick_hobby() {}
#endif
#if WITH_VOTRONIC
// ---------------------------------------------------------------- Votronic (S-BC per Bluetooth, Display-Link per Kabel)
static lv_obj_t *s_vt_ble_val = nullptr, *s_vt_ser_val = nullptr;
static std::vector<lv_obj_t *> s_vt_rows;
static std::vector<VotronicCand> s_vt_cands;
static uint32_t s_vt_sig = 0;

static uint32_t votronic_structure_sig() {
  VotronicCfg c = votronic_cfg();
  uint32_t sig = 11 + c.ble_on * 2 + c.uart_on * 4 + c.uart * 8 + (c.rx + 2) * 64;
  for (char ch : c.mac) sig = sig * 31 + (uint8_t)ch;
  for (char ch : c.pin) sig = sig * 31 + (uint8_t)ch;
  for (auto &r : votronic_rows()) {
    for (char ch : r.dev) sig = sig * 31 + (uint8_t)ch;
    for (char ch : r.name) sig = sig * 31 + (uint8_t)ch;
  }
  if (c.ble_on && c.mac.empty())
    for (auto &x : votronic_candidates())
      for (char ch : x.mac) sig = sig * 31 + (uint8_t)ch;
  return sig;
}

static void vt_rebuild(void *) {
  if (!s_scr || s_sec != SEC_VOTRONIC) return;
  lv_coord_t y = lv_obj_get_scroll_y(s_content);
  show_section(SEC_VOTRONIC);
  lv_obj_update_layout(s_content);
  lv_obj_scroll_to_y(s_content, y, LV_ANIM_OFF);
}

static void vt_cfg_change(const std::function<void(VotronicCfg &)> &f) {
  VotronicCfg c = votronic_cfg();
  f(c);
  votronic_set_cfg(c);
  lv_async_call(vt_rebuild, nullptr);
}

static void vt_sw_cb(lv_event_t *e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  vt_cfg_change([&](VotronicCfg &c) {
    if (which == 0) c.ble_on = on;
    else c.uart_on = on;
  });
}

static void vt_switch_row(const char *title, const char *hint, bool on, int which) {
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
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(sw, vt_sw_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)which);
}

static void vt_hint(const char *txt) {
  lv_obj_t *h = ui_label(s_content, txt, F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
}

static void vt_cand_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_vt_cands.size()) return;
  VotronicCand x = s_vt_cands[i];
  vt_cfg_change([&](VotronicCfg &c) {
    c.mac = x.mac;
    c.mac_type = x.type;
  });
  ui_toast("S-BC gewählt – verbinde und kopple …");
}

static void vt_forget_cb(lv_event_t *) {
  ui_confirm("Anderen S-BC wählen", "Den gemerkten Bluetooth Connector vergessen und neu suchen?", "Vergessen", false,
             [] { vt_cfg_change([](VotronicCfg &c) { c.mac.clear(); }); });
}

static void vt_mac_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Adresse des S-BC";
  o.hint = "Bluetooth-Adresse, z. B. aa:bb:cc:dd:ee:ff – wenn die Suche den S-BC nicht findet";
  o.initial = votronic_cfg().mac;
  o.max_len = 17;
  o.on_ok = [](const std::string &t, bool) {
    std::string m = t;
    for (auto &ch : m) ch = (char)tolower((unsigned char)ch);
    if (m.size() != 17 || m[2] != ':' || m[5] != ':') {
      ui_toast("Adresse im Format aa:bb:cc:dd:ee:ff eingeben.");
      return;
    }
    vt_cfg_change([&](VotronicCfg &c) {
      c.mac = m;
      c.mac_type = 0;
    });
  };
  ui_input_open(o);
}

static void vt_pin_cb(lv_event_t *) {
  InputOptions o;
  o.title = "Kopplungs-PIN";
  o.hint = "PIN des S-BC (ab Werk 173928); leer = Standard";
  o.initial = votronic_cfg().pin;
  o.numeric = true;
  o.max_len = 6;
  o.on_ok = [](const std::string &t, bool) { vt_cfg_change([&](VotronicCfg &c) { c.pin = t.empty() ? VOTRONIC_PIN_DEFAULT : t; }); };
  ui_input_open(o);
}

static void vt_uart_cb(lv_event_t *) {
  vt_cfg_change([](VotronicCfg &c) {
    int n = votronic_uart_count();
    c.uart = c.uart + 1 >= n ? 1 : c.uart + 1;   // UART0 = Konsole/USB, nicht anbieten
  });
}

static void vt_rx_cb(lv_event_t *) {
  VotronicCfg c = votronic_cfg();
  InputOptions o;
  o.title = "RX-Pin (GPIO)";
  o.hint = "GPIO, an dem die Sendeleitung (TXD, RJ11 Pin 1) des Votronic-Geräts hängt; leer = keiner";
  o.initial = c.rx >= 0 ? std::to_string(c.rx) : "";
  o.numeric = true;
  o.max_len = 2;
  o.on_ok = [](const std::string &t, bool) {
    int v = t.empty() ? -1 : atoi(t.c_str());
    if (v < -1 || v > 54) v = -1;
    vt_cfg_change([&](VotronicCfg &c) { c.rx = (int8_t)v; });
  };
  ui_input_open(o);
}

static void vt_tiles_cb(lv_event_t *) {
  std::vector<ControlView> add;
  for (auto &v : votronic_list()) {
    ControlView cv;
    cv.key = v.key;
    cv.name = v.name;
    add.push_back(cv);
  }
  if (add.empty()) {
    ui_toast("Noch keine Werte – erst verbinden.");
    return;
  }
  layout_auto_arrange(add, true);
  ui_toast("Kacheln angelegt (vorhandene bleiben).");
}

static void build_votronic() {
  s_vt_ble_val = s_vt_ser_val = nullptr;
  s_vt_rows.clear();
  s_vt_sig = votronic_structure_sig();
  VotronicCfg c = votronic_cfg();
  heading("Votronic", "Batteriecomputer und Smart Shunt, Solarregler (SR/MPP), Ladegeräte (VBCS, VAC) und Ladebooster (VCC) "
                      "von Votronic – per Bluetooth über den Bluetooth Connector S-BC oder per Kabel an der Display-Buchse "
                      "eines Geräts. Nur Anzeige, das Panel stellt an den Geräten nichts ein. Noch nicht an echten Geräten getestet.");
  vt_switch_row("Bluetooth Connector S-BC", "Batteriecomputer und Solarregler per Bluetooth", c.ble_on, 0);
  if (c.ble_on) {
    lv_obj_t *g = group();
    s_vt_ble_val = ui_row(g, "Status", votronic_status().c_str(), nullptr, nullptr, false);
    if (!c.mac.empty()) {
      ui_row(g, "Adresse", c.mac.c_str(), vt_forget_cb, nullptr);
    } else {
      votronic_search(60000);
      s_vt_cands = votronic_candidates();
      ui_row(g, "Adresse", tr("von Hand eingeben"), vt_mac_cb, nullptr);
    }
    ui_row(g, "Kopplungs-PIN", c.pin.c_str(), vt_pin_cb, nullptr);
    if (c.mac.empty()) {
      if (s_vt_cands.empty()) {
        vt_hint("Suche läuft … Der S-BC muss Strom haben und in Reichweite sein; die Votronic-App auf dem Handy vorher trennen.");
      } else {
        lv_obj_t *cg = group();
        for (size_t i = 0; i < s_vt_cands.size(); i++) {
          auto &x = s_vt_cands[i];
          char val[24];
          snprintf(val, sizeof(val), "%d dBm", x.rssi);
          std::string t = (x.name.empty() ? std::string("Votronic") : x.name) + "  (" + x.mac + ")";
          ui_row(cg, t.c_str(), val, vt_cand_cb, (void *)(intptr_t)i);
        }
      }
    }
    vt_hint("Erstes Koppeln: am S-BC die Taste kurz drücken, dann verbindet sich das Panel – leuchten beide LEDs, hat es "
            "geklappt. Die PIN ist ab Werk 173928.");
  }
  vt_switch_row("Display-Link (Kabel)", "Display-Buchse eines Votronic-Geräts an einer seriellen Schnittstelle", c.uart_on, 1);
  if (c.uart_on) {
    lv_obj_t *g = group();
    s_vt_ser_val = ui_row(g, "Status", votronic_uart_status().c_str(), nullptr, nullptr, false);
    char b[16];
    snprintf(b, sizeof(b), "UART%d", c.uart);
    ui_row(g, "Schnittstelle", b, vt_uart_cb, nullptr);
    std::string rx = c.rx >= 0 ? "GPIO " + std::to_string(c.rx) : std::string(tr("nicht gesetzt"));
    ui_row(g, "RX-Pin", rx.c_str(), vt_rx_cb, nullptr);
    vt_hint("Westernstecker RJ11 (6-polig): Pin 1 TXD an den RX-Pin, Pin 6 GND an GND. Pin 3 frei lassen, Pin 2 (VBAT+) nicht "
            "zur Versorgung nutzen. 1000 Baud, nur Empfang. Pegel vorher messen – über 3,3 V einen Spannungsteiler davor.");
#if WITH_ZB_DIRECT
    if (c.uart == (int)ZBD_UART && zbd_enabled()) {
      char w[96];
      snprintf(w, sizeof(w), tr("Achtung: UART%d benutzt das Zigbee-Modul – bitte eine andere wählen."), (int)ZBD_UART);
      lv_obj_t *wl = ui_label(s_content, "", F_SMALL, C_RED);
      (lv_label_set_text)(wl, w);
    }
#endif
#if WITH_HOBBY
    HobbyCfg hc = hobby_cfg();
    if (hc.uart_on && hc.uart == c.uart) {
      lv_obj_t *wl = ui_label(s_content, "Achtung: diese Schnittstelle benutzt schon Hobby (CI-BUS).", F_SMALL, C_RED);
      lv_label_set_long_mode(wl, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(wl, lv_pct(100));
    }
#endif
  }
  auto rows = votronic_rows();
  if (rows.empty()) return;
  std::string dev;
  lv_obj_t *vg = nullptr;
  for (auto &r : rows) {
    if (r.dev != dev || !vg) {
      dev = r.dev;
      subheading(dev.c_str());
      vg = group();
    }
    s_vt_rows.push_back(ui_row(vg, r.name.c_str(), r.value.c_str(), nullptr, nullptr, false));
  }
  lv_obj_t *row = button_row();
  ui_button(row, ICON_PLUS, "Kacheln anlegen", true, vt_tiles_cb, nullptr);
  vt_hint("Im Editor stehen die Geräte in der Geräteliste wie Victron-Geräte (Batteriewächter, Solarregler …).");
}

void ui_settings_tick_votronic() {
  if (!s_scr || s_sec != SEC_VOTRONIC || ui_input_is_open()) return;
  if (votronic_structure_sig() != s_vt_sig) {
    vt_rebuild(nullptr);
    return;
  }
  VotronicCfg c = votronic_cfg();
  if (c.ble_on && c.mac.empty()) votronic_search(60000);   // solange die Seite offen ist, weiter suchen
  if (s_vt_ble_val) lv_label_set_text(s_vt_ble_val, votronic_status().c_str());
  if (s_vt_ser_val) lv_label_set_text(s_vt_ser_val, votronic_uart_status().c_str());
  auto rows = votronic_rows();
  for (size_t i = 0; i < s_vt_rows.size() && i < rows.size(); i++) lv_label_set_text(s_vt_rows[i], rows[i].value.c_str());
}
#else
static void build_votronic() {}
void ui_settings_tick_votronic() {}
#endif
#if WITH_ZB_DIRECT
// ---------------------------------------------------------------- Zigbee-Modul (ESP32-H2 im Steckplatz)
static uint32_t s_zb_sig = 0;
static lv_obj_t *s_zb_status = nullptr, *s_zb_net = nullptr, *s_zb_open = nullptr, *s_zb_log = nullptr;
static std::vector<std::pair<std::string, lv_obj_t *>> s_zb_rows;   // IEEE-Adresse -> Wert-Label
static std::vector<std::string> s_zb_ieee;                          // Index = user_data der Zeile

static std::string zb_status_text(const ZbdInfo &i) {
  if (!i.started) return tr("nicht gestartet");
  if (!i.present) return tr("nicht gefunden – Modul gesteckt und aufgespielt?");
  return std::string(tr("verbunden")) + (i.fw.empty() ? "" : " · Firmware " + i.fw);
}
static std::string zb_net_text(const ZbdInfo &i) {
  if (!i.present) return "–";
  if (!i.ready) return tr("wird gebildet …");
  char b[48];
  snprintf(b, sizeof(b), "PAN 0x%04X · %s %d", i.pan, tr("Kanal"), i.ch);
  return b;
}
static std::string zb_open_text(const ZbdInfo &i) {
  if (!i.open) return tr("geschlossen");
  char b[48];
  snprintf(b, sizeof(b), tr("offen, noch %d s"), i.open);
  return b;
}
static std::string zb_log_text() {
  auto l = zbd_log();
  std::string s;
  size_t from = l.size() > 6 ? l.size() - 6 : 0;
  for (size_t i = from; i < l.size(); i++) s += (s.empty() ? "" : "\n") + l[i];
  return s.empty() ? std::string(tr("Noch keine Meldungen.")) : s;
}
static std::string zb_dev_value(const ZbdDevice &d) {
  if (!d.done) return tr("wird abgefragt …");
  return d.summary.empty() ? std::string(tr("noch keine Werte")) : d.summary;
}

static void zb_rebuild(void *) {
  if (s_scr && s_sec == SEC_ZIGBEE) show_section(SEC_ZIGBEE);
}
static void zb_switch_cb(lv_event_t *e) {
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  zbd_set_enabled(on);
  if (!on) ui_toast("Ausgeschaltet – wirkt nach dem nächsten Neustart des Panels.");
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_permit_cb(lv_event_t *) {
  if (zbd_info().open) {
    zbd_permit(0);
    ui_toast("Anlernen beendet.");
  } else {
    zbd_permit(180);
    ui_toast("Anlernen 3 Minuten offen – jetzt das Gerät in den Kopplungsmodus bringen.");
  }
}
// ---- Geraeteseite: Werte und Einstellungen eines Geraets. Die Einstellungen holt das Panel erst beim Oeffnen vom
// Funkmodul und haelt sie nur, solange die Seite offen ist (spart RAM). Speichern schickt nur Geaendertes.
static std::string s_zb_dev;                                        // offene Geraeteseite ("" = Liste)
static std::vector<ZbdCfgItem> s_zb_cfg;                            // Einstellungen (nur solange offen)
static bool s_zb_cfg_done = false;
static std::vector<std::pair<std::string, std::string>> s_zb_pending;   // geaenderte Werte (Name -> JSON)
static std::vector<std::pair<std::string, lv_obj_t *>> s_zb_valrows;    // aktuelle Werte (Name -> Label)

static void zb_leave_device() {
  if (s_zb_dev.empty()) return;
  zbd_config_close();
  s_zb_dev.clear();
  s_zb_cfg.clear();
  s_zb_cfg.shrink_to_fit();
  s_zb_pending.clear();
  s_zb_valrows.clear();
  s_zb_cfg_done = false;
}
static std::string zb_plain(const std::string &json) {   // "\"ON\"" -> ON
  if (json.size() >= 2 && json.front() == '"' && json.back() == '"') return json.substr(1, json.size() - 2);
  return json;
}
static std::string zb_json_str(const std::string &t) {
  std::string o = "\"";
  for (char c : t) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o + "\"";
}
static void zb_set_pending(const std::string &name, const std::string &json) {
  for (auto &x : s_zb_pending)
    if (x.first == name) {
      x.second = json;
      return;
    }
  s_zb_pending.push_back({name, json});
}
static void zb_dev_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_zb_ieee.size()) return;
  s_zb_dev = s_zb_ieee[i];
  s_zb_cfg.clear();
  s_zb_pending.clear();
  s_zb_cfg_done = false;
  zbd_config_open(s_zb_dev);
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_back_cb(lv_event_t *) {
  if (!s_zb_pending.empty()) {
    ui_confirm("Änderungen verwerfen?", "Die geänderten Einstellungen wurden noch nicht gespeichert.", "Verwerfen", true,
               [] {
                 zb_leave_device();
                 lv_async_call(zb_rebuild, nullptr);
               });
    return;
  }
  zb_leave_device();
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_save_cb(lv_event_t *) {
  if (s_zb_pending.empty()) return;
  std::string j = "{";
  for (auto &x : s_zb_pending) j += (j.size() > 1 ? "," : "") + zb_json_str(x.first) + ":" + x.second;
  j += "}";
  if (!zbd_set(s_zb_dev, j)) {
    ui_toast("Zigbee-Modul nicht verbunden.");
    return;
  }
  s_zb_pending.clear();
  ui_toast("Gesendet – das Gerät übernimmt die Einstellungen.");
  s_zb_cfg_done = false;
  zbd_config_open(s_zb_dev);   // neuen Stand holen
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_discard_cb(lv_event_t *) {
  s_zb_pending.clear();
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_rename_cb(lv_event_t *) {
  std::string ieee = s_zb_dev, cur;
  for (auto &d : zbd_devices())
    if (d.ieee == ieee) cur = d.name;
  InputOptions o;
  o.title = tr("Name des Geräts");
  o.hint = tr("z. B. „Bewegung Flur“ oder „Taster Bett“. Leer lassen = Modellname.");
  o.initial = cur;
  o.max_len = 32;
  o.on_ok = [ieee](const std::string &t, bool) {
    zbd_rename(ieee, t);
    lv_async_call(zb_rebuild, nullptr);
  };
  ui_input_open(o);
}
static void zb_remove_cb(lv_event_t *) {
  std::string ieee = s_zb_dev;
  std::string txt = tr("Das Gerät ") + ieee + tr(" wird abgemeldet und vergessen. Zum erneuten Verwenden neu anlernen.");
  ui_confirm("Gerät entfernen?", txt.c_str(), "Entfernen", true, [ieee] {
    zbd_remove(ieee);
    zb_leave_device();
    ui_toast("Gerät entfernt.");
    lv_async_call(zb_rebuild, nullptr);
  });
}
// angezeigter Wert einer Einstellung: geaendert (noch nicht gespeichert) oder aktueller Stand
static std::string zb_cfg_value(const ZbdCfgItem &it) {
  for (auto &x : s_zb_pending)
    if (x.first == it.name) return zb_plain(x.second) + (it.unit.empty() ? "" : " " + it.unit) + " *";
  if (it.cur.empty()) return "–";
  return zb_plain(it.cur) + (it.unit.empty() ? "" : " " + it.unit);
}
static void zb_cfg_num_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_zb_cfg.size()) return;
  ZbdCfgItem it = s_zb_cfg[i];
  InputOptions o;
  o.title = it.name;
  char h[96];
  snprintf(h, sizeof(h), tr("Wert von %g bis %g%s%s"), it.min, it.max, it.unit.empty() ? "" : " ", it.unit.c_str());
  o.hint = h;
  o.numeric = it.type == 2;
  o.initial = zb_plain(it.cur);
  o.max_len = 32;
  o.on_ok = [it](const std::string &t, bool) {
    if (it.type != 2) {
      zb_set_pending(it.name, zb_json_str(t));
    } else {
      double v = atof(t.c_str());
      if (it.max > it.min && (v < it.min || v > it.max)) {
        ui_toast("Wert außerhalb des erlaubten Bereichs.");
        return;
      }
      char b[32];
      snprintf(b, sizeof(b), "%g", v);
      zb_set_pending(it.name, b);
    }
    lv_async_call(zb_rebuild, nullptr);
  };
  ui_input_open(o);
}
static void zb_cfg_bin_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_zb_cfg.size() || s_zb_cfg[i].values.size() < 2) return;
  const ZbdCfgItem &it = s_zb_cfg[i];
  std::string now = zb_plain(it.cur);
  for (auto &x : s_zb_pending)
    if (x.first == it.name) now = zb_plain(x.second);
  std::string next = now == it.values[0] ? it.values[1] : it.values[0];   // umschalten Ein <-> Aus
  zb_set_pending(it.name, (next == "true" || next == "false") ? next : zb_json_str(next));
  lv_async_call(zb_rebuild, nullptr);
}
static void zb_cfg_enum_cb(lv_event_t *e) {
  lv_obj_t *dd = lv_event_get_target(e);
  size_t i = (size_t)(intptr_t)lv_obj_get_user_data(dd);
  if (i >= s_zb_cfg.size()) return;
  uint16_t sel = lv_dropdown_get_selected(dd);
  if (sel >= s_zb_cfg[i].values.size()) return;
  zb_set_pending(s_zb_cfg[i].name, zb_json_str(s_zb_cfg[i].values[sel]));
  lv_async_call(zb_rebuild, nullptr);
}

static void build_zigbee_device() {
  s_zb_valrows.clear();
  ZbdDevice dev;
  for (auto &d : zbd_devices())
    if (d.ieee == s_zb_dev) dev = d;
  lv_obj_t *top = button_row();
  ui_button(top, ICON_LEFT, "Zurück zur Liste", false, zb_back_cb, nullptr);
  std::string model = dev.model.empty() ? s_zb_dev : (dev.mf.empty() ? dev.model : dev.mf + " " + dev.model);
  if (!dev.desc.empty()) model += " – " + dev.desc;
  heading(dev.name.empty() ? model.c_str() : dev.name.c_str(), dev.name.empty() ? (dev.desc.empty() ? nullptr : dev.desc.c_str()) : model.c_str());
  lv_obj_t *nr = button_row();
  ui_button(nr, ICON_PENCIL, "Umbenennen", false, zb_rename_cb, nullptr);

  subheading("Aktuelle Werte");
  auto vals = zbd_values(s_zb_dev);
  if (vals.empty()) {
    ui_label(s_content, "Noch keine Werte – das Gerät meldet sich bei der nächsten Änderung.", F_SMALL, C_TEXT2);
  } else {
    lv_obj_t *g = group();
    for (auto &v : vals) s_zb_valrows.push_back({v.first, ui_row(g, v.first.c_str(), zb_plain(v.second).c_str(), nullptr, nullptr, false)});
  }

  subheading("Einstellungen");
  s_zb_cfg_done = zbd_config_get(s_zb_cfg);
  if (!s_zb_cfg_done) {
    ui_label(s_content, "Werden vom Funkmodul geladen …", F_SMALL, C_TEXT2);
  } else if (s_zb_cfg.empty()) {
    ui_label(s_content, "Dieses Gerät hat keine Einstellungen.", F_SMALL, C_TEXT2);
  } else {
    lv_obj_t *g = group();
    for (size_t i = 0; i < s_zb_cfg.size(); i++) {
      const ZbdCfgItem &it = s_zb_cfg[i];
      if (it.type == 3 && !it.values.empty()) {   // Auswahl
        std::string opts, cur = zb_plain(it.cur);
        int sel = 0;
        for (auto &x : s_zb_pending)
          if (x.first == it.name) cur = zb_plain(x.second);
        for (size_t k = 0; k < it.values.size(); k++) {
          opts += (k ? "\n" : "") + it.values[k];
          if (it.values[k] == cur) sel = (int)k;
        }
        lv_obj_t *dd = dropdown_row(it.name.c_str(), opts.c_str(), sel, zb_cfg_enum_cb);
        lv_obj_set_user_data(dd, (void *)(intptr_t)i);
      } else if (it.type == 1) {   // Ein/Aus: antippen schaltet um
        ui_row(g, it.name.c_str(), zb_cfg_value(it).c_str(), zb_cfg_bin_cb, (void *)(intptr_t)i);
      } else {   // Zahl oder Text: antippen = eingeben
        ui_row(g, it.name.c_str(), zb_cfg_value(it).c_str(), zb_cfg_num_cb, (void *)(intptr_t)i);
      }
    }
    lv_obj_t *h = ui_label(s_content, "Mit * markiert: geändert, noch nicht gespeichert.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  lv_obj_t *row = button_row();
  ui_button(row, ICON_CHECK, "Speichern", !s_zb_pending.empty(), s_zb_pending.empty() ? nullptr : zb_save_cb, nullptr);
  if (!s_zb_pending.empty()) ui_button(row, ICON_CLOSE, "Verwerfen", false, zb_discard_cb, nullptr);
  ui_button(row, ICON_CLOSE, "Gerät entfernen", false, zb_remove_cb, nullptr);
}
static void zb_reset_cb(lv_event_t *) {
  ui_confirm("Zigbee-Netz neu bilden?",
             "Alle Geräte verlieren die Verbindung und müssen neu angelernt werden. Nur bei Störungen nötig.", "Neu bilden",
             true, [] {
               zbd_reset_network();
               ui_toast("Netz wird neu gebildet …");
             });
}

static void build_zigbee() {
  s_zb_sig = zbd_structure_sig();
  if (!s_zb_dev.empty()) {
    s_zb_status = s_zb_net = s_zb_open = s_zb_log = nullptr;
    s_zb_rows.clear();
    build_zigbee_device();
    return;
  }
  s_zb_status = s_zb_net = s_zb_open = s_zb_log = nullptr;
  s_zb_rows.clear();
  s_zb_ieee.clear();
  heading("Zigbee", "Zigbee-Geräte (Lampen, Steckdosen, Sensoren) direkt am Panel – über das Funkmodul ESP32-H2 im Steckplatz "
                    "auf der Rückseite, ohne Zigbee2MQTT und ohne Broker.");
  // Schalter wie bei den Bluetooth-Geraeten
  lv_obj_t *r = lv_obj_create(s_content);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), 64);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, C_LINE, 0);
  lv_obj_align(ui_label(r, "Zigbee-Modul", F_BODY, C_TEXT), LV_ALIGN_TOP_LEFT, 8, 8);
  lv_obj_align(ui_label(r, "Funkmodul im Steckplatz verwenden", F_SMALL, C_TEXT2), LV_ALIGN_TOP_LEFT, 8, 36);
  lv_obj_t *sw = lv_switch_create(r);
  lv_obj_set_size(sw, 64, 34);
  lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (zbd_enabled()) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(sw, zb_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  if (!zbd_enabled()) return;

  ZbdInfo in = zbd_info();
  subheading("Funkmodul");
  lv_obj_t *g = group();
  s_zb_status = ui_row(g, "Modul", zb_status_text(in).c_str(), nullptr, nullptr, false);
  s_zb_net = ui_row(g, "Zigbee-Netz", zb_net_text(in).c_str(), nullptr, nullptr, false);
  s_zb_open = ui_row(g, "Anlernen", zb_open_text(in).c_str(), nullptr, nullptr, false);
  lv_obj_t *row = button_row();
  bool can = in.present && in.ready;
  ui_button(row, in.open ? ICON_CLOSE : ICON_PLUS, in.open ? "Anlernen beenden" : "Gerät anlernen", !in.open,
            can ? zb_permit_cb : nullptr, nullptr);
  lv_obj_t *h = ui_label(s_content,
                         "„Gerät anlernen“ öffnet das Netz für 3 Minuten. Dann das Gerät in den Kopplungsmodus bringen "
                         "(meist Taste lange drücken oder Lampe mehrmals aus- und einschalten). Das Modul fragt neue Geräte "
                         "selbst ab und richtet die Meldungen ein.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));

  auto devs = zbd_devices();
  subheading((std::string(tr("Geräte")) + " (" + std::to_string(devs.size()) + ")").c_str());
  if (devs.empty()) {
    ui_label(s_content, in.present ? "Noch keine Geräte angelernt." : "Ohne Funkmodul keine Geräte.", F_SMALL, C_TEXT2);
  } else {
    lv_obj_t *dg = group();
    for (auto &d : devs) {
      std::string title = d.model.empty() ? d.ieee : (d.mf.empty() ? d.model : d.mf + " " + d.model);
      if (!d.desc.empty()) title += " – " + d.desc;   // Beschreibung aus der Treiberliste
      if (!d.name.empty()) title = d.name + "  (" + (d.model.empty() ? d.ieee : d.model) + ")";   // eigener Name zuerst
      lv_obj_t *v = ui_row(dg, title.c_str(), zb_dev_value(d).c_str(), zb_dev_cb, (void *)(intptr_t)s_zb_ieee.size());
      s_zb_ieee.push_back(d.ieee);
      s_zb_rows.push_back({d.ieee, v});
    }
    lv_obj_t *h2 = ui_label(s_content, "Gerät antippen: Werte, Einstellungen, Name, Entfernen.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h2, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h2, lv_pct(100));
  }

  subheading("Meldungen des Moduls");
  s_zb_log = ui_label(s_content, zb_log_text().c_str(), F_SMALL, C_TEXT2);
  lv_label_set_long_mode(s_zb_log, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_zb_log, lv_pct(100));
  if (in.present) {
    lv_obj_t *row2 = button_row();
    ui_button(row2, ICON_REFRESH, "Netz neu bilden", false, zb_reset_cb, nullptr);
  }
}

void ui_settings_tick_zigbee() {
  if (!s_scr || s_sec != SEC_ZIGBEE || ui_input_is_open() || !zbd_enabled()) return;
  if (!s_zb_dev.empty()) {   // Geraeteseite: Einstellungen fertig geladen -> neu aufbauen, sonst Werte nachfuehren
    std::vector<ZbdCfgItem> tmp;
    bool done = zbd_config_get(tmp);
    if (done != s_zb_cfg_done) {
      show_section(SEC_ZIGBEE);
      return;
    }
    auto vals = zbd_values(s_zb_dev);
    if (vals.size() != s_zb_valrows.size()) {
      show_section(SEC_ZIGBEE);
      return;
    }
    for (auto &r : s_zb_valrows)
      for (auto &v : vals)
        if (v.first == r.first) lv_label_set_text(r.second, zb_plain(v.second).c_str());
    return;
  }
  if (zbd_structure_sig() != s_zb_sig) {
    show_section(SEC_ZIGBEE);
    return;
  }
  ZbdInfo in = zbd_info();
  if (s_zb_status) lv_label_set_text(s_zb_status, zb_status_text(in).c_str());
  if (s_zb_net) lv_label_set_text(s_zb_net, zb_net_text(in).c_str());
  if (s_zb_open) lv_label_set_text(s_zb_open, zb_open_text(in).c_str());
  if (s_zb_log) lv_label_set_text(s_zb_log, zb_log_text().c_str());
  auto devs = zbd_devices();
  for (auto &r : s_zb_rows)
    for (auto &d : devs)
      if (d.ieee == r.first) lv_label_set_text(r.second, zb_dev_value(d).c_str());
}
#endif

#if WITH_BMS
static void build_bms(BmsCat cat);
#endif
static void build_gas() {
#if SOC_VICTRON_EDITION
  heading("Gas", "Füllstand per Bluetooth. Wähle, welche Geräte das Panel auswerten soll – was aus ist, "
                 "wird nicht gesucht und belastet Funk und WLAN nicht.");
#else
  heading("Gas und Heizung", "Füllstand und Heizung per Bluetooth. Wähle, welche Geräte das Panel auswerten soll – was aus ist, "
                             "wird nicht gesucht und belastet Funk und WLAN nicht.");
#endif
  proto_switch("Truma LevelControl", "Gasflaschen, bis zu vier", BLE_P_TRUMA);
  proto_switch("Mopeka", "Gas- und Tanksensoren, bis zu vier", BLE_P_MOPEKA);
  if (inetx_enabled()) proto_switch("Truma iNet X", "Heizung, Boiler und Klima über das Bedienteil", BLE_P_INETX);
  Layout lay = layout_get();
  if (ble_proto_on(BLE_P_TRUMA)) build_truma_lc(lay);
  if (ble_proto_on(BLE_P_MOPEKA)) build_mopeka(lay);
  build_heat();
#if WITH_BMS
  build_bms(BMS_CAT_HEAT);   // Dieselheizung und Kuehlbox (Bluetooth, Modul bms.cpp)
#endif
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

// ---------------------------------------------------------------- Nach dem Hinzufuegen: Kachel anbieten
// Fragt (kurz nach dem Schliessen des Eingabefensters), ob die passende Kachel auf dem Hauptbildschirm angelegt werden
// soll; "Ja" setzt sie auf den ersten freien Platz (wie "Fehlende ergaenzen", nur fuer dieses eine Geraet).
struct TileOffer {
  std::string key, name;
};
static void offer_tile_cb(lv_timer_t *t) {
  TileOffer *o = (TileOffer *)t->user_data;
  lv_timer_del(t);
  std::string key = o->key, name = o->name;
  delete o;
  for (auto &p : layout_get().pages)
    for (auto &tl : p.tiles)
      if (tl.key == key) return;   // liegt schon auf dem Panel
  ui_confirm("Kachel anlegen?", (std::string(tr("Soll „")) + name + tr("“ als Kachel auf den Hauptbildschirm?")).c_str(),
             "Kachel anlegen", false, [key, name] {
               ControlView v;
               v.key = key;
               v.name = name;
               layout_auto_arrange({v}, true);
               ui_toast("Kachel angelegt. Position und Größe änderst du im Editor.");
             });
}
static void offer_tile(const std::string &key, const std::string &name) {
  lv_timer_create(offer_tile_cb, 400, new TileOffer{key, name});
}
static std::string lower_mac(std::string s) {
  for (auto &c : s) c = tolower((unsigned char)c);
  return s;
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
    if (err.empty())
      offer_tile(VICTRON_KEY_PREFIX + lower_mac(f.mac),
                 f.name.empty() ? std::string(victron_kind_text(victron_kind_of_record(f.record))) : f.name);
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

// ---------------------------------------------------------------- Akkus mit BMS (Bluetooth)
#if WITH_BMS
static std::vector<BmsFound> s_bms_found;
static std::vector<lv_obj_t *> s_bms_vals;
static std::vector<std::string> s_bms_keys;   // Schluessel zu s_bms_vals (Akkus bzw. Power Stations)
static bool is_ps(BmsModel m) { return bms_dev_of(m) == BMS_DEV_PSTATION; }
static uint32_t s_bms_sig = 0;

static uint32_t bms_struct_sig() {
  uint32_t h = bms_discovery_running() ? 11 : 5;
  auto mix = [&h](uint32_t x) { h = h * 31 + x; };
  for (auto &b : bms_list()) {
    for (char c : b.mac) mix(c);
    for (char c : b.name) mix(c);
  }
  for (auto &f : bms_found()) {
    for (char c : f.mac) mix(c);
    mix(f.configured);
  }
  return h;
}

static std::string bms_summary(const BmsInfo &b) { return bms_value_text(b); }   // je Geraeteart (bms.cpp)

static void bms_row_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i < s_bms_keys.size()) ui_vic_open(s_bms_keys[i]);
}

// Hinzufuegen: Name -> Kopplungs-PIN (leer = keine; Super B verlangt immer eine) -> bms_add -> Kachel anbieten
struct BmsAdd {
  BmsFound f;
  std::string name;
};
static void bms_pin_ask_cb(lv_timer_t *tm) {
  BmsAdd *a = (BmsAdd *)tm->user_data;
  lv_timer_del(tm);
  BmsFound f = a->f;
  std::string name = a->name;
  delete a;
  InputOptions o;
  o.title = "Kopplungs-PIN";
  o.hint = f.model == BMS_SUPERB
               ? "Super B verlangt beim Koppeln eine PIN – sie steht auf dem Akku bzw. in der Anleitung."
               : "Nur nötig, wenn der Akku beim Koppeln eine PIN verlangt (steht auf dem Akku bzw. in der Anleitung). "
                 "Meist leer lassen. Später änderbar: Akku antippen > PIN.";
  o.numeric = true;
  o.max_len = 32;   // Simarine-Schluessel; sonst kuerzt bms.cpp auf 6
  o.on_ok = [f, name](const std::string &pin, bool) {
    std::string err = bms_add(f.mac, f.addr_type, name, f.model, f.capacity_ah, f.cells);
    if (err.empty() && !pin.empty()) bms_set_pin(f.mac, pin);
    ui_toast(err.empty() ? (pin.empty() ? "Eingerichtet – das Panel verbindet sich gleich."
                                        : "Eingerichtet – das Panel koppelt gleich mit PIN.")
                         : err.c_str());
    if (err.empty()) offer_tile(BMS_KEY_PREFIX + lower_mac(f.mac), name.empty() ? std::string(bms_model_text(f.model)) : name);
  };
  ui_input_open(o);
}

static void bms_found_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_bms_found.size()) return;
  BmsFound f = s_bms_found[i];
  if (f.configured) {
    ui_toast("Dieses Gerät ist schon eingerichtet.");
    return;
  }
  InputOptions o;
  BmsCat cat = bms_cat_of(f.model);
  o.title = cat == BMS_CAT_PS ? "Name der Power Station" : cat == BMS_CAT_SENSOR ? "Name des Sensors"
          : cat == BMS_CAT_HEAT ? "Name des Geräts" : "Name des Akkus";
  o.hint = cat == BMS_CAT_SENSOR ? "Zum Beispiel „Kühlschrank“ oder „Frischwasser“."
         : cat == BMS_CAT_HEAT ? "Zum Beispiel „Standheizung“. Die App des Herstellers vorher schließen – das Gerät spricht nur mit einem Gerät."
                               : "Zum Beispiel „Bordbatterie“. Die App des Herstellers vorher schließen – der Akku spricht nur mit einem Gerät.";
  o.initial = f.name.empty() ? std::string(bms_model_text(f.model)) : f.name;
  o.max_len = 32;
  o.on_ok = [f](const std::string &t, bool) {
    if (is_ps(f.model) || f.model == BMS_RUUVI || f.model == BMS_SWITCHBOT || f.model == BMS_BTHOME ||
        f.model == BMS_HEATER || f.model == BMS_FRIDGE) {   // keine PIN-Frage (Heizung: Passkey 1234, bei Bedarf in den Einstellungen)
      std::string err = bms_add(f.mac, f.addr_type, t, f.model, f.capacity_ah, f.cells);
      ui_toast(err.empty() ? "Eingerichtet – das Panel verbindet sich gleich." : err.c_str());
      if (err.empty()) offer_tile(BMS_KEY_PREFIX + lower_mac(f.mac), t.empty() ? std::string(bms_model_text(f.model)) : t);
      return;
    }
    // zweiter Dialog erst nach dem Schliessen des ersten (Timer, wie offer_tile)
    lv_timer_create(bms_pin_ask_cb, 300, new BmsAdd{f, t});
  };
  ui_input_open(o);
}

static void bms_scan_cb(lv_event_t *) {
  if (bms_list().size() >= BMS_MAX) {
    ui_toast("Kein Platz mehr – es sind schon so viele Bluetooth-Geräte eingerichtet, wie möglich sind.");
    return;
  }
  bms_discovery_start();
  ui_toast("Suche läuft (1 Minute). Die App des Herstellers vorher schließen.");
}

static void ef_uid_cb(lv_event_t *) {
  InputOptions o;
  o.title = "EcoFlow-Benutzer-ID";
  o.hint = "Die Benutzer-ID des EcoFlow-Kontos, an das die Power Station gebunden ist. Bequemer: im Editor unter "
           "„Power Stations“ mit E-Mail und Passwort anmelden – dann wird sie automatisch eingetragen. Leer = löschen.";
  o.initial = "";
  o.max_len = 32;
  o.on_ok = [](const std::string &t, bool) {
    bms_ef_set_uid(t);
    ui_toast(t.empty() ? "Benutzer-ID gelöscht." : "Benutzer-ID gespeichert.");
    show_section(SEC_PSTATION);
  };
  ui_input_open(o);
}

// Rubriken (gleiches Modul bms.cpp): Akkus inkl. Shunts und Renogy-Laderegler, Power Stations, Sensoren (eigene Seiten),
// Dieselheizung/Kuehlbox (Teil der Seite Gas/Heizung)
static void build_bms(BmsCat cat) {
  bool ps = cat == BMS_CAT_PS;
  if (ps)
    heading("Power Stations", "Bluetti, Allpowers, Anker SOLIX und EcoFlow per Bluetooth – ungetestet. Die App des Herstellers "
                              "vorher schließen; Anker: beim ersten Verbinden ggf. die Taste an der Power Station drücken. "
                              "Antippen zeigt alle Werte; dort lassen sich AC- und DC-Ausgang ein- und ausschalten.");
  else if (cat == BMS_CAT_SENSOR)
    heading("Sensoren", "Bluetooth-Sensoren – ungetestet: Ruuvi, SwitchBot (Meter, Meter Plus, Outdoor), BTHome (Shelly BLU, "
                        "Xiaomi mit pvvx-Firmware …), Simarine STRx und SuperSense-Tankmodul. Sie senden nur, das Panel hört zu. "
                        "Verschlüsselte BTHome- und Simarine-Sensoren brauchen den Schlüssel (32 Zeichen) als PIN.");
  else if (cat == BMS_CAT_HEAT)
    subheading("Dieselheizung und Kühlbox");
  else
    heading("Akkus (BMS)", "Akkus mit eigenem Bluetooth-BMS. Die App des Herstellers vorher schließen – der Akku spricht "
                           "nur mit einem Gerät. Das Panel liest nur Werte und schaltet am Akku nichts.");
  if (cat == BMS_CAT_HEAT) {
    lv_obj_t *h = ui_label(s_content, "China-Dieselheizungen (Vevor, Hcalory, HeaterCC/AirHeater) und Kompressor-Kühlboxen "
                                      "(Alpicool, IceCube/Plug-in Festivals, Bodega, Brass Monkey) – ungetestet. Bedienen im "
                                      "Fenster der Kachel.", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  proto_switch(cat == BMS_CAT_BAT ? "Akkus auswerten" : "Bluetooth-Geräte auswerten",
               cat == BMS_CAT_BAT ? "Super B, Power Quarry, Daly, JBD, JK, LiTime, Renogy (auch Laderegler, DC-DC-Lader, "
                                    "Wechselrichter, Shunt), Eco-Worthy, ANT, Vatrer – nur lesen"
                                  : "Ein Schalter für Akkus, Power Stations, Sensoren, Heizung und Kühlbox",
               BLE_P_BMS);
  s_bms_vals.clear();
  s_bms_keys.clear();
  if (!ble_proto_on(BLE_P_BMS)) {
    s_bms_sig = 0;
    return;
  }
  Layout lay = layout_get();
  lv_obj_t *g = group();
  for (auto &b : bms_list()) {
    if (bms_cat_of(b.model) != cat) continue;
    auto it = lay.names.find(b.key);
    std::string label = it != lay.names.end() && !it->second.empty() ? it->second : b.name;
    s_bms_vals.push_back(ui_row(g, label.c_str(), bms_summary(b).c_str(), bms_row_cb, (void *)(intptr_t)s_bms_keys.size()));
    s_bms_keys.push_back(b.key);
  }
  if (s_bms_keys.empty())
    ui_label(g, ps ? "Noch keine Power Station eingerichtet." : cat == BMS_CAT_SENSOR ? "Noch kein Sensor eingerichtet."
                : cat == BMS_CAT_HEAT ? "Noch keine Heizung oder Kühlbox eingerichtet." : "Noch kein Akku eingerichtet.",
             F_BODY, C_TEXT2);
  if (ps && BMS_ECOFLOW_ON) {   // EcoFlow braucht die Benutzer-ID des Kontos
    lv_obj_t *eg = group();
    std::string v = bms_ef_uid_set() ? std::string(tr("gespeichert ")) + bms_ef_uid_hint() : std::string(tr("nicht eingetragen"));
    ui_row(eg, "EcoFlow-Benutzer-ID", v.c_str(), ef_uid_cb, nullptr);
  }
  lv_obj_t *row = button_row();
  bool running = bms_discovery_running();
  ui_button(row, ICON_SEARCH,
            running ? "Suche läuft …" : ps ? "Power Stations suchen" : cat == BMS_CAT_SENSOR ? "Sensoren suchen"
                    : cat == BMS_CAT_HEAT ? "Heizung/Kühlbox suchen" : "Akkus suchen",
            true, bms_scan_cb, nullptr);
  s_bms_found.clear();
  for (auto &f : bms_found())
    if (bms_cat_of(f.model) == cat) s_bms_found.push_back(f);
  if (running || !s_bms_found.empty()) {
    subheading(running ? "Gefunden (Suche läuft …)" : "Gefunden");
    lv_obj_t *fg = group();
    if (s_bms_found.empty())
      ui_label(fg, ps ? "Noch nichts gefunden. Power Station eingeschaltet, Bluetooth an, in der Nähe?"
                   : cat == BMS_CAT_SENSOR ? "Noch nichts gefunden. Sensor in der Nähe? Manche senden nur alle paar Sekunden."
                   : cat == BMS_CAT_HEAT ? "Noch nichts gefunden. Gerät eingeschaltet und in der Nähe? App des Herstellers geschlossen?"
                                         : "Noch nichts gefunden. Akku eingeschaltet und in der Nähe?",
               F_BODY, C_TEXT2);
    for (size_t i = 0; i < s_bms_found.size(); i++) {
      const BmsFound &f = s_bms_found[i];
      std::string title = (f.name.empty() ? std::string(bms_model_text(f.model)) : f.name) + "  ·  " + bms_model_text(f.model);
      const char *q = f.rssi > -65 ? "stark" : f.rssi > -80 ? "mittel" : "schwach";
      char v[64];
      snprintf(v, sizeof(v), "%s%s", f.configured ? tr("eingerichtet, ") : "", q);
      ui_row(fg, title.c_str(), v, bms_found_cb, (void *)(intptr_t)i);
    }
  }
  s_bms_sig = bms_struct_sig();
}
#endif

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
  if (!s_scr || ui_input_is_open()) return;
#if WITH_BMS
  // eigene Abschnitte "Akkus (BMS)", "Power Stations", "Sensoren"; Heizung/Kuehlbox stehen unter Gas/Heizung
  if (s_sec == SEC_BMS || s_sec == SEC_PSTATION || s_sec == SEC_SENSOR || s_sec == SEC_GAS) {
    if (!ble_proto_on(BLE_P_BMS)) return;
    if (bms_struct_sig() != s_bms_sig) {
      show_section(s_sec);
      return;
    }
    for (size_t i = 0; i < s_bms_keys.size() && i < s_bms_vals.size(); i++) {
      BmsInfo b;
      if (bms_get(s_bms_keys[i], b)) lv_label_set_text(s_bms_vals[i], bms_summary(b).c_str());
    }
    return;
  }
#endif
  if (s_sec != SEC_VICTRON) return;
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
static const char *INFO_TITLES[] = {"Firmware", "IP-Adresse", "WLAN-Signal", SOC_VICTRON_EDITION ? "" : "Zigbee2MQTT", "Geräteliste",
                                    "Touch", "Akku", "Freier Speicher", "Laufzeit", "Panel-WLAN"};
static const int INFO_N = 10;

// Debug-Log am Bildschirm ein/aus (dasselbe wie im Editor unter Info); Stand: SD-Datei bzw. nur Arbeitsspeicher
static void dbg_state_fill() {
  if (!s_dbg_state) return;
  std::string t;
  if (!dbglog_on()) t = tr("Aus – zum Einschalten antippen. Ab Werk aus.");
  else if (!dbglog_current().empty()) t = std::string(tr("Schreibt auf die SD-Karte: ")) + "panel/logs/" + dbglog_current();
  else if (sd_ok()) t = tr("An – SD-Karte wird eingebunden …");
  else t = tr("An – keine SD-Karte, nur die letzten 64 KB im Arbeitsspeicher (im Editor unter Info ansehen).");
  (lv_label_set_text)(s_dbg_state, t.c_str());
}
static void dbg_sw_cb(lv_event_t *e) {
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  dbglog_set(on);
  ui_toast(on ? "Debug-Log läuft – alle Meldungen werden mitgeschrieben." : "Debug-Log ist aus.");
  dbg_state_fill();
}

static void info_fill() {
  dbg_state_fill();
#if WITH_TAILSCALE
  if (s_ts_val) {
    TsStatus t = ts_status();
    std::string v = !t.on ? std::string(tr("aus (einschalten im Editor unter Netzwerk)"))
                    : t.ip.empty() ? std::string(tr(t.state.c_str())) : t.ip + "  ·  " + t.name;
    (lv_label_set_text)(s_ts_val, v.c_str());
  }
#endif
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
  for (int i = 0; i < INFO_N; i++) {
    s_info[i] = ui_row(g, INFO_TITLES[i], "", nullptr, nullptr, false);
    if (SOC_VICTRON_EDITION && (i == 3 || i == 4)) lv_obj_add_flag(lv_obj_get_parent(s_info[i]), LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_t *row = button_row();
  if (!SOC_VICTRON_EDITION) ui_button(row, ICON_REFRESH, "Zustände neu abfragen", false, refresh_cb, nullptr);
  ui_button(row, ICON_POWER, "Neu starten", false, reboot_cb, nullptr);
  // Debug-Log (Fehlersuche): auch am Bildschirm schaltbar, Dateien auf der SD-Karte
  subheading("Fehlersuche (Debug-Log)");
  {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_t *t = ui_label(r, "Debug-Log auf die SD-Karte schreiben", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (dbglog_on()) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, dbg_sw_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    s_dbg_state = ui_label(s_content, "", F_SMALL, C_TEXT2);
    lv_label_set_long_mode(s_dbg_state, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_dbg_state, lv_pct(100));
    lv_obj_t *h = ui_label(s_content, "Alle Meldungen mit Uhrzeit, alle 60 s Speicher, WLAN und Verbindungen. Dateien je bis 1 MB, "
                                      "die letzten 10. Ansehen und herunterladen im Editor unter Einstellungen → Info.", F_SMALL, C_TEXT3);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h, lv_pct(100));
  }
  info_fill();
  // Projekt und Unterstuetzung (Edition: nur das oeffentliche Beta-Repo)
#if SOC_VICTRON_EDITION
  const char *git = "github.com/cyclemat/simply-open-camper-victron-beta";
#else
  const char *git = "github.com/cyclemat/simple-open-camper";
#endif
  lv_obj_t *pg = group();
  ui_row(pg, "Projekt auf GitHub", git, nullptr, nullptr, false);
  subheading("Unterstütze das Projekt");
  lv_obj_t *sup = group();
  lv_obj_set_style_bg_opa(sup, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(sup, C_CARD, 0);
  lv_obj_set_style_radius(sup, 16, 0);
  lv_obj_set_style_pad_all(sup, 20, 0);
  lv_obj_set_style_pad_row(sup, 6, 0);
  lv_obj_t *h = ui_label(sup, "Gefällt dir das Panel? Über eine kleine Spende freue ich mich:", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
  lv_obj_t *pp = ui_label(sup, "", F_TITLE, C_AMBER);
  (lv_label_set_text)(pp, "paypal.me/cyclemat");
}


// ---------------------------------------------------------------- Shelly per Bluetooth (Gen2+, Beta)
#if WITH_SHELLY_BLE
static std::vector<ShbFound> s_shb_found;
static std::vector<ShbInfo> s_shb_list;
static uint32_t s_shb_sig = 0;

static void shb_rebuild(void *) {
  if (!s_scr || s_sec != SEC_SHELLY_BT || ui_input_is_open()) return;
  lv_coord_t y = lv_obj_get_scroll_y(s_content);
  show_section(SEC_SHELLY_BT);
  lv_obj_update_layout(s_content);
  lv_obj_scroll_to_y(s_content, y, LV_ANIM_OFF);
}

static void shb_on_cb(lv_event_t *e) {
  shb_set_enabled(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
  lv_async_call(shb_rebuild, nullptr);
}

static void shb_search_cb(lv_event_t *) {
  shb_search(120000);
  ui_toast("Suche läuft (2 Minuten). Bluetooth muss am Shelly an sein (Shelly-App > Einstellungen > Bluetooth).");
  lv_async_call(shb_rebuild, nullptr);
}

static void shb_tiles_cb(lv_event_t *) {
  std::vector<ControlView> all, add;
  shb_all(all);
  for (auto &v : all) add.push_back(v);
  if (add.empty()) {
    ui_toast("Noch keine Kanäle – die erste Verbindung zum Shelly steht noch aus.");
    return;
  }
  layout_auto_arrange(add, true);
  ui_toast("Kacheln angelegt.");
}

struct ShbAdd {
  ShbFound f;
  std::string name;
};
static void shb_pw_ask_cb(lv_timer_t *tm) {
  ShbAdd *a = (ShbAdd *)tm->user_data;
  lv_timer_del(tm);
  ShbFound f = a->f;
  std::string name = a->name;
  delete a;
  InputOptions o;
  o.title = "Passwort des Shelly";
  o.hint = "Nur nötig, wenn am Shelly ein Passwort gesetzt ist (Shelly-App > Einstellungen > Authentifizierung). "
           "Meist leer lassen. Später änderbar: Shelly antippen.";
  o.password = true;
  o.max_len = 64;
  o.on_ok = [f, name](const std::string &pw, bool) {
    std::string err = shb_add(f.mac, f.type, name, pw);
    ui_toast(err.empty() ? "Eingerichtet – das Panel verbindet sich gleich. Danach „Kacheln anlegen“." : err.c_str());
    lv_async_call(shb_rebuild, nullptr);
  };
  ui_input_open(o);
}

static void shb_found_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_shb_found.size()) return;
  ShbFound f = s_shb_found[i];
  if (f.configured) {
    ui_toast("Dieser Shelly ist schon eingerichtet.");
    return;
  }
  InputOptions o;
  o.title = "Name des Shelly";
  o.hint = "Zum Beispiel „Außenlicht“ oder „Wasserpumpe“.";
  o.initial = f.name;
  o.max_len = 32;
  o.on_ok = [f](const std::string &t, bool) { lv_timer_create(shb_pw_ask_cb, 300, new ShbAdd{f, t}); };
  ui_input_open(o);
}

// Shelly antippen: umbenennen; Haken = entfernen
static void shb_dev_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_shb_list.size()) return;
  ShbInfo x = s_shb_list[i];
  InputOptions o;
  o.title = "Shelly umbenennen";
  o.initial = x.name;
  o.max_len = 32;
  o.check_label = "Diesen Shelly entfernen";
  o.on_ok = [x](const std::string &t, bool remove) {
    if (remove) shb_remove(x.mac);
    else shb_rename(x.mac, t);
    ui_toast(remove ? "Entfernt." : "Gespeichert.");
    lv_async_call(shb_rebuild, nullptr);
  };
  ui_input_open(o);
}

static void shb_pw_cb(lv_event_t *e) {
  size_t i = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_shb_list.size()) return;
  std::string mac = s_shb_list[i].mac;
  InputOptions o;
  o.title = "Passwort des Shelly";
  o.hint = "Wie in der Shelly-App unter Authentifizierung. Leer = kein Passwort.";
  o.password = true;
  o.max_len = 64;
  o.on_ok = [mac](const std::string &pw, bool) {
    shb_set_password(mac, pw);
    ui_toast("Gespeichert – das Panel meldet sich gleich neu an.");
    lv_async_call(shb_rebuild, nullptr);
  };
  ui_input_open(o);
}

static std::string shb_ch_text(const ShbInfo &x) {
  if (!x.online) return x.status.empty() ? std::string(tr("noch nicht verbunden")) : x.status;
  std::string r;
  for (auto &c : x.ch) {
    if (!r.empty()) r += "  ·  ";
    if (x.ch.size() > 1) r += std::to_string(atoi(c.comp.c_str() + c.comp.find(':') + 1) + 1) + ": ";
    r += tr(c.on ? "An" : "Aus");
    if (c.bri >= 0 && c.on) r += " " + std::to_string(c.bri) + " %";
    if (!isnan(c.power)) {
      char b[16];
      snprintf(b, sizeof(b), " %.0f W", c.power);
      r += b;
    }
  }
  return r.empty() ? std::string(tr("keine Kanäle")) : r;
}

static void build_shb() {
  heading("Shelly per Bluetooth (Beta)",
          "Shelly Gen2, Gen3 und Gen4 (Plus, Pro, Mini …) ohne WLAN schalten – das Panel verbindet sich kurz per "
          "Bluetooth (Stand alle 30 s, Schalten sofort, dauert 1–2 s). Gen1 hat kein Bluetooth: dafür Shelly über MQTT.");
  {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 56);
    lv_obj_t *t = ui_label(r, "Shellys per Bluetooth", F_BODY, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (shb_enabled()) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, shb_on_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  }
  s_shb_sig = shb_version() * 2 + shb_searching();
  if (!shb_enabled()) return;
  lv_obj_t *row = button_row();
  ui_button(row, ICON_SEARCH, shb_searching() ? "Suche läuft …" : "Shellys suchen", !shb_searching(), shb_search_cb, nullptr);
  ui_button(row, ICON_PLUS, "Kacheln anlegen", false, shb_tiles_cb, nullptr);
  s_shb_list = shb_list();
  if (!s_shb_list.empty()) {
    subheading("Eingerichtet");
    lv_obj_t *g = group();
    for (size_t i = 0; i < s_shb_list.size(); i++) {
      auto &x = s_shb_list[i];
      std::string title = x.name.empty() ? x.mac : x.name;
      if (!x.model.empty()) title += "  (" + x.model + ")";
      ui_row(g, title.c_str(), shb_ch_text(x).c_str(), shb_dev_cb, (void *)(intptr_t)i);
      ui_row(g, "Passwort", x.has_pw ? "gesetzt – antippen zum Ändern" : x.auth ? "VERLANGT – antippen zum Eintragen" : "keins",
             shb_pw_cb, (void *)(intptr_t)i);
    }
  }
  s_shb_found = shb_found();
  if (shb_searching() || !s_shb_found.empty()) {
    subheading(shb_searching() ? "Gefundene Shellys (Suche läuft)" : "Gefundene Shellys");
    lv_obj_t *g = group();
    if (s_shb_found.empty()) ui_label(g, "Noch keiner gefunden … Shelly in der Nähe und Bluetooth an?", F_SMALL, C_TEXT2);
    for (size_t i = 0; i < s_shb_found.size(); i++) {
      auto &f = s_shb_found[i];
      char v[48];
      snprintf(v, sizeof(v), "%d dBm%s", f.rssi, f.configured ? tr(" · schon eingerichtet") : "");
      ui_row(g, (f.name.empty() ? f.mac : f.name).c_str(), v, shb_found_cb, (void *)(intptr_t)i);
    }
  }
}

static void ui_settings_tick_shb() {
  if (!s_scr || s_sec != SEC_SHELLY_BT || ui_input_is_open()) return;
  if (shb_version() * 2 + shb_searching() != s_shb_sig) lv_async_call(shb_rebuild, nullptr);
}
#else
static void build_shb() {}
static void ui_settings_tick_shb() {}
#endif

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

// ---------------------------------------------------------------- Regeln
#if WITH_RULES
// Nur Liste mit Ein/Aus und "Jetzt ausfuehren" – bearbeitet wird im Web-Editor (Blockly, Reiter "Regeln")
static std::vector<lv_obj_t *> s_rule_st, s_rule_sw;
static int s_rule_n = -1;
static void rule_sw_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  rules_set_enabled((int)(intptr_t)lv_event_get_user_data(e), lv_obj_has_state(sw, LV_STATE_CHECKED));
}
static void rule_run_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  rules_run(i);
  app_toast(tr("Regel „%s“ ausgeführt"), rules_name(i).c_str());
}
static void build_rules() {
  s_rule_st.clear();
  s_rule_sw.clear();
  s_rule_n = rules_count();
  heading("Regeln", "Automatisch schalten: wenn etwas passiert (Taster, Bewegungsmelder, Gerät, Uhrzeit), und falls eine "
                    "Bedingung stimmt, dann Aktionen ausführen. Angelegt und bearbeitet werden Regeln im Web-Editor "
                    "(Einstellungen > Regeln) mit Bausteinen.");
  if (s_rule_n == 0) {
    ui_label(s_content, "Noch keine Regel angelegt.", F_SMALL, C_TEXT2);
    return;
  }
  for (int i = 0; i < s_rule_n; i++) {
    lv_obj_t *r = lv_obj_create(s_content);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, C_LINE, 0);
    lv_obj_t *n = ui_label(r, rules_name(i).c_str(), F_BODY, C_TEXT);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_width(n, LV_HOR_RES - 260 - 64 - 160);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_t *st = ui_label(r, rules_status(i).c_str(), F_SMALL, C_TEXT2);
    lv_obj_align(st, LV_ALIGN_TOP_LEFT, 8, 36);
    s_rule_st.push_back(st);
    lv_obj_t *run = ui_icon_button(r, ICON_RIGHT, rule_run_cb, (void *)(intptr_t)i, 48);
    lv_obj_align(run, LV_ALIGN_RIGHT_MID, -84, 0);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_set_style_bg_color(sw, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (rules_enabled(i)) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(sw, rule_sw_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
    s_rule_sw.push_back(sw);
  }
  lv_obj_t *h = ui_label(s_content, "Pfeil: Regel sofort ausführen (ohne Auslöser und Bedingung). Schalter: Regel an/aus.",
                         F_SMALL, C_TEXT2);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(h, lv_pct(100));
}
static void ui_settings_tick_rules() {
  if (!s_scr || s_sec != SEC_RULES) return;
  if (rules_count() != s_rule_n) {   // im Web-Editor gespeichert
    show_section(SEC_RULES);
    return;
  }
  for (size_t i = 0; i < s_rule_st.size(); i++) lv_label_set_text(s_rule_st[i], rules_status((int)i).c_str());
  for (size_t i = 0; i < s_rule_sw.size(); i++) {   // eine Regel kann eine andere umschalten
    bool on = rules_enabled((int)i);
    if (on != lv_obj_has_state(s_rule_sw[i], LV_STATE_CHECKED)) {
      if (on) lv_obj_add_state(s_rule_sw[i], LV_STATE_CHECKED);
      else lv_obj_clear_state(s_rule_sw[i], LV_STATE_CHECKED);
    }
  }
}
#endif

// ---------------------------------------------------------------- Rahmen
static const char *NAV_ICONS[SEC_COUNT] = {ICON_WIFI, ICON_LINK, ICON_MONITOR, ICON_LIST, ICON_GAS, ICON_SUN, ICON_LIGHTNING, ICON_BATTERY, ICON_POWER, ICON_THERMO, ICON_HOUSE, ICON_PLUG, ICON_BULB, ICON_WAND, ICON_LOCK, ICON_INFO};
#if SOC_VICTRON_EDITION
static const char *NAV_TITLES[SEC_COUNT] = {"Netzwerk", "Shelly", "Anzeige", "Geräte", "Gas", "Victron", "", "Akkus (BMS)", "Power Stations", "Sensoren", "Fahrzeug", "Shelly (BT)", "", "", "Sicherung", "Info"};
#else
static const char *NAV_TITLES[SEC_COUNT] = {"Netzwerk", "MQTT", "Anzeige", "Geräte", "Gas/Heizung", "Victron", "Votronic", "Akkus (BMS)", "Power Stations", "Sensoren", "Fahrzeug", "Shelly (BT)", "Zigbee", "Regeln", "Sicherung", "Info"};
#endif

static void show_section(int sec) {
#if WITH_ZB_DIRECT
  if (sec != SEC_ZIGBEE) zb_leave_device();   // Einstellungen eines Zigbee-Geraets nur solange die Seite offen ist
#endif
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
  lv_obj_scroll_to_view(s_nav_items[sec], LV_ANIM_OFF);   // Leiste scrollt: gewaehlten Punkt sichtbar machen
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
    case SEC_VOTRONIC: build_votronic(); break;
#if WITH_BMS
    case SEC_BMS: build_bms(BMS_CAT_BAT); break;
    case SEC_PSTATION: build_bms(BMS_CAT_PS); break;
    case SEC_SENSOR: build_bms(BMS_CAT_SENSOR); break;
#endif
    case SEC_HYMER:   // "Fahrzeug": Hymer Connect und Hobby
      if (hymer_enabled()) build_hymer();
      build_hobby();
      break;
    case SEC_SHELLY_BT:
      build_shb();
      break;
#if WITH_ZB_DIRECT
    case SEC_ZIGBEE: build_zigbee(); break;
#endif
#if WITH_RULES
    case SEC_RULES: build_rules(); break;
#endif
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
  // Leiste scrollt, wenn mehr Punkte da sind als auf den Bildschirm passen (Zeilen werden nicht mehr kleiner)
  lv_obj_add_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(nav, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(nav, LV_SCROLLBAR_MODE_ACTIVE);

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

  // Feste Zeilenhoehe; passen nicht alle auf den Bildschirm, scrollt die Leiste
  auto nav_visible = [](int i) {
    if (i == SEC_HYMER && !hymer_enabled() && !WITH_HOBBY) return false;
    if ((i == SEC_BMS || i == SEC_PSTATION || i == SEC_SENSOR) && !WITH_BMS) return false;
    if (i == SEC_SHELLY_BT && !WITH_SHELLY_BLE) return false;
    if (i == SEC_VOTRONIC && !WITH_VOTRONIC) return false;
    if (i == SEC_ZIGBEE && !WITH_ZB_DIRECT) return false;
    if (i == SEC_RULES && !WITH_RULES) return false;
    return true;
  };
  const int nav_h = 46;
  for (int i = 0; i < SEC_COUNT; i++) {
    lv_obj_t *it = lv_obj_create(nav);
    ui_plain(it);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(it, lv_pct(100), nav_h);
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
    if (!nav_visible(i)) lv_obj_add_flag(it, LV_OBJ_FLAG_HIDDEN);   // nicht eingebaut
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
#if WITH_ZB_DIRECT
  zb_leave_device();
#endif
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
  ui_settings_tick_shb();      // Shelly per Bluetooth: neue Werte, Suche
  ui_settings_tick_sat();      // Satelliten im Einrichtungsmodus kommen und gehen, Stand der Einrichtung
#if WITH_ZB_DIRECT
  ui_settings_tick_zigbee();   // Zigbee-Modul: Stand, Anlernen, Geraete und Werte
#endif
#if WITH_RULES
  ui_settings_tick_rules();    // Regeln: zuletzt ausgeloest, wartet, Fehler
#endif
}
