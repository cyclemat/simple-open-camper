// Kleiner Updater fuer Geraete mit 4 MB Flash – siehe ../updater_link.h
// Laeuft aus app1 (ota_1), schreibt eine neue volle Firmware nach app0 (ota_0) und startet sie.
// So wenig wie moeglich: WLAN, Zugangspasswort/Einrichtungs-WLAN (devsetup), eine Upload-Seite.
#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include "../devsetup.h"
#include "esp_ota_ops.h"
#include "updater_page.h"
#include "../soc_version.h"

static WebServer s_srv(80);
static String s_name, s_status = "Bereit";
static bool s_upl_ok = false, s_done = false;
static uint32_t s_active_ms = 0;            // letzte Aktivitaet (Upload, Seitenaufruf)
static const uint32_t IDLE_MS = 15 * 60 * 1000UL;

static const esp_partition_t *fw_slot() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}
static bool fw_valid(String *ver = nullptr) {
  const esp_partition_t *p = fw_slot();
  esp_app_desc_t d;
  if (!p || esp_ota_get_partition_description(p, &d) != ESP_OK) return false;
  if (ver) *ver = String(d.project_name) + " " + d.version + " (" + d.date + ")";
  return true;
}

static void back_to_firmware() {
  const esp_partition_t *p = fw_slot();
  if (p && fw_valid() && esp_ota_set_boot_partition(p) == ESP_OK) {
    delay(400);
    ESP.restart();
  }
}

static void h_info() {
  s_active_ms = millis();
  String ver;
  bool ok = fw_valid(&ver);
  String j = "{\"upd\":\"" SOC_VERSION "\",\"name\":\"" + s_name + "\",\"fw\":" + (ok ? "true" : "false") + ",\"ver\":\"" + ver + "\",\"status\":\"" +
             s_status + "\",\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("4.3.2.1")) +
             "\",\"slot\":" + String(fw_slot() ? fw_slot()->size : 0) + "}";
  s_srv.send(200, "application/json", j);
}

static void h_back() {
  if (!fw_valid()) {
    s_srv.send(400, "application/json", "{\"error\":\"Keine gültige Firmware vorhanden – bitte zuerst eine hochladen.\"}");
    return;
  }
  s_srv.send(200, "application/json", "{\"ok\":true}");
  back_to_firmware();
}

static void h_update_done() {
  if (!s_upl_ok) return devsetup::deny();
  bool ok = !Update.hasError() && s_done;
  String err = Update.hasError() ? String(Update.errorString()) : String("Upload unvollständig");
  s_srv.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"" + err + "\"}");
  if (ok) {
    delay(600);
    ESP.restart();   // Update.end(true) hat die neue Firmware schon als Startbereich gesetzt
  }
}

static void h_update_data() {
  HTTPUpload &u = s_srv.upload();
  s_active_ms = millis();
  if (u.status == UPLOAD_FILE_START) {
    s_done = false;
    s_upl_ok = devsetup::upload_allowed();
    if (!s_upl_ok) return;
    const esp_partition_t *p = fw_slot();
    size_t want = s_srv.hasArg("size") ? (size_t)s_srv.arg("size").toInt() : 0;
    if (!p || (want && want > p->size)) {
      s_upl_ok = false;
      s_status = "Datei zu groß";
      return;
    }
    s_status = "Schreibe …";
    if (!Update.begin(want ? want : UPDATE_SIZE_UNKNOWN, U_FLASH)) s_status = Update.errorString();
  } else if (!s_upl_ok) {
    return;
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.write(u.buf, u.currentSize) != u.currentSize) s_status = Update.errorString();
  } else if (u.status == UPLOAD_FILE_END) {
    s_done = Update.end(true);
    s_status = s_done ? "Fertig – starte neu" : String(Update.errorString());
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    s_status = "Abgebrochen";
  }
}

void setup() {
  Serial.begin(115200);
  Preferences p;
  String ssid, pass;
  if (p.begin("upd", true)) {
    ssid = p.getString("ssid", "");
    pass = p.getString("pass", "");
    s_name = p.getString("name", "");
    p.end();
  }
  if (s_name.isEmpty()) {
    uint8_t m[6];
    WiFi.macAddress(m);
    char b[24];
    snprintf(b, sizeof(b), "SOC-Update-%02X%02X", m[4], m[5]);
    s_name = b;
  }
  Serial.printf("Updater %s: %s, WLAN \"%s\"\n", SOC_VERSION, s_name.c_str(), ssid.c_str());
  WiFi.persistent(false);
  WiFi.setHostname(s_name.c_str());
  bool sta = false;
  if (!ssid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (uint32_t t0 = millis(); millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED;) delay(100);
    sta = WiFi.status() == WL_CONNECTED;
  }
  if (!sta) {   // ohne WLAN: Einrichtungs-WLAN wie bei der Ersteinrichtung
    WiFi.mode(WIFI_AP);
    devsetup::ap_start(s_name);
  }
  Serial.printf("Updater: %s\n", sta ? WiFi.localIP().toString().c_str() : "Einrichtungs-WLAN 4.3.2.1");
  devsetup::begin(s_srv);
  s_srv.on("/", HTTP_GET, [] {
    s_active_ms = millis();
    s_srv.send_P(200, "text/html; charset=utf-8", UPD_PAGE);
  });
  s_srv.on("/api/info", HTTP_GET, h_info);
  s_srv.on("/api/back", HTTP_POST, devsetup::guard(h_back));
  s_srv.on("/api/update", HTTP_POST, h_update_done, h_update_data);
  s_srv.begin();
  s_active_ms = millis();
}

void loop() {
  s_srv.handleClient();
  devsetup::loop();
  // Niemand da: nach 15 Minuten zurueck zur Firmware (falls eine gueltige vorhanden ist)
  if (!Update.isRunning() && millis() - s_active_ms > IDLE_MS) {
    s_active_ms = millis();
    back_to_firmware();
  }
  delay(2);
}
