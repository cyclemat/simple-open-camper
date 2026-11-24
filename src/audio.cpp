// Ton am Panel (siehe audio.h): MP3 von der SD-Karte und Hinweistoene ueber den I2S-Verstaerker des P4.
#include "audio.h"
#if WITH_AUDIO
#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/idf_additions.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <algorithm>
#include <mutex>
#include "sdcard.h"
#include "i18n/i18n.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "third_party/minimp3/minimp3.h"

namespace {
enum Cmd : uint8_t { C_PLAY, C_TOGGLE, C_STOP, C_NEXT, C_PREV, C_TONE, C_SOUND, C_SEEK };
struct Msg {
  Cmd cmd;
  int arg;
  char name[96];
};

std::mutex s_mtx;
AudioCfg s_cfg;
AudioState s_st;
uint32_t s_ver = 1;
std::vector<std::string> s_files;   // Musik, sortiert
uint32_t s_files_ms = 0;
QueueHandle_t s_q = nullptr;
TaskHandle_t s_task = nullptr;
int s_last_idx = 0;

const size_t BUF = 16384;           // Lesepuffer (PSRAM)
const int TONE_HZ = 22050;

// ---- nur im Audio-Task benutzt
i2s_chan_handle_t s_tx = nullptr;
uint32_t s_rate = 0;
mp3dec_t *s_dec = nullptr;
uint8_t *s_buf = nullptr;
int16_t *s_pcm = nullptr, *s_out = nullptr;
FILE *s_f = nullptr;
size_t s_blen = 0, s_bpos = 0;
bool s_eof = false;
long s_start = 0, s_size = 0;       // Beginn der MP3-Daten (nach ID3), Dateigroesse
uint64_t s_samples = 0;             // ausgegebene Werte je Kanal seit Titelanfang
uint32_t s_info_ms = 0;
// Spektrum (FFT 256 Punkte ueber die Mitte des Kanals)
volatile bool s_spec_on = false;
uint8_t s_spec[AUDIO_BANDS];
float *s_fft = nullptr;             // 2 x 256 (re, im), nur waehrend der Wiedergabe mit offenem CampAmp
}  // namespace

static void bump() {
  std::lock_guard<std::mutex> l(s_mtx);
  s_ver++;
}

static void save(const AudioCfg &c) {
  Preferences p;
  if (!p.begin("audio", false)) return;
  p.putBool("on", c.on);
  p.putInt("vol", c.vol);
  p.putInt("avol", c.alert_vol);
  p.end();
}

static void amp(bool on) {
  gpio_set_level((gpio_num_t)AUDIO_PIN_AMP, on ? AUDIO_AMP_ON_LEVEL : !AUDIO_AMP_ON_LEVEL);
}

void audio_begin() {
  gpio_config_t g = {};
  g.pin_bit_mask = 1ULL << AUDIO_PIN_AMP;
  g.mode = GPIO_MODE_OUTPUT;
  gpio_config(&g);
  amp(false);   // Verstaerker aus, solange nichts spielt (kein Rauschen)
  Preferences p;
  AudioCfg c;
  if (p.begin("audio", true)) {
    c.on = p.getBool("on", false);
    c.vol = p.getInt("vol", 40);
    c.alert_vol = p.getInt("avol", 70);
    p.end();
  }
  std::lock_guard<std::mutex> l(s_mtx);
  s_cfg = c;
  s_q = xQueueCreate(8, sizeof(Msg));
}

AudioCfg audio_cfg() {
  std::lock_guard<std::mutex> l(s_mtx);
  return s_cfg;
}

static void post(Cmd c, int arg = 0, const std::string &name = "");
static void audio_task(void *);

void audio_set_cfg(const AudioCfg &in) {
  AudioCfg c = in;
  c.vol = constrain(c.vol, 0, 100);
  c.alert_vol = constrain(c.alert_vol, 0, 100);
  bool off;
  {
    std::lock_guard<std::mutex> l(s_mtx);
    off = s_cfg.on && !c.on;
    s_cfg = c;
    s_ver++;
  }
  save(c);
  if (off) post(C_STOP);
}

void audio_set_volume(int vol) {
  AudioCfg c = audio_cfg();
  c.vol = vol;
  audio_set_cfg(c);
}

uint32_t audio_version() {
  std::lock_guard<std::mutex> l(s_mtx);
  return s_ver;
}

AudioState audio_state() {
  std::lock_guard<std::mutex> l(s_mtx);
  return s_st;
}

static std::vector<std::string> list_dir(const char *sub) {
  std::vector<std::string> r;
  if (!sd_ok()) return r;
  DIR *d = opendir((std::string(SD_MOUNT) + sub).c_str());
  if (!d) return r;
  struct dirent *e;
  while ((e = readdir(d)) != nullptr) {
    std::string n = e->d_name;
    if (e->d_type == DT_DIR || n.size() < 5 || n[0] == '.') continue;
    std::string x = n.substr(n.size() - 4);
    for (auto &ch : x) ch = (char)tolower((unsigned char)ch);
    if (x == ".mp3") r.push_back(n);
    if (r.size() >= 500) break;
  }
  closedir(d);
  std::sort(r.begin(), r.end(), [](const std::string &a, const std::string &b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
  return r;
}

std::vector<std::string> audio_files() {
  std::vector<std::string> f = list_dir(AUDIO_MUSIC_DIR);
  std::lock_guard<std::mutex> l(s_mtx);
  s_files = f;
  s_files_ms = millis();
  s_st.count = (int)f.size();
  return f;
}

std::vector<std::string> audio_sounds() { return list_dir(AUDIO_SOUND_DIR); }

static void post(Cmd c, int arg, const std::string &name) {
  if (!s_q) return;
  if (!audio_cfg().on && c != C_STOP) return;
  Msg m = {};
  m.cmd = c;
  m.arg = arg;
  strncpy(m.name, name.c_str(), sizeof(m.name) - 1);
  std::lock_guard<std::mutex> l(s_mtx);
  if (xQueueSend(s_q, &m, 0) != pdTRUE) return;
  // Einmal anlegen, Stapel (24 KB, der MP3-Decoder braucht viel) im PSRAM – intern bleibt nur der Verwaltungsblock.
  // Der Task schreibt nie ins Flash (Einstellungen nur aus dem Zwischenspeicher, Dateien von der SD-Karte), darum
  // ist ein PSRAM-Stapel hier erlaubt. Zwischen den Wiedergaben wartet er auf die Warteschlange.
  if (!s_task) {
    if (xTaskCreatePinnedToCoreWithCaps(audio_task, "audio", 24576, nullptr, 5, &s_task, 1,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
      s_task = nullptr;
  }
}

void audio_play(int idx) { post(C_PLAY, idx); }
void audio_seek(int permille) { post(C_SEEK, constrain(permille, 0, 1000)); }
void audio_shuffle(bool on) {
  std::lock_guard<std::mutex> l(s_mtx);
  s_st.shuffle = on;
  s_ver++;
}
void audio_repeat_one(bool on) {
  std::lock_guard<std::mutex> l(s_mtx);
  s_st.repeat1 = on;
  s_ver++;
}
void audio_spectrum_on(bool on) {
  s_spec_on = on;
  if (!on) memset(s_spec, 0, sizeof(s_spec));
}
void audio_spectrum(uint8_t *bands) {
  std::lock_guard<std::mutex> l(s_mtx);
  memcpy(bands, s_spec, AUDIO_BANDS);
}
void audio_toggle() { post(C_TOGGLE); }
void audio_stop() { post(C_STOP); }
void audio_next() { post(C_NEXT); }
void audio_prev() { post(C_PREV); }
void audio_tone(AudioTone t) { post(C_TONE, (int)t); }
void audio_sound(const std::string &name) {
  if (name.empty()) post(C_TONE, AUDIO_TONE_GONG);
  else post(C_SOUND, 0, name);
}

// ---------------------------------------------------------------- Audio-Task
static void set_state(bool playing, bool paused, int idx, const std::string &title, const std::string &status) {
  std::lock_guard<std::mutex> l(s_mtx);
  s_st.playing = playing;
  s_st.paused = paused;
  s_st.idx = idx;
  s_st.title = title;
  s_st.status = status;
  s_st.count = (int)s_files.size();
  if (!playing && !paused) s_st.pos_s = s_st.dur_s = s_st.kbps = s_st.hz = s_st.ch = 0;
  s_ver++;
}

// Spektrum: FFT (radix 2, 256) mit Hann-Fenster, logarithmisch auf AUDIO_BANDS Balken verteilt
static void spectrum(const int16_t *pcm, int samples, int ch) {
  const int N = 256;
  if (samples < N) return;
  if (!s_fft) s_fft = (float *)heap_caps_malloc(2 * N * sizeof(float), MALLOC_CAP_SPIRAM);
  if (!s_fft) return;
  float *re = s_fft, *im = s_fft + N;
  int off = (samples - N) / 2;
  for (int i = 0; i < N; i++) {
    const int16_t *p = pcm + (off + i) * ch;
    float x = ch == 2 ? (p[0] + p[1]) * 0.5f : p[0];
    re[i] = x * (0.5f - 0.5f * cosf(2 * (float)M_PI * i / (N - 1)));
    im[i] = 0;
  }
  for (int i = 1, j = 0; i < N; i++) {   // Bit-Umkehr
    int bit = N >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
      std::swap(im[i], im[j]);
    }
  }
  for (int len = 2; len <= N; len <<= 1) {
    float a = -2 * (float)M_PI / len, wr = cosf(a), wi = sinf(a);
    for (int i = 0; i < N; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; k++) {
        float ur = re[i + k], ui = im[i + k];
        float vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        float vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr;
        im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr;
        im[i + k + len / 2] = ui - vi;
        float t = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = t;
      }
    }
  }
  uint8_t out[AUDIO_BANDS];
  for (int b = 0; b < AUDIO_BANDS; b++) {   // Bins 1..127 logarithmisch
    int lo = (int)(powf(127.0f, (float)b / AUDIO_BANDS)), hi = (int)(powf(127.0f, (float)(b + 1) / AUDIO_BANDS));
    lo = std::max(lo, 1);
    hi = std::max(hi, lo + 1);
    float m = 0;
    for (int k = lo; k < hi && k < N / 2; k++) m = std::max(m, re[k] * re[k] + im[k] * im[k]);
    float db = 10 * log10f(m + 1.0f);           // ~0 .. ~140
    out[b] = (uint8_t)constrain((int)((db - 70) * 1.6f), 0, 100);
  }
  std::lock_guard<std::mutex> l(s_mtx);
  memcpy(s_spec, out, sizeof(out));
}

static bool i2s_open() {
  if (s_tx) return true;
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  cc.auto_clear = true;
  if (i2s_new_channel(&cc, &s_tx, nullptr) != ESP_OK) {
    s_tx = nullptr;
    return false;
  }
  i2s_std_config_t sc = {};
  sc.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100);
  sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  sc.gpio_cfg.bclk = (gpio_num_t)AUDIO_PIN_BCLK;
  sc.gpio_cfg.ws = (gpio_num_t)AUDIO_PIN_WS;
  sc.gpio_cfg.dout = (gpio_num_t)AUDIO_PIN_DOUT;
  sc.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK || i2s_channel_enable(s_tx) != ESP_OK) {
    i2s_del_channel(s_tx);
    s_tx = nullptr;
    return false;
  }
  s_rate = 44100;
  return true;
}

static void i2s_close() {
  if (!s_tx) return;
  i2s_channel_disable(s_tx);
  i2s_del_channel(s_tx);
  s_tx = nullptr;
  s_rate = 0;
}

static void set_rate(uint32_t hz) {
  if (!s_tx || hz == s_rate || hz < 8000) return;
  i2s_channel_disable(s_tx);
  i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(hz);
  i2s_channel_reconfig_std_clock(s_tx, &clk);
  i2s_channel_enable(s_tx);
  s_rate = hz;
}

// samples = Werte je Kanal; gain 0..32768
static void write_pcm(const int16_t *pcm, int samples, int ch, uint32_t hz, int32_t gain) {
  if (!s_tx || samples <= 0) return;
  set_rate(hz);
  for (int i = 0; i < samples; i++) {
    int32_t l = pcm[ch == 2 ? 2 * i : i], r = pcm[ch == 2 ? 2 * i + 1 : i];
    s_out[2 * i] = (int16_t)((l * gain) >> 15);
    s_out[2 * i + 1] = (int16_t)((r * gain) >> 15);
  }
  size_t w = 0;
  i2s_channel_write(s_tx, s_out, (size_t)samples * 4, &w, pdMS_TO_TICKS(1000));
}

static int32_t gain_of(int vol) {   // Lautstaerke quadratisch (klingt gleichmaessiger)
  float g = constrain(vol, 0, 100) / 100.0f;
  return (int32_t)(g * g * 32768.0f);
}

static void file_close() {
  if (s_f) fclose(s_f);
  s_f = nullptr;
  s_blen = s_bpos = 0;
  s_eof = false;
}

// Datei oeffnen (ID3v2-Kopf ueberspringen), optional ab Byte-Position
static bool file_open(const std::string &path, long at = 0) {
  file_close();
  s_f = fopen(path.c_str(), "rb");
  if (!s_f) return false;
  long start = 0;
  uint8_t h[10];
  if (fread(h, 1, 10, s_f) == 10 && h[0] == 'I' && h[1] == 'D' && h[2] == '3')
    start = 10 + ((h[6] & 0x7F) << 21 | (h[7] & 0x7F) << 14 | (h[8] & 0x7F) << 7 | (h[9] & 0x7F)) + ((h[5] & 0x10) ? 10 : 0);
  fseek(s_f, 0, SEEK_END);
  s_size = ftell(s_f);
  s_start = start;
  fseek(s_f, std::max(start, at), SEEK_SET);
  mp3dec_init(s_dec);
  return true;
}

static long file_pos() { return s_f ? ftell(s_f) - (long)(s_blen - s_bpos) : 0; }

// Einen MP3-Rahmen dekodieren und ausgeben; false = Datei zu Ende
static bool mp3_step(int32_t gain) {
  if (!s_f) return false;
  if (s_blen - s_bpos < 4096 && !s_eof) {
    memmove(s_buf, s_buf + s_bpos, s_blen - s_bpos);
    s_blen -= s_bpos;
    s_bpos = 0;
    size_t n = fread(s_buf + s_blen, 1, BUF - s_blen, s_f);
    if (n == 0) s_eof = true;
    s_blen += n;
  }
  if (s_bpos >= s_blen) return !s_eof ? true : false;
  mp3dec_frame_info_t info;
  int samples = mp3dec_decode_frame(s_dec, s_buf + s_bpos, (int)(s_blen - s_bpos), s_pcm, &info);
  if (info.frame_bytes == 0) {   // kein Rahmen im Puffer: Rest verwerfen bzw. Ende
    if (s_eof) return false;
    s_bpos = s_blen;
    return true;
  }
  s_bpos += info.frame_bytes;
  if (samples > 0) {
    if (s_spec_on) spectrum(s_pcm, samples, info.channels);
    write_pcm(s_pcm, samples, info.channels, info.hz, gain);
    s_samples += samples;
    if (millis() - s_info_ms > 250 && info.hz > 0) {   // Anzeige (CampAmp) 4x je Sekunde
      s_info_ms = millis();
      std::lock_guard<std::mutex> l(s_mtx);
      s_st.pos_s = (int)(s_samples / info.hz);
      s_st.kbps = info.bitrate_kbps;
      s_st.hz = info.hz;
      s_st.ch = info.channels;
      if (info.bitrate_kbps > 0) s_st.dur_s = (int)((int64_t)(s_size - s_start) * 8 / (info.bitrate_kbps * 1000));
      s_ver++;
    }
  }
  return true;
}

// Eingebaute Toene: Sinus mit Abklingen, 22,05 kHz
static void tone_play(AudioTone t, int32_t gain) {
  struct Part { float hz, sec, decay; };
  static const Part GONG[] = {{660, 0.5f, 5}, {523, 0.8f, 3.5f}};
  static const Part BEEP[] = {{1000, 0.15f, 0}, {0, 0.1f, 0}, {1000, 0.15f, 0}};
  static const Part ALARM[] = {{880, 0.25f, 0}, {660, 0.25f, 0}, {880, 0.25f, 0}, {660, 0.25f, 0}, {880, 0.25f, 0}, {660, 0.25f, 0}};
  const Part *p = t == AUDIO_TONE_BEEP ? BEEP : t == AUDIO_TONE_ALARM ? ALARM : GONG;
  int n = t == AUDIO_TONE_BEEP ? 3 : t == AUDIO_TONE_ALARM ? 6 : 2;
  const int CH = 1152;
  for (int k = 0; k < n; k++) {
    int total = (int)(p[k].sec * TONE_HZ);
    float ph = 0, step = 2 * (float)M_PI * p[k].hz / TONE_HZ;
    for (int i0 = 0; i0 < total; i0 += CH) {
      int m = std::min(CH, total - i0);
      for (int i = 0; i < m; i++) {
        float tt = (float)(i0 + i) / TONE_HZ;
        float env = p[k].decay > 0 ? expf(-p[k].decay * tt) : 1.0f;
        int ramp = std::min(std::min(i0 + i, total - (i0 + i)), 200);   // Knackschutz an Anfang/Ende
        env *= ramp / 200.0f;
        s_pcm[i] = p[k].hz > 0 ? (int16_t)(sinf(ph) * 20000.0f * env) : 0;
        ph += step;
        if (ph > 2 * (float)M_PI) ph -= 2 * (float)M_PI;
      }
      write_pcm(s_pcm, m, 1, TONE_HZ, gain);
    }
  }
}

// Eine Sitzung: Puffer und I2S anlegen, spielen, nach 20 s Ruhe alles freigeben
static void session() {
  s_dec = (mp3dec_t *)heap_caps_malloc(sizeof(mp3dec_t), MALLOC_CAP_SPIRAM);
  s_buf = (uint8_t *)heap_caps_malloc(BUF, MALLOC_CAP_SPIRAM);
  s_pcm = (int16_t *)heap_caps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2, MALLOC_CAP_SPIRAM);
  s_out = (int16_t *)heap_caps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2, MALLOC_CAP_SPIRAM);
  bool ok = s_dec && s_buf && s_pcm && s_out && i2s_open();
  if (!ok) set_state(false, false, -1, "", tr("Ton lässt sich nicht starten (Speicher/I2S)."));
  enum { IDLE, PLAY, PAUSE } mode = IDLE;
  int idx = s_last_idx;
  std::string title;
  uint32_t idle_since = millis();
  auto open_track = [&](int i) -> bool {
    std::vector<std::string> f = audio_files();
    if (!sd_ok()) {
      set_state(false, false, -1, "", tr("Keine SD-Karte"));
      return false;
    }
    if (f.empty()) {
      set_state(false, false, -1, "", tr("Keine MP3-Dateien im Ordner /music der SD-Karte"));
      return false;
    }
    idx = ((i % (int)f.size()) + (int)f.size()) % (int)f.size();
    s_last_idx = idx;
    title = f[idx].substr(0, f[idx].size() - 4);
    if (!file_open(std::string(SD_MOUNT) + AUDIO_MUSIC_DIR + "/" + f[idx])) {
      set_state(false, false, idx, title, tr("Datei lässt sich nicht öffnen"));
      return false;
    }
    s_samples = 0;
    set_state(true, false, idx, title, "");
    return true;
  };
  auto next_idx = [&]() {   // Titel zu Ende: gleicher (Wiederholen), zufaelliger oder naechster
    AudioState st = audio_state();
    if (st.repeat1) return idx;
    if (st.shuffle && st.count > 1) {
      int n = (int)(esp_random() % (st.count - 1));
      return n >= idx ? n + 1 : n;
    }
    return idx + 1;
  };
  auto alert = [&](const Msg &m) {   // Musik merken, Hinweis spielen, Musik fortsetzen
    long pos = mode != IDLE ? file_pos() : 0;
    bool had = mode != IDLE;
    file_close();
    amp(true);
    int32_t g = gain_of(audio_cfg().alert_vol);
    if (m.cmd == C_TONE) {
      tone_play((AudioTone)m.arg, g);
    } else if (file_open(std::string(SD_MOUNT) + AUDIO_SOUND_DIR + "/" + m.name)) {
      uint32_t t0 = millis();
      while (mp3_step(g) && millis() - t0 < 120000) {}
      file_close();
    } else {
      tone_play(AUDIO_TONE_GONG, g);   // Datei fehlt: wenigstens ein Gong
    }
    if (had) {
      std::vector<std::string> f = audio_files();
      if (idx >= 0 && idx < (int)f.size()) file_open(std::string(SD_MOUNT) + AUDIO_MUSIC_DIR + "/" + f[idx], pos);
      else mode = IDLE;
    }
    if (mode != PLAY) amp(false);
  };
  while (ok) {
    Msg m;
    TickType_t wait = mode == PLAY ? 0 : pdMS_TO_TICKS(500);
    while (xQueueReceive(s_q, &m, wait) == pdTRUE) {
      wait = 0;
      switch (m.cmd) {
        case C_PLAY:
          if (open_track(m.arg < 0 ? idx : m.arg)) mode = PLAY, amp(true);
          else mode = IDLE;
          break;
        case C_TOGGLE:
          if (mode == PLAY) {
            mode = PAUSE;
            amp(false);
            set_state(false, true, idx, title, "");
          } else if (mode == PAUSE) {
            mode = PLAY;
            amp(true);
            set_state(true, false, idx, title, "");
          } else if (open_track(idx)) {
            mode = PLAY;
            amp(true);
          }
          break;
        case C_STOP:
          file_close();
          mode = IDLE;
          amp(false);
          set_state(false, false, idx, "", "");
          break;
        case C_NEXT:
        case C_PREV:
          if (open_track(idx + (m.cmd == C_NEXT ? 1 : -1))) mode = PLAY, amp(true);
          else mode = IDLE;
          break;
        case C_TONE:
        case C_SOUND:
          alert(m);
          break;
        case C_SEEK:
          if (mode != IDLE && s_f && s_size > s_start) {
            std::vector<std::string> f = audio_files();
            int dur = audio_state().dur_s;
            if (idx >= 0 && idx < (int)f.size() &&
                file_open(std::string(SD_MOUNT) + AUDIO_MUSIC_DIR + "/" + f[idx], s_start + (long)((int64_t)(s_size - s_start) * m.arg / 1000))) {
              s_samples = (uint64_t)dur * m.arg / 1000 * (s_rate ? s_rate : 44100);
            }
          }
          break;
      }
    }
    if (mode == PLAY) {
      if (!mp3_step(gain_of(audio_cfg().vol))) {   // Titel zu Ende: naechster (Ordner im Kreis)
        if (open_track(next_idx())) continue;
        mode = IDLE;
        amp(false);
      }
      idle_since = millis();
      continue;
    }
    if (mode == PAUSE) idle_since = millis();
    if (mode == IDLE && millis() - idle_since > 20000 && uxQueueMessagesWaiting(s_q) == 0) break;   // 20 s Ruhe
  }
  file_close();
  i2s_close();
  amp(false);
  heap_caps_free(s_dec);
  heap_caps_free(s_buf);
  heap_caps_free(s_pcm);
  heap_caps_free(s_out);
  heap_caps_free(s_fft);
  s_fft = nullptr;
  s_dec = nullptr;
  s_buf = nullptr;
  s_pcm = s_out = nullptr;
  if (!ok) xQueueReset(s_q);   // ging nicht: Befehle verwerfen statt endlos neu zu versuchen
}

// Der Task bleibt bestehen (Stapel im PSRAM): auf einen Befehl warten, Sitzung spielen (legt Puffer/I2S an und gibt
// sie nach 20 s Ruhe wieder frei), dann wieder warten. Es gibt so immer nur einen Audio-Task.
static void audio_task(void *) {
  for (;;) {
    Msg m;
    xQueuePeek(s_q, &m, portMAX_DELAY);
    session();
  }
}

// ---------------------------------------------------------------- Kachel "Musik"
bool audio_get(const std::string &key, VictronInfo &v) {
  if (key != AUDIO_KEY) return false;
  AudioCfg c = audio_cfg();
  if (!c.on) return false;
  AudioState st = audio_state();
  v = VictronInfo();
  v.key = AUDIO_KEY;
  v.name = tr("Musik");
  v.kind = VIC_AUDIO;
  v.has = true;
  v.age_s = 0;
  v.status = "OK";
  v.state = st.playing ? 1 : st.paused ? 2 : 0;
  v.note = st.status.empty() ? st.title : st.status;
  v.soc = c.vol;
  v.au_idx = st.idx;
  v.au_count = st.count;
  return true;
}

std::vector<VictronInfo> audio_list() {
  std::vector<VictronInfo> r;
  VictronInfo v;
  if (audio_get(AUDIO_KEY, v)) r.push_back(v);
  return r;
}
#endif
