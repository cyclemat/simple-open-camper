// Funkchip-Update (ESP32-C6) – siehe c6_update.h
#include "c6_update.h"
#if WITH_C6_UPDATE
#include <Arduino.h>
#include <stdio.h>
#include "esp32-hal-hosted.h"
#include "ESP_HostedOTA.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app.h"
#include "net.h"
#include "sdcard.h"

#define C6_FILE "/sd/c6.bin"

static volatile int s_state = 0;
static std::string s_msg;
static bool s_from_file = false;

static std::string ver(void (*get)(uint32_t *, uint32_t *, uint32_t *)) {
  uint32_t a = 0, b = 0, c = 0;
  get(&a, &b, &c);
  char t[32];
  snprintf(t, sizeof(t), "%u.%u.%u", (unsigned)a, (unsigned)b, (unsigned)c);
  return t;
}

C6Info c6_info() {
  C6Info i;
  i.ready = hostedIsInitialized();
  i.fw = ver(hostedGetSlaveVersion);
  i.host = ver(hostedGetHostVersion);
  i.update = i.ready && hostedHasUpdate();
  i.file = fs_exists(C6_FILE);
  i.state = s_state;
  i.msg = s_msg;
  return i;
}

static std::string jstr(const std::string &s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    if (c == '\n') o += "\\n";
    else o += c;
  }
  return o + "\"";
}

void c6_info_json(std::string &out) {
  C6Info i = c6_info();
  out = "{\"fw\":" + jstr(i.fw) + ",\"host\":" + jstr(i.host) + ",\"ready\":" + (i.ready ? "true" : "false") +
        ",\"update\":" + (i.update ? "true" : "false") + ",\"file\":" + (i.file ? "true" : "false") +
        ",\"state\":" + std::to_string(i.state) + ",\"msg\":" + jstr(i.msg) + "}";
}

// Abbild aus einer Datei in den Funkchip schreiben (blockiert den Aufrufer, ~1 min)
bool c6_update_file(const char *path, std::string &msg) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    msg = std::string("Datei ") + path + " nicht gefunden (SD-Karte eingesteckt?)";
    return false;
  }
  uint8_t hdr[16] = {0};
  fread(hdr, 1, sizeof(hdr), f);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  // Abbild-Kopf: 0xE9, Chip-Kennung bei Byte 12 (13 = ESP32-C6)
  if (hdr[0] != 0xE9 || (hdr[12] | hdr[13] << 8) != 13) {
    fclose(f);
    msg = std::string(path) + " ist keine Firmware für den ESP32-C6";
    return false;
  }
  if (!hostedIsInitialized()) {
    fclose(f);
    msg = "Funkchip nicht bereit";
    return false;
  }
  log_i("Funkchip-Update aus %s (%ld Byte)", path, size);
  uint8_t *buf = (uint8_t *)heap_caps_malloc(1500, MALLOC_CAP_SPIRAM);
  bool ok = buf && hostedBeginUpdate();
  long done = 0;
  while (ok) {
    size_t n = fread(buf, 1, 1500, f);
    if (!n) break;
    ok = hostedWriteUpdate(buf, n);
    done += n;
  }
  fclose(f);
  if (buf) heap_caps_free(buf);
  ok = ok && done == size && hostedEndUpdate() && hostedActivateUpdate();
  if (!ok) msg = "Schreiben in den Funkchip fehlgeschlagen";
  return ok;
}

static void c6_task(void *) {
  std::string msg;
  bool ok;
  if (s_from_file) ok = c6_update_file(C6_FILE, msg);
  else {
    ok = updateEspHostedSlave();   // Download von espressif.github.io und Schreiben per SDIO (Arduino-Bibliothek)
    if (!ok) msg = "Download oder Schreiben fehlgeschlagen – WLAN mit Internet? Einzelheiten im seriellen Log";
  }
  s_msg = msg;
  s_state = ok ? 2 : 3;
  if (ok) {
    app_toast("Funkmodul aktualisiert – Panel startet neu.");
    vTaskDelay(pdMS_TO_TICKS(2500));
    ESP.restart();
  } else {
    app_toast("Funkmodul-Update fehlgeschlagen: %s", msg.c_str());
  }
  vTaskDelete(nullptr);
}

bool c6_update_start(bool from_file, std::string &err) {
  if (s_state == 1) {
    err = "Das Update läuft schon";
    return false;
  }
  if (!hostedIsInitialized()) {
    err = "Funkchip nicht bereit";
    return false;
  }
  if (from_file && !fs_exists(C6_FILE)) {
    err = "Keine Datei c6.bin auf der SD-Karte";
    return false;
  }
  if (!from_file && !net_connected()) {
    err = "Kein WLAN – das Update kommt aus dem Internet";
    return false;
  }
  s_from_file = from_file;
  s_msg.clear();
  s_state = 1;
  // eigener Task: HTTPS-Download + SDIO-Schreiben (~1 min) blockieren weder Oberflaeche noch Webserver
  if (xTaskCreatePinnedToCore(c6_task, "c6upd", 16384, nullptr, 3, nullptr, 0) != pdPASS) {
    s_state = 3;
    err = "Kein Speicher für den Update-Task";
    return false;
  }
  return true;
}
#endif
