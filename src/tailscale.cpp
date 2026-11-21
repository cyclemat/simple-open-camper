// Tailscale (Beta) – siehe tailscale.h
#include "tailscale.h"
#if WITH_TAILSCALE
#include <Arduino.h>
#include <Preferences.h>
#include "net.h"
#include "settings.h"
extern "C" {
#include "microlink.h"
}

namespace {

SemaphoreHandle_t s_mx = nullptr;   // schuetzt s_ml und die Texte
microlink_t *s_ml = nullptr;
bool s_on = false;
std::string s_key, s_name, s_url;
std::string s_cfg_name, s_cfg_key;  // muessen gueltig bleiben, solange microlink laeuft (Konfiguration haelt Zeiger)
volatile int s_state = -1;
volatile bool s_restart = false, s_logout = false;
TaskHandle_t s_task = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_mx); }
};

void load() {
  Preferences p;
  if (!p.begin("tailscale", true)) return;
  s_on = p.getBool("on", false);
  s_key = p.getString("key", "").c_str();
  s_name = p.getString("name", "").c_str();
  p.end();
}

void save() {
  Preferences p;
  if (!p.begin("tailscale", false)) return;
  p.putBool("on", s_on);
  p.putString("key", s_key.c_str());
  p.putString("name", s_name.c_str());
  p.end();
}

// Tailnet-Name: klein, nur a-z 0-9 und '-', hoechstens 63 Zeichen
std::string dev_name() {
  std::string n = s_name.empty() ? std::string(g_settings.hostname.c_str()) : s_name, r;
  for (char c : n) {
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') r += c;
    else if (c == ' ' || c == '_' || c == '.') r += '-';
  }
  if (r.empty()) r = "soc-panel";
  if (r.size() > 63) r.resize(63);
  return r;
}

const char *state_text(int s) {
  switch (s) {
    case -1: return "aus";
    case ML_STATE_IDLE: return "bereit";
    case ML_STATE_WIFI_WAIT: return "wartet auf WLAN";
    case ML_STATE_CONNECTING: return "verbindet …";
    case ML_STATE_REGISTERING: return "meldet sich an …";
    case ML_STATE_CONNECTED: return "verbunden";
    case ML_STATE_RECONNECTING: return "verbindet neu …";
    case ML_STATE_ERROR: return "Fehler";
    case ML_STATE_AUTH_FAILED: return "Anmeldung abgelehnt – Auth-Key abgelaufen oder Gerät nicht freigegeben";
    case ML_STATE_NEEDS_LOGIN: return "wartet auf Anmeldung (Link öffnen)";
    default: return "?";
  }
}

void state_cb(microlink_t *, microlink_state_t st, void *) {
  s_state = st;
  log_i("Tailscale: %s", state_text(st));
}

void start() {
  microlink_config_t c = {};
  {
    Lock l;
    s_cfg_name = dev_name();
    s_cfg_key = s_key;
  }
  c.auth_key = s_cfg_key.empty() ? nullptr : s_cfg_key.c_str();   // ohne Key: Anmeldung per Link
  c.device_name = s_cfg_name.c_str();
  c.enable_derp = c.enable_stun = c.enable_disco = true;
  c.max_peers = 16;
  microlink_t *ml = microlink_init(&c);
  if (!ml) {
    log_w("Tailscale: microlink_init fehlgeschlagen (Speicher?)");
    return;
  }
  microlink_set_state_callback(ml, state_cb, nullptr);
  if (microlink_start(ml) != ESP_OK) {
    log_w("Tailscale: Start fehlgeschlagen");
    microlink_destroy(ml);
    return;
  }
  Lock l;
  s_ml = ml;
  s_url.clear();
  log_i("Tailscale: gestartet als \"%s\"%s", s_cfg_name.c_str(), c.auth_key ? " mit Auth-Key" : " (Anmeldung per Link)");
}

void stop() {
  microlink_t *ml;
  {
    Lock l;
    ml = s_ml;
    s_ml = nullptr;
    s_url.clear();
  }
  if (!ml) return;
  microlink_stop(ml);   // kann dauern (bis CONFIG_ML_STOP_TIMEOUT_MS)
  microlink_destroy(ml);
  s_state = -1;
  log_i("Tailscale: gestoppt");
}

// Steuer-Task: nur solange Tailscale eingeschaltet ist (bzw. bis es sauber gestoppt ist)
void task(void *) {
  uint32_t last_try = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (s_logout) {
      s_logout = false;
      stop();
      microlink_factory_reset();   // Geraeteschluessel und Gegenstellen loeschen
      log_i("Tailscale: abgemeldet, Schluessel geloescht");
      last_try = 0;
    }
    if (s_restart) {
      s_restart = false;
      stop();
      last_try = 0;
    }
    if (!s_on) {
      stop();
      break;
    }
    bool have;
    {
      Lock l;
      have = s_ml != nullptr;
    }
    // Start erst mit WLAN; nach einem Fehlschlag hoechstens alle 30 s neu versuchen
    if (!have && net_connected() && (!last_try || millis() - last_try > 30000)) {
      last_try = millis();
      start();
    }
    if (have && s_state == ML_STATE_NEEDS_LOGIN) {
      char b[MICROLINK_AUTH_URL_MAX];
      Lock l;
      if (s_ml && microlink_get_auth_url(s_ml, b, sizeof(b)) == ESP_OK && s_url != b) {
        s_url = b;
        log_i("Tailscale: Anmelde-Link %s", b);
      }
    } else if (have && s_state == ML_STATE_CONNECTED && !s_url.empty()) {
      Lock l;
      s_url.clear();
    }
  }
  s_task = nullptr;
  vTaskDelete(nullptr);
}

void ensure_task() {
  if (s_on && !s_task) xTaskCreate(task, "ts", 4096, nullptr, 2, &s_task);
}

}  // namespace

void ts_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  load();
  ensure_task();
}

TsStatus ts_status() {
  TsStatus r;
  r.avail = true;
  if (!s_mx) return r;
  Lock l;
  r.on = s_on;
  r.has_key = !s_key.empty();
  r.running = s_ml != nullptr;
  r.code = s_ml ? s_state : -1;
  r.state = s_on ? state_text(r.code) : "aus";
  if (s_on && !s_ml) r.state = net_connected() ? "startet …" : "wartet auf WLAN";
  r.url = s_url;
  r.name = dev_name();
  if (s_ml && s_state == ML_STATE_CONNECTED) {
    char b[20];
    microlink_ip_to_str(microlink_get_vpn_ip(s_ml), b);
    r.ip = b;
    r.peers = microlink_get_peer_count(s_ml);
  }
  return r;
}

void ts_set(const bool *on, const std::string *name, const std::string *key) {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  bool changed = false;
  {
    Lock l;
    if (name && *name != s_name) {
      s_name = *name;
      changed = true;
    }
    if (key && *key != s_key) {
      s_key = *key;
      changed = true;
    }
    if (on) s_on = *on;
  }
  save();
  if (changed && s_ml) s_restart = true;   // neuer Name/Key: neu anmelden
  ensure_task();                            // aus: der Task stoppt microlink und beendet sich
}

void ts_logout() {
  s_logout = true;
  ensure_task();
}

std::string ts_status_line() {
  if (!s_on) return std::string();
  TsStatus s = ts_status();
  return "Tailscale " + s.state + (s.ip.empty() ? std::string() : " " + s.ip + " (" + std::to_string(s.peers) + " Geräte)");
}

#endif   // WITH_TAILSCALE
