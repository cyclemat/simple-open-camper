// Eingebauter Mini-MQTT-Broker – siehe mqtt_broker.h
#include "mqtt_broker.h"
#if WITH_BROKER
#include <Arduino.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "lwip/sockets.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "settings.h"

namespace {
const int PORT = 1883;
const int MAX_CLIENTS = 12;
const size_t MAX_PACKET = 2 * 1024 * 1024;   // groesste Nachricht (Zigbee2MQTT-Geraeteliste ~0,5 MB)

// Puffer im PSRAM (grosse Nachrichten)
struct Buf {
  uint8_t *p = nullptr;
  size_t n = 0, cap = 0;
  bool reserve(size_t want) {
    if (want <= cap) return true;
    size_t nc = cap ? cap : 512;
    while (nc < want) nc *= 2;
    uint8_t *q = (uint8_t *)heap_caps_realloc(p, nc, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!q) return false;
    p = q;
    cap = nc;
    return true;
  }
  bool append(const uint8_t *d, size_t k) {
    if (!reserve(n + k)) return false;
    memcpy(p + n, d, k);
    n += k;
    return true;
  }
  void consume(size_t k) {
    memmove(p, p + k, n - k);
    n -= k;
    if (!n && cap > 4096) clear();   // nach grossen Nachrichten wieder freigeben
  }
  void clear() {
    heap_caps_free(p);
    p = nullptr;
    n = cap = 0;
  }
};

struct Payload {   // gespeicherte Nachricht im PSRAM
  uint8_t *p = nullptr;
  size_t n = 0;
};

struct Client {
  int fd = -1;
  Buf rx;
  bool connected = false;
  std::string id;
  std::vector<std::string> subs;
  uint16_t keepalive = 0;
  uint32_t last_rx = 0;
  bool has_will = false, will_retain = false;
  std::string will_topic;
  std::string will_msg;
};

Client s_cl[MAX_CLIENTS];
std::map<std::string, Payload> s_ret;
size_t s_ret_bytes = 0;
int s_listen = -1;
volatile bool s_run = false, s_task = false;
uint32_t s_msgs = 0;
std::string s_user, s_pass;
SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() {
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
  }
  ~Lock() { xSemaphoreGive(s_mtx); }
};

// ---------------------------------------------------------------- Senden
bool send_all(int fd, const uint8_t *d, size_t n) {
  while (n) {
    int r = send(fd, d, n, 0);
    if (r <= 0) return false;
    d += r;
    n -= r;
  }
  return true;
}

size_t put_len(uint8_t *o, size_t len) {
  size_t i = 0;
  do {
    uint8_t b = len % 128;
    len /= 128;
    if (len) b |= 0x80;
    o[i++] = b;
  } while (len);
  return i;
}

bool send_publish(Client &c, const std::string &topic, const uint8_t *pl, size_t pn, bool retain) {
  uint8_t hdr[8 + 2];
  size_t rem = 2 + topic.size() + pn;
  hdr[0] = 0x30 | (retain ? 1 : 0);
  size_t h = 1 + put_len(hdr + 1, rem);
  uint8_t tl[2] = {(uint8_t)(topic.size() >> 8), (uint8_t)topic.size()};
  return send_all(c.fd, hdr, h) && send_all(c.fd, tl, 2) && send_all(c.fd, (const uint8_t *)topic.data(), topic.size()) &&
         (pn == 0 || send_all(c.fd, pl, pn));
}

// ---------------------------------------------------------------- Themen
// MQTT-Platzhalter: "+" eine Ebene, "#" Rest; Themen mit "$" vorne passen nicht auf Platzhalter am Anfang
bool match(const std::string &f, const std::string &t) {
  if (!t.empty() && t[0] == '$' && !f.empty() && (f[0] == '+' || f[0] == '#')) return false;
  size_t i = 0, j = 0;
  while (i < f.size()) {
    if (f[i] == '#') return true;
    if (f[i] == '+') {
      while (j < t.size() && t[j] != '/') j++;
      i++;
    } else {
      size_t fe = f.find('/', i), te = t.find('/', j);
      if (fe == std::string::npos) fe = f.size();
      if (te == std::string::npos) te = t.size();
      if (f.compare(i, fe - i, t, j, te - j) != 0) return false;
      i = fe;
      j = te;
    }
    if (i == f.size()) return j == t.size();
    if (j == t.size()) return f.compare(i, std::string::npos, "/#") == 0;   // "a/#" passt auch auf "a"
    if (f[i] != '/' || t[j] != '/') return false;
    i++;
    j++;
  }
  return j == t.size();
}

void close_client(Client &c, bool will);

void deliver(const std::string &topic, const uint8_t *pl, size_t pn) {
  for (auto &c : s_cl) {
    if (c.fd < 0 || !c.connected) continue;
    for (auto &f : c.subs)
      if (match(f, topic)) {
        if (!send_publish(c, topic, pl, pn, false)) close_client(c, true);
        else s_msgs++;
        break;
      }
  }
}

void store_retained(const std::string &topic, const uint8_t *pl, size_t pn) {
  Lock l;
  auto it = s_ret.find(topic);
  if (it != s_ret.end()) {
    s_ret_bytes -= it->second.n;
    heap_caps_free(it->second.p);
    s_ret.erase(it);
  }
  if (!pn) return;   // leere Nachricht loescht
  Payload p;
  p.p = (uint8_t *)heap_caps_malloc(pn, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p.p) return;
  memcpy(p.p, pl, pn);
  p.n = pn;
  s_ret[topic] = p;
  s_ret_bytes += pn;
}

void close_client(Client &c, bool will) {
  if (c.fd < 0) return;
  int fd = c.fd;
  c.fd = -1;
  ::close(fd);
  bool w = will && c.connected && c.has_will;
  std::string wt = c.will_topic, wm = c.will_msg;
  bool wr = c.will_retain;
  c.rx.clear();
  c.connected = false;
  c.subs.clear();
  c.has_will = false;
  c.id.clear();
  if (w) {   // Letzter Wille (z. B. "offline")
    if (wr) store_retained(wt, (const uint8_t *)wm.data(), wm.size());
    deliver(wt, (const uint8_t *)wm.data(), wm.size());
  }
}

// ---------------------------------------------------------------- Pakete
struct Rd {
  const uint8_t *p;
  size_t n, i = 0;
  bool ok = true;
  uint16_t u16() {
    if (i + 2 > n) { ok = false; return 0; }
    uint16_t v = (p[i] << 8) | p[i + 1];
    i += 2;
    return v;
  }
  std::string str() {
    uint16_t l = u16();
    if (!ok || i + l > n) { ok = false; return ""; }
    std::string s((const char *)p + i, l);
    i += l;
    return s;
  }
};

void on_connect(Client &c, const uint8_t *p, size_t n) {
  Rd r{p, n};
  std::string proto = r.str();
  if (!r.ok || r.i + 4 > n) return close_client(c, false);
  uint8_t level = p[r.i++], flags = p[r.i++];
  c.keepalive = r.u16();
  auto connack = [&](uint8_t rc) {
    uint8_t a[4] = {0x20, 0x02, 0x00, rc};
    send_all(c.fd, a, 4);
  };
  if ((proto != "MQTT" && proto != "MQIsdp") || level < 3 || level > 4) {   // MQTT 5 nicht
    connack(0x01);
    return close_client(c, false);
  }
  c.id = r.str();
  if (flags & 0x04) {
    c.will_topic = r.str();
    c.will_msg = r.str();
    c.has_will = r.ok;
    c.will_retain = (flags & 0x20) != 0;
  }
  std::string user = (flags & 0x80) ? r.str() : std::string(), pass = (flags & 0x40) ? r.str() : std::string();
  if (!r.ok) return close_client(c, false);
  if (!s_pass.empty() && (user != s_user || pass != s_pass)) {
    connack(0x04);   // Benutzer/Passwort falsch
    return close_client(c, false);
  }
  if (c.id.empty()) c.id = "anon-" + std::to_string(c.fd);
  for (auto &o : s_cl)   // gleiche Kennung: alte Verbindung beenden
    if (&o != &c && o.fd >= 0 && o.connected && o.id == c.id) close_client(o, false);
  c.connected = true;
  connack(0x00);
}

void on_publish(Client &c, uint8_t fl, const uint8_t *p, size_t n) {
  Rd r{p, n};
  std::string topic = r.str();
  uint8_t qos = (fl >> 1) & 3;
  uint16_t id = qos ? r.u16() : 0;
  if (!r.ok || topic.empty()) return close_client(c, true);
  const uint8_t *pl = p + r.i;
  size_t pn = n - r.i;
  if (qos == 1) {
    uint8_t a[4] = {0x40, 0x02, (uint8_t)(id >> 8), (uint8_t)id};
    send_all(c.fd, a, 4);
  } else if (qos == 2) {   // vereinfacht: gleich weiterleiten, PUBREC; PUBREL beantworten wir mit PUBCOMP
    uint8_t a[4] = {0x50, 0x02, (uint8_t)(id >> 8), (uint8_t)id};
    send_all(c.fd, a, 4);
  }
  if (fl & 1) store_retained(topic, pl, pn);
  deliver(topic, pl, pn);
}

void on_subscribe(Client &c, const uint8_t *p, size_t n) {
  Rd r{p, n};
  uint16_t id = r.u16();
  std::vector<std::string> fs;
  while (r.ok && r.i < n) {
    std::string f = r.str();
    if (!r.ok || r.i >= n) break;
    r.i++;   // gewuenschte QoS
    fs.push_back(f);
  }
  if (!r.ok || fs.empty()) return close_client(c, false);
  uint8_t a[4 + 32];
  size_t cnt = std::min<size_t>(fs.size(), 32);
  a[0] = 0x90;
  a[1] = 2 + cnt;
  a[2] = id >> 8;
  a[3] = id;
  for (size_t i = 0; i < cnt; i++) a[4 + i] = 0x00;   // immer QoS 0 gewaehrt
  if (!send_all(c.fd, a, 4 + cnt)) return close_client(c, true);
  for (auto &f : fs) {
    if (std::find(c.subs.begin(), c.subs.end(), f) == c.subs.end()) c.subs.push_back(f);
    // gespeicherte Nachrichten zu diesem Filter sofort schicken
    std::vector<std::pair<std::string, Payload>> hits;
    {
      Lock l;
      for (auto &kv : s_ret)
        if (match(f, kv.first)) hits.push_back(kv);
    }
    for (auto &h : hits) {
      bool fail = false;
      {
        Lock l;   // Payload darf waehrend des Sendens nicht freigegeben werden
        auto it = s_ret.find(h.first);
        if (it != s_ret.end()) fail = !send_publish(c, it->first, it->second.p, it->second.n, true);
      }
      if (fail) return close_client(c, true);
    }
  }
}

void on_unsubscribe(Client &c, const uint8_t *p, size_t n) {
  Rd r{p, n};
  uint16_t id = r.u16();
  while (r.ok && r.i < n) {
    std::string f = r.str();
    if (!r.ok) break;
    c.subs.erase(std::remove(c.subs.begin(), c.subs.end(), f), c.subs.end());
  }
  uint8_t a[4] = {0xB0, 0x02, (uint8_t)(id >> 8), (uint8_t)id};
  send_all(c.fd, a, 4);
}

// Vollstaendige Pakete aus dem Empfangspuffer verarbeiten
void process(Client &c) {
  while (c.fd >= 0 && c.rx.n >= 2) {
    size_t len = 0, mul = 1, i = 1;
    bool complete = false;
    while (i < c.rx.n && i <= 4) {
      uint8_t b = c.rx.p[i++];
      len += (b & 0x7F) * mul;
      mul *= 128;
      if (!(b & 0x80)) {
        complete = true;
        break;
      }
    }
    if (!complete) {
      if (i > 4) close_client(c, true);
      return;
    }
    if (len > MAX_PACKET) return close_client(c, true);
    if (c.rx.n < i + len) return;   // noch nicht alles da
    uint8_t type = c.rx.p[0] >> 4, fl = c.rx.p[0] & 0x0F;
    const uint8_t *body = c.rx.p + i;
    if (!c.connected && type != 1) return close_client(c, false);
    switch (type) {
      case 1: on_connect(c, body, len); break;
      case 3: on_publish(c, fl, body, len); break;
      case 6: {   // PUBREL (QoS 2) -> PUBCOMP
        uint8_t a[4] = {0x70, 0x02, len >= 2 ? body[0] : (uint8_t)0, len >= 2 ? body[1] : (uint8_t)0};
        send_all(c.fd, a, 4);
        break;
      }
      case 8: on_subscribe(c, body, len); break;
      case 10: on_unsubscribe(c, body, len); break;
      case 12: {   // PINGREQ
        uint8_t a[2] = {0xD0, 0x00};
        send_all(c.fd, a, 2);
        break;
      }
      case 14: return close_client(c, false);   // DISCONNECT: ohne Letzten Willen
      default: break;                            // PUBACK/PUBREC/PUBCOMP von Clients: nichts zu tun (wir senden QoS 0)
    }
    if (c.fd >= 0) c.rx.consume(i + len);
  }
}

void task(void *) {
  s_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int one = 1;
  setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (s_listen < 0 || bind(s_listen, (sockaddr *)&a, sizeof(a)) < 0 || listen(s_listen, 4) < 0) {
    log_w("Broker: Port %d nicht verfuegbar", PORT);
    if (s_listen >= 0) ::close(s_listen);
    s_listen = -1;
    s_task = false;
    vTaskDelete(nullptr);
    return;
  }
  log_w("Broker: laeuft auf Port %d", PORT);
  uint8_t *tmp = (uint8_t *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  while (s_run) {
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(s_listen, &rs);
    int mx = s_listen;
    for (auto &c : s_cl)
      if (c.fd >= 0) {
        FD_SET(c.fd, &rs);
        mx = std::max(mx, c.fd);
      }
    timeval tv = {1, 0};
    int n = select(mx + 1, &rs, nullptr, nullptr, &tv);
    uint32_t now = millis();
    if (n > 0 && FD_ISSET(s_listen, &rs)) {
      int fd = accept(s_listen, nullptr, nullptr);
      if (fd >= 0) {
        Client *slot = nullptr;
        for (auto &c : s_cl)
          if (c.fd < 0) {
            slot = &c;
            break;
          }
        if (!slot) {
          ::close(fd);
        } else {
          timeval st = {3, 0};   // langsamer Empfaenger blockiert den Broker hoechstens 3 s
          setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof(st));
          setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
          *slot = Client();
          slot->fd = fd;
          slot->last_rx = now;
        }
      }
    }
    for (auto &c : s_cl) {
      if (c.fd < 0) continue;
      if (n > 0 && FD_ISSET(c.fd, &rs)) {
        int r = recv(c.fd, tmp, 4096, 0);
        if (r <= 0 || !c.rx.append(tmp, r)) {
          close_client(c, true);
          continue;
        }
        c.last_rx = now;
        process(c);
        continue;
      }
      // Keepalive: 1,5-fache Zeit ohne Paket -> Verbindung tot; ohne CONNECT nach 10 s schliessen
      uint32_t ka = c.connected ? (c.keepalive ? c.keepalive * 1500UL : 0) : 10000UL;
      if (ka && now - c.last_rx > ka) close_client(c, true);
    }
  }
  for (auto &c : s_cl) close_client(c, false);
  ::close(s_listen);
  s_listen = -1;
  heap_caps_free(tmp);
  log_w("Broker: beendet");
  s_task = false;
  vTaskDelete(nullptr);
}
}  // namespace

void broker_apply() {
  Settings st = settings_copy();
  bool want = st.broker_on && st.net_mode != NET_OFF;
  s_user = st.broker_user.c_str();
  s_pass = st.broker_pass.c_str();
  if (want && !s_task) {
    s_run = true;
    s_task = true;
    xTaskCreatePinnedToCore(task, "broker", 6144, nullptr, 3, nullptr, 1);
  } else if (!want && s_task) {
    s_run = false;   // Task beendet sich innerhalb von 1 s
  }
}

BrokerInfo broker_info() {
  BrokerInfo i;
  i.running = s_task && s_listen >= 0;
  for (auto &c : s_cl)
    if (c.fd >= 0 && c.connected) i.clients++;
  {
    Lock l;
    i.retained = s_ret.size();
    i.retained_bytes = s_ret_bytes;
  }
  i.msgs = s_msgs;
  return i;
}
#endif
