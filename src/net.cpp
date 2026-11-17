#include "net.h"
#include "i18n/i18n.h"
#include "settings.h"
#include "app_config.h"
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "board.h"
#include "lwip/sockets.h"
#include <fcntl.h>
#include <unistd.h>

#if UI_WEB_SETUP
// Einrichtung nur ueber den Browser (UI_WEB_SETUP, z. B. CrowPanel 1.46"): wie WLED 4.3.2.1 – Android zeigt die Anmeldeseite des WLANs nur
// bei einer Adresse, die nicht nach Heimnetz aussieht (bei 192.168.x.x meldet es nur "kein Internet")
static const IPAddress AP_IP(4, 3, 2, 1);
#else
static const IPAddress AP_IP(192, 168, 4, 1);   // grosse Panels: Router-Modus mit Hubs, Adresse bleibt
#endif
static const IPAddress AP_MASK(255, 255, 255, 0);

static uint8_t s_mode = NET_STA;
static volatile bool s_got_ip = false;
static volatile bool s_disconnected = false;
static volatile uint8_t s_disc_reason = 0;
static bool s_was_connected = false;
static bool s_mdns = false;
static bool s_ap = false;
// Panel-WLAN nur zum Einrichten zugeschaltet (UI_WEB_SETUP, nicht der Kombi-Modus): schliesst wieder, sobald das
// Camp-WLAN verbunden ist und 2 min niemand im Panel-WLAN war
static bool s_auto_ap = false;
static uint32_t s_auto_ap_idle = 0;
static bool s_napt = false;
static bool s_scanning = false;
static bool s_scan_req = false, s_scan_failed = false, s_scan_paused_sta = false;
static bool s_scan_fail_pending = false;  // gescheiterte Suche noch nicht an net_scan_take gemeldet
static uint32_t s_scan_req_at = 0, s_scan_try_at = 0;
static uint32_t s_last_ok = 0;  // millis(), zu dem das Camp-WLAN zuletzt verbunden war
static char s_status[80] = "Nicht eingerichtet";
static DNSServer s_dns;
static bool s_dns_on = false;

static void on_wifi_event(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: s_got_ip = true; break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_got_ip = false;
      s_disconnected = true;
      s_disc_reason = info.wifi_sta_disconnected.reason;
      break;
    default: break;
  }
}

static void start_mdns() {
  if (s_mdns) return;
  s_mdns = MDNS.begin(g_settings.hostname.c_str());
  if (s_mdns) MDNS.addService("http", "tcp", 80);
}

static void start_ap() {
  esp_wifi_set_country_code("DE", true);  // Kanaele 1-13
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  String pass = g_settings.ap_pass.length() >= 8 ? g_settings.ap_pass : String("");
  s_ap = WiFi.softAP(g_settings.ap_ssid.c_str(), pass.length() ? pass.c_str() : nullptr, g_settings.ap_channel, 0, 8);
  log_w("Panel-WLAN %s auf Kanal %u: %s", g_settings.ap_ssid.c_str(), g_settings.ap_channel, s_ap ? "aktiv" : "FEHLER");
  // Ohne Internet ueber das Panel-WLAN: alle DNS-Anfragen auf das Panel lenken und per DHCP (Option 114) die Anmelde-
  // seite nennen – das Handy oeffnet dann den Editor von selbst (Captive Portal). Router-Modus der grossen Panels und
  // Geraete ohne Einstellungsmenue (UI_WEB_SETUP: Panel-WLAN nur zum Einrichten). Im Kombi-Modus nicht: dort gibt es Internet.
  if (s_ap && (s_mode == NET_AP || UI_WEB_SETUP)) {
    WiFi.AP.enableDhcpCaptivePortal();
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    if (!s_dns_on) s_dns_on = s_dns.start(53, "*", AP_IP);
  }
  start_mdns();
}

// Kombi-Modus: Teilnehmer im Panel-WLAN bekommen den DNS des Heimnetzes und Internet per NAT
static void enable_forwarding() {
  if (s_napt || s_mode != NET_APSTA) return;
#if defined(CONFIG_LWIP_IPV4_NAPT) && CONFIG_LWIP_IPV4_NAPT
  esp_netif_t *ap = WiFi.AP.netif();
  esp_netif_t *sta = WiFi.STA.netif();
  if (!ap || !sta) return;
  esp_netif_dns_info_t dns;
  if (esp_netif_get_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
    esp_netif_dhcps_stop(ap);
    uint8_t offer_dns = 0x02;  // OFFER_DNS
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns, sizeof(offer_dns));
    esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
    esp_netif_dhcps_start(ap);
  }
  s_napt = esp_netif_napt_enable(ap) == ESP_OK;
  log_i("Weiterleitung ins Heimnetz: %s", s_napt ? "aktiv" : "fehlgeschlagen");
#else
  log_w("NAPT ist in dieser Firmware nicht aktiviert (CONFIG_LWIP_IPV4_NAPT)");
  s_napt = true;  // nicht erneut versuchen
#endif
}

// ---------------------------------------------------------------- Uhrzeit
static const char *s_time_src = "";

static void on_ntp_sync(struct timeval *) {
  s_time_src = "Internet";
  board_rtc_write(time(nullptr));   // Uhrenchip mitziehen, damit die Zeit einen Stromausfall uebersteht
}

void net_begin() {
  s_mode = g_settings.net_mode;
  s_last_ok = millis();
#if UI_WEB_SETUP
  // Kein Einstellungsmenue am Display: ohne Camp-WLAN gleich das Panel-WLAN dazu, damit man die Weboberflaeche
  // erreicht (nicht gespeichert – schliesst wieder, sobald das Camp-WLAN verbunden ist, siehe net_loop)
  if (s_mode == NET_STA && g_settings.wifi_ssid.isEmpty()) {
    s_mode = NET_APSTA;
    s_auto_ap = true;
  }
#endif
  if (s_mode == NET_OFF) {
    // Nur den TCP/IP-Stack (Webserver/UDP brauchen ihn), aber keinen WLAN-Treiber: der kostet den meisten internen RAM
    Network.begin();
    time_t rtc;
    if (!net_time_valid() && board_rtc_read(rtc)) {
      struct timeval tv = {rtc, 0};
      settimeofday(&tv, nullptr);
      s_time_src = "Uhrenchip";
    }
    snprintf(s_status, sizeof(s_status), "WLAN ausgeschaltet");
    log_i("WLAN ausgeschaltet");
    return;
  }
  WiFi.onEvent(on_wifi_event);
  WiFi.persistent(false);
  WiFi.setHostname(g_settings.hostname.c_str());
  WiFi.mode(s_mode == NET_STA ? WIFI_STA : (s_mode == NET_AP ? WIFI_AP : WIFI_AP_STA));
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  configTzTime(g_settings.tz.c_str(), "pool.ntp.org", "time.google.com");
  sntp_set_time_sync_notification_cb(on_ntp_sync);
  // Ohne Internet: Zeit aus dem Uhrenchip, falls die Platine einen hat und er noch laeuft
  time_t rtc;
  if (!net_time_valid() && board_rtc_read(rtc)) {
    struct timeval tv = {rtc, 0};
    settimeofday(&tv, nullptr);
    s_time_src = "Uhrenchip";
    log_i("Uhrzeit aus dem Uhrenchip übernommen");
  }
  if (s_mode != NET_STA) start_ap();
  if (s_mode != NET_AP) net_reconnect();
  else snprintf(s_status, sizeof(s_status), tr("Panel-WLAN %s aktiv"), g_settings.ap_ssid.c_str());
}

uint8_t net_mode() { return s_mode; }

void net_reconnect() {
  if (s_mode == NET_AP || s_mode == NET_OFF) return;
  s_scan_paused_sta = false;
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(false, false);
  s_got_ip = false;
  if (g_settings.wifi_ssid.isEmpty()) {
    snprintf(s_status, sizeof(s_status), "Camp-WLAN nicht eingerichtet");
    return;
  }
  WiFi.setHostname(g_settings.hostname.c_str());
  WiFi.begin(g_settings.wifi_ssid.c_str(), g_settings.wifi_pass.c_str());
  snprintf(s_status, sizeof(s_status), tr("Verbinde mit %s …"), g_settings.wifi_ssid.c_str());
}

static void scan_try();
static void scan_resume_sta();

// ---- WLAN-Suche fuer die Weboberflaeche: der Web-Task setzt nur Merker, gesucht wird in net_loop()
static volatile bool s_web_scan_req = false, s_web_scanning = false, s_web_failed = false;
static std::vector<WifiNet> s_web_nets;
static SemaphoreHandle_t s_web_mtx = nullptr;
static uint32_t s_web_scan_at = 0;

void net_web_scan_request() { s_web_scan_req = true; }

bool net_web_scan_result(std::vector<WifiNet> &out, bool &failed) {
  if (s_web_scan_req || s_web_scanning || !s_web_mtx) return false;
  xSemaphoreTake(s_web_mtx, portMAX_DELAY);
  out = s_web_nets;
  failed = s_web_failed;
  xSemaphoreGive(s_web_mtx);
  return true;
}

static void web_scan_loop() {
  if (s_web_scan_req) {
    s_web_scan_req = false;
    if (!s_web_mtx) s_web_mtx = xSemaphoreCreateMutex();
    s_web_scanning = true;
    s_web_scan_at = millis();
    net_scan_start();
  }
  if (!s_web_scanning) return;
  std::vector<WifiNet> v;
  bool timeout = millis() - s_web_scan_at > 20000;   // z. B. hat die Einstellungsseite am Panel das Ergebnis abgeholt
  if (!net_scan_take(v) && !timeout) return;
  xSemaphoreTake(s_web_mtx, portMAX_DELAY);
  s_web_nets.swap(v);
  s_web_failed = net_scan_failed() || (timeout && v.empty());
  xSemaphoreGive(s_web_mtx);
  s_web_scanning = false;
}

void net_loop() {
  if (s_mode == NET_OFF) return;
  web_scan_loop();
#if UI_WEB_SETUP
  // Camp-WLAN eingerichtet, aber 3 Minuten nicht erreichbar: Panel-WLAN zuschalten (sonst kaeme man nicht mehr an die
  // Weboberflaeche, um es zu aendern)
  if (s_mode == NET_STA && !net_connected() && millis() - s_last_ok > 180000UL) {
    s_mode = NET_APSTA;
    s_auto_ap = true;
    s_auto_ap_idle = 0;
    WiFi.mode(WIFI_AP_STA);
    start_ap();
    log_w("Camp-WLAN nicht erreichbar – Panel-WLAN %s zugeschaltet", g_settings.ap_ssid.c_str());
  }
  // Camp-WLAN wieder da (das Panel sucht im Hintergrund weiter): Panel-WLAN wieder schliessen, sobald 2 min niemand
  // darin ist (sonst risse eine laufende Einrichtung ab)
  if (s_auto_ap && s_mode == NET_APSTA && !g_settings.wifi_ssid.isEmpty()) {
    if (net_connected() && WiFi.softAPgetStationNum() == 0) {
      if (!s_auto_ap_idle) s_auto_ap_idle = millis();
      if (millis() - s_auto_ap_idle > 120000UL) {
        if (s_dns_on) {
          s_dns.stop();
          s_dns_on = false;
        }
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        s_ap = false;
        s_auto_ap = false;
        s_mode = NET_STA;
        log_i("Camp-WLAN verbunden – Panel-WLAN wieder geschlossen");
      }
    } else {
      s_auto_ap_idle = 0;
    }
  }
#endif
  if (s_dns_on) s_dns.processNextRequest();
  if (s_scan_req) scan_try();
  // Nach der Suche wieder verbinden – auch wenn niemand die Ergebnisse abholt (Seite verlassen)
  if (s_scan_paused_sta && !s_scan_req && (!s_scanning || WiFi.scanComplete() != WIFI_SCAN_RUNNING)) scan_resume_sta();
  if (s_mode == NET_AP) {
    static uint32_t t = 0;
    if (millis() - t > 1000) {
      t = millis();
      int n = WiFi.softAPgetStationNum();
      snprintf(s_status, sizeof(s_status), tr("Panel-WLAN %s aktiv, %d %s verbunden"), g_settings.ap_ssid.c_str(), n,
               n == 1 ? "Gerät" : "Geräte");
    }
    return;
  }
  bool now = s_got_ip && WiFi.status() == WL_CONNECTED;
  if (now) s_last_ok = millis();
  if (now && !s_was_connected) {
    snprintf(s_status, sizeof(s_status), tr("Verbunden mit %s"), WiFi.SSID().c_str());
    log_i("WLAN verbunden, IP %s", WiFi.localIP().toString().c_str());
    start_mdns();
    enable_forwarding();
  } else if (!now && s_was_connected) {
    snprintf(s_status, sizeof(s_status), "Verbindung verloren, verbinde neu …");
  }
  if (!now && s_disconnected) {
    s_disconnected = false;
    uint8_t r = s_disc_reason;
    if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT)
      snprintf(s_status, sizeof(s_status), "Passwort falsch?");
    else if (r == WIFI_REASON_NO_AP_FOUND)
      snprintf(s_status, sizeof(s_status), tr("Netz %s nicht gefunden"), g_settings.wifi_ssid.c_str());
  }
  // Waechter: Arduino verbindet nach manchen Trennungsgruenden (z. B. Handshake-Timeout) nicht von selbst neu
  static uint32_t retry_at = 0;
  if (now || g_settings.wifi_ssid.isEmpty() || s_scan_req || s_scanning || s_scan_paused_sta) {
    retry_at = 0;
  } else {
    uint32_t iv = s_mode == NET_STA ? 30000 : 60000;   // Kombi seltener: jeder Versuch stoert das Panel-WLAN
    if (!retry_at) {
      retry_at = millis() + iv;
    } else if ((int32_t)(millis() - retry_at) > 0) {
      retry_at = millis() + iv;
      log_w("Camp-WLAN seit %lu s getrennt – neuer Verbindungsversuch", (unsigned long)((millis() - s_last_ok) / 1000));
      WiFi.disconnect(false, false);
      WiFi.begin(g_settings.wifi_ssid.c_str(), g_settings.wifi_pass.c_str());
    }
  }
  s_was_connected = now;
}

bool net_connected() { return s_mode != NET_AP && s_mode != NET_OFF && s_got_ip && WiFi.status() == WL_CONNECTED; }
String net_ip() { return net_connected() ? WiFi.localIP().toString() : String(""); }
String net_ssid() { return net_connected() ? WiFi.SSID() : g_settings.wifi_ssid; }
int net_rssi() { return net_connected() ? WiFi.RSSI() : 0; }
const char *net_status_text() { return s_status; }

bool net_ap_active() { return s_ap; }
String net_ap_ip() { return s_ap ? AP_IP.toString() : String(""); }
int net_ap_clients() { return s_ap ? WiFi.softAPgetStationNum() : 0; }
String net_editor_ip() {
  String ip = net_ip();
  return ip.length() ? ip : net_ap_ip();
}

static bool port_open(uint32_t ip_be, uint16_t port, int timeout_ms) {
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) return false;
  int fl = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, fl | O_NONBLOCK);
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = ip_be;
  connect(s, (struct sockaddr *)&a, sizeof(a));
  fd_set w;
  FD_ZERO(&w);
  FD_SET(s, &w);
  struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
  bool ok = false;
  if (select(s + 1, nullptr, &w, nullptr, &tv) > 0) {
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
    ok = err == 0;
  }
  close(s);
  return ok;
}

bool net_find_broker(String &host) {
  if (!s_ap) return false;
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK || list.num == 0) return false;
  esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM];
  memset(pairs, 0, sizeof(pairs));
  int n = list.num < ESP_WIFI_MAX_CONN_NUM ? list.num : ESP_WIFI_MAX_CONN_NUM;
  for (int i = 0; i < n; i++) memcpy(pairs[i].mac, list.sta[i].mac, 6);
  if (esp_netif_dhcps_get_clients_by_mac(WiFi.AP.netif(), n, pairs) != ESP_OK) return false;
  for (int i = 0; i < n; i++) {
    if (pairs[i].ip.addr == 0) continue;
    if (port_open(pairs[i].ip.addr, 1883, 600)) {
      IPAddress ip(pairs[i].ip.addr);
      host = ip.toString();
      log_i("MQTT-Broker im Panel-WLAN gefunden: %s", host.c_str());
      return true;
    }
  }
  return false;
}

uint32_t net_offline_ms() {
  if (s_mode == NET_AP || net_connected()) return 0;
  return millis() - s_last_ok;
}

// Die Suche wird in net_loop() gestartet: Solange das Panel noch versucht, sich mit dem gespeicherten
// Camp-WLAN zu verbinden, lehnt der WLAN-Treiber jede Suche ab. Dann den Verbindungsversuch kurz
// anhalten und es erneut probieren; nach der Suche verbindet sich das Panel wieder.
void net_scan_start() {
  if (s_scanning || s_scan_req) return;
  s_scan_req = true;
  s_scan_failed = false;
  s_scan_fail_pending = false;
  s_scan_req_at = millis();
  s_scan_try_at = 0;
}

static void scan_resume_sta() {
  if (!s_scan_paused_sta) return;
  s_scan_paused_sta = false;
  WiFi.setAutoReconnect(true);
  if (!g_settings.wifi_ssid.isEmpty() && !net_connected()) net_reconnect();
}

static void scan_try() {
  if (s_scan_try_at && millis() - s_scan_try_at < 500) return;
  s_scan_try_at = millis();
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true, false) != WIFI_SCAN_FAILED) {
    s_scan_req = false;
    s_scanning = true;
    return;
  }
  if (s_mode != NET_AP && !net_connected() && !s_scan_paused_sta) {
    log_i("WLAN-Suche blockiert (Verbindungsversuch läuft) – halte Verbindungsversuch an");
    s_scan_paused_sta = true;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
  }
  if (millis() - s_scan_req_at > 8000) {
    log_w("WLAN-Suche fehlgeschlagen");
    s_scan_req = false;
    s_scan_failed = true;
    s_scan_fail_pending = true;
    scan_resume_sta();
  }
}

bool net_scan_running() { return s_scanning || s_scan_req; }
bool net_scan_failed() { return s_scan_failed; }

bool net_scan_take(std::vector<WifiNet> &out) {
  if (s_scan_fail_pending) {
    s_scan_fail_pending = false;
    out.clear();
    return true;   // leere Liste; net_scan_failed() liefert true
  }
  if (!s_scanning) return false;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  s_scanning = false;
  if (n < 0) s_scan_failed = true;
  scan_resume_sta();
  out.clear();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || ssid == g_settings.ap_ssid) continue;
    bool dup = false;
    for (auto &w : out)
      if (w.ssid == ssid) {
        dup = true;
        if (WiFi.RSSI(i) > w.rssi) w.rssi = WiFi.RSSI(i);
      }
    if (!dup) out.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  std::sort(out.begin(), out.end(), [](const WifiNet &a, const WifiNet &b) { return a.rssi > b.rssi; });
  WiFi.scanDelete();
  return true;
}

bool net_time_valid() { return time(nullptr) > 1700000000; }

void net_set_time(uint32_t epoch) {
  if (epoch < 1700000000) return;
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  s_time_src = "Browser";
  board_rtc_write(epoch);
  log_i("Uhrzeit vom Browser übernommen");
}

void net_set_time_manual(time_t epoch) {
  if (epoch < 1700000000) return;
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  s_time_src = "von Hand";
  board_rtc_write(epoch);
  log_i("Uhrzeit von Hand gestellt");
}

const char *net_time_source() { return net_time_valid() ? s_time_src : ""; }
