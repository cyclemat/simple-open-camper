// Hymer Connect – Anmeldung beim Hymer-Server, siehe hymer_cloud.h
#include "hymer_cloud.h"
#include "hymer.h"

#if WITH_HYMER
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <atomic>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "mbedtls/platform.h"


static const char *API = "https://smartrv.erwinhymergroup.com";
static const char *APP_VERSION = "2.10.14";
static const char *USER_AGENT = "okhttp/4.10.0";
// App-Kennung der Hymer-App (OAuth-Client "ehg-prod-mobile-app-technical-user"), Standardwert aus der HA-Integration
// Facetti/hymer-connect-ha-ble (const.py, OAUTH2_BASIC_AUTH_LEGACY_DEFAULT, MIT). Dort als veraltet markiert – aendert
// Hymer ihn, traegt man im Webinterface einen neuen ein (NVS "basic" hat Vorrang).
static const char *DEFAULT_BASIC =
    "Basic ZWhnLXByb2QtbW9iaWxlLWFwcC10ZWNobmljYWwtdXNlcjpaez96Ois3bVFhNXZAb2VlNV0lZEVeUSpxeDh9WXIoYWw1eFNUaC05LERdYm48OzhWbzh1PGclc8OcLShOMyV5";

static std::atomic<int> s_state{HC_IDLE};
static SemaphoreHandle_t s_mtx = nullptr;
static char s_text[120] = "";
static Preferences s_prefs;

static void set_text(int st, const char *fmt, ...) {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_text, sizeof(s_text), fmt, ap);
  va_end(ap);
  xSemaphoreGive(s_mtx);
  s_state = st;
  log_i("Hymer-Konto: %s", s_text);
}

// ---------------------------------------------------------------- mbedtls ins PSRAM
static std::atomic<int> s_psram_users{0};
// Wie CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC (Standard der Firmware)
static void *internal_calloc(size_t n, size_t size) {
  return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void *psram_calloc(size_t n, size_t size) {
  void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : calloc(n, size);
}
void hymer_mbedtls_psram(bool on) {
  if (on) {
    if (s_psram_users++ == 0) mbedtls_platform_set_calloc_free(psram_calloc, free);
  } else if (s_psram_users > 0 && --s_psram_users == 0) {
    mbedtls_platform_set_calloc_free(internal_calloc, free);
  }
}

// ---------------------------------------------------------------- HTTPS
struct Hdr {
  const char *k;
  std::string v;
};

// Antwort wird auf 8 KB begrenzt (die Hymer-Antworten sind klein)
static bool https(esp_http_client_method_t m, const std::string &url, const std::vector<Hdr> &hdr, const std::string &body,
                  int &status, std::string &resp) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.method = m;
  cfg.timeout_ms = 20000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 2048;
  cfg.buffer_size_tx = 2048;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) return false;
  for (auto &h : hdr) esp_http_client_set_header(c, h.k, h.v.c_str());
  status = 0;
  resp.clear();
  bool ok = false;
  if (esp_http_client_open(c, body.size()) == ESP_OK) {
    if (body.empty() || esp_http_client_write(c, body.data(), body.size()) == (int)body.size()) {
      esp_http_client_fetch_headers(c);
      status = esp_http_client_get_status_code(c);
      char buf[512];
      int r;
      while ((r = esp_http_client_read(c, buf, sizeof(buf))) > 0 && resp.size() < 8192) resp.append(buf, r);
      ok = true;
    }
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return ok;
}

static std::string urlenc(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string o;
  for (unsigned char ch : s) {
    if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') o += (char)ch;
    else {
      o += '%';
      o += hex[ch >> 4];
      o += hex[ch & 15];
    }
  }
  return o;
}

static std::string brand_header(const std::string &brand) {
  std::string b = brand.empty() ? "hymer" : brand;
  for (auto &ch : b) ch = tolower((unsigned char)ch);
  b[0] = toupper((unsigned char)b[0]);
  return b + "/" + APP_VERSION;
}

// ---------------------------------------------------------------- Ablauf
static void cloud_task(void *arg) {
  HymerCloudReq *r = (HymerCloudReq *)arg;
  hymer_mbedtls_psram(true);
  int st = 0;
  std::string resp;
  JsonDocument d;
  std::string access, refresh, conf, urn;
  const std::string brand = brand_header(r->brand);
  std::vector<Hdr> api_hdr;

  // 1. Anmelden
  set_text(HC_RUNNING, "Melde mich beim Hymer-Konto an …");
  {
    std::string form = "grant_type=password&username=" + urlenc(r->user) + "&password=" + urlenc(r->pass);
    std::vector<Hdr> h = {{"Accept", "application/json, text/plain, */*"},
                          {"Content-Type", "application/x-www-form-urlencoded"},
                          {"Authorization", r->basic},
                          {"User-Agent", USER_AGENT},
                          {"ehg-smart-caravan-brand", brand}};
    if (!https(HTTP_METHOD_POST, std::string(API) + "/api/v2/oauth/token", h, form, st, resp)) {
      set_text(HC_ERROR, "Hymer-Server nicht erreichbar – ist das WLAN mit Internet verbunden?");
      goto done;
    }
    if (st == 401 || st == 400) {
      set_text(HC_ERROR, "Anmeldung abgelehnt (%d) – E-Mail, Passwort oder App-Kennung prüfen", st);
      goto done;
    }
    if (st != 200 || deserializeJson(d, resp)) {
      set_text(HC_ERROR, "Anmeldung fehlgeschlagen (HTTP %d)", st);
      goto done;
    }
    access = d["access_token"] | "";
    refresh = d["refresh_token"] | "";
    if (access.empty()) {
      set_text(HC_ERROR, "Anmeldung: kein Zugangs-Token erhalten");
      goto done;
    }
  }
  api_hdr = {{"Accept", "application/json, text/plain, */*"},
             {"User-Agent", USER_AGENT},
             {"ehg-smart-caravan-brand", brand},
             {"Authorization", "Bearer " + access}};

  // 2. QR-Code pruefen (Fahrzeug zum Konto)
  set_text(HC_RUNNING, "Prüfe den QR-Code …");
  {
    std::vector<Hdr> h = api_hdr;
    h.push_back({"ehg-token", r->qr});
    if (https(HTTP_METHOD_GET, std::string(API) + "/api/ehg/v1/vehicles/byToken", h, "", st, resp) && st == 200 &&
        !deserializeJson(d, resp)) {
      urn = d["urn"] | "";
    } else {
      // Nicht abbrechen: die HA-Integration braucht das Fahrzeug nur fuer die Cloud, die SCU prueft den QR-Code selbst
      log_w("Hymer-Konto: QR-Code-Abfrage HTTP %d: %.200s", st, resp.c_str());
    }
  }

  // 3. Bestaetigungs-Token
  set_text(HC_RUNNING, "Hole das Bestätigungs-Token …");
  {
    if (!https(HTTP_METHOD_POST, std::string(API) + "/api/ehg/v1/accounts/confirmationToken", api_hdr, "", st, resp) ||
        st != 200 || deserializeJson(d, resp)) {
      set_text(HC_ERROR, "Bestätigungs-Token nicht erhalten (HTTP %d)", st);
      log_w("Hymer-Konto: Antwort %.200s", resp.c_str());
      goto done;
    }
    conf = d["token"] | "";
    if (conf.empty()) {
      set_text(HC_ERROR, "Bestätigungs-Token leer – Antwort des Servers unbekannt");
      log_w("Hymer-Konto: Antwort %.200s", resp.c_str());
      goto done;
    }
  }

  s_prefs.begin("hymer", false);
  s_prefs.putString("qr", r->qr.c_str());
  s_prefs.putString("conf", conf.c_str());
  s_prefs.putString("brand", r->brand.c_str());
  if (r->own_basic) s_prefs.putString("basic", r->basic.c_str());
  else s_prefs.remove("basic");
  s_prefs.putString("mail", r->user.c_str());
  if (!refresh.empty()) s_prefs.putString("art", refresh.c_str());   // fuer spaeteres Erneuern ohne Passwort
  s_prefs.putULong("conf_t", (uint32_t)time(nullptr));
  s_prefs.remove("mob");   // neue Anmeldung bei der SCU noetig
  s_prefs.end();
  set_text(HC_OK, urn.empty() ? "Token erhalten. Jetzt an der SCU „CONNECTION“ drücken und koppeln."
                              : "Token erhalten, Fahrzeug erkannt. Jetzt an der SCU „CONNECTION“ drücken und koppeln.");

done:
  // Passwort im Speicher ueberschreiben
  std::fill(r->pass.begin(), r->pass.end(), 0);
  delete r;
  hymer_mbedtls_psram(false);
  vTaskDelete(nullptr);
}

bool hymer_cloud_start(const HymerCloudReq &r, std::string &err) {
  if (s_state == HC_RUNNING) {
    err = "Die Anmeldung läuft schon";
    return false;
  }
  HymerCloudReq *c = new HymerCloudReq(r);
  if (!c->own_basic) {
    c->basic = DEFAULT_BASIC;
  } else if (c->basic.empty()) {
    s_prefs.begin("hymer", true);
    c->basic = s_prefs.getString("basic", "").c_str();
    s_prefs.end();
  }
  if (c->qr.empty()) c->qr = hymer_cloud_qr();
  while (!c->qr.empty() && isspace((unsigned char)c->qr.back())) c->qr.pop_back();
  while (!c->qr.empty() && isspace((unsigned char)c->qr.front())) c->qr.erase(0, 1);
  if (c->basic.rfind("Basic ", 0) != 0 && !c->basic.empty()) c->basic = "Basic " + c->basic;
  if (c->user.empty() || c->pass.empty()) err = "E-Mail und Passwort fehlen";
  else if (c->basic.empty()) err = "Eigene App-Kennung gewählt, aber keine eingetragen";
  else if (c->qr.empty()) err = "Der Text aus dem QR-Code fehlt";
  if (!err.empty()) {
    delete c;
    return false;
  }
  s_state = HC_RUNNING;
  set_text(HC_RUNNING, "Starte …");
  // Einmaliger Task; 8 KB Stack fuer TLS-Aufbau und Zertifikatspruefung
  if (xTaskCreatePinnedToCore(cloud_task, "hymer_cloud", 8192, c, 2, nullptr, 1) != pdPASS) {
    delete c;
    set_text(HC_ERROR, "Zu wenig Speicher für die Anmeldung");
    err = s_text;
    return false;
  }
  return true;
}

int hymer_cloud_state() { return s_state; }

std::string hymer_cloud_text() {
  if (!s_mtx) return "";
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  std::string t = s_text;
  xSemaphoreGive(s_mtx);
  return t;
}

static std::string pref(const char *k) {
  s_prefs.begin("hymer", true);
  std::string v = s_prefs.getString(k, "").c_str();
  s_prefs.end();
  return v;
}
std::string hymer_cloud_qr() { return pref("qr"); }
std::string hymer_cloud_conf() { return pref("conf"); }
std::string hymer_cloud_brand() { return pref("brand"); }
std::string hymer_cloud_user() { return pref("mail"); }
bool hymer_cloud_has_basic() { return !pref("basic").empty(); }

void hymer_cloud_forget() {
  s_prefs.begin("hymer", false);
  for (const char *k : {"qr", "conf", "brand", "basic", "mail", "art", "conf_t", "mob", "mrt"}) s_prefs.remove(k);
  s_prefs.end();
  s_state = HC_IDLE;
  set_text(HC_IDLE, "");
}

#else
bool hymer_cloud_start(const HymerCloudReq &, std::string &err) {
  err = "nicht eingebaut";
  return false;
}
int hymer_cloud_state() { return HC_IDLE; }
std::string hymer_cloud_text() { return ""; }
std::string hymer_cloud_qr() { return ""; }
std::string hymer_cloud_conf() { return ""; }
std::string hymer_cloud_brand() { return ""; }
bool hymer_cloud_has_basic() { return false; }
std::string hymer_cloud_user() { return ""; }
void hymer_cloud_forget() {}
void hymer_mbedtls_psram(bool) {}
#endif
