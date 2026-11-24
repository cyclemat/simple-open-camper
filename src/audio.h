// Ton am Panel (nur ESP32-P4: Lautsprecher-Buchse des CrowPanel Advanced, I2S-Verstaerker ohne Codec).
// - Musik: MP3-Dateien aus /sd/music auf der SD-Karte (der Reihe nach, am Ende wieder von vorn), Kachel "Musik"
// - Hinweistoene/Ansagen: eingebaute Toene (Gong, Piepton, Alarm) oder MP3 aus /sd/sounds – als Regel-Aktion, z. B.
//   "Gas fast leer" oder als Wecker (Ausloeser "taeglich um"). Laeuft Musik, wird sie dafuer kurz angehalten.
// MP3-Decoder: minimp3 (lieff/minimp3, CC0, src/third_party/minimp3). Der Audio-Task (24 KB Stack) laeuft nur, solange
// etwas spielt; Puffer liegen im PSRAM und werden danach freigegeben. Ab Werk aus.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "victron.h"

#ifndef WITH_AUDIO
#define WITH_AUDIO 0
#endif

// Pins des CrowPanel Advanced 7"/9"/10,1" ESP32-P4 (Elecrow-Beispiele "Playback After Recording" / "Play Music")
#ifndef AUDIO_PIN_BCLK
#define AUDIO_PIN_BCLK 22
#endif
#ifndef AUDIO_PIN_WS
#define AUDIO_PIN_WS 21
#endif
#ifndef AUDIO_PIN_DOUT
#define AUDIO_PIN_DOUT 23
#endif
#ifndef AUDIO_PIN_AMP
#define AUDIO_PIN_AMP 30       // Verstaerker an/aus
#endif
#ifndef AUDIO_AMP_ON_LEVEL
#define AUDIO_AMP_ON_LEVEL 0   // Low = an
#endif

#define AUDIO_KEY "audio:player"
#define AUDIO_MUSIC_DIR "/music"
#define AUDIO_SOUND_DIR "/sounds"

enum AudioTone : uint8_t { AUDIO_TONE_GONG = 0, AUDIO_TONE_BEEP = 1, AUDIO_TONE_ALARM = 2 };

struct AudioCfg {
  bool on = false;
  int vol = 40;          // Musik 0..100 %
  int alert_vol = 70;    // Hinweistoene 0..100 %
};

struct AudioState {
  bool playing = false, paused = false;
  std::string title;     // aktueller Titel (Dateiname ohne .mp3)
  int idx = -1, count = 0;
  std::string status;    // "", "Keine SD-Karte", "Keine MP3-Dateien in /music" …
  // fuer CampAmp: Laufzeit/Dauer (s, Dauer bei VBR geschaetzt), Datenrate, Abtastrate, Kanaele, Zufall/Wiederholen
  int pos_s = 0, dur_s = 0, kbps = 0, hz = 0, ch = 0;
  bool shuffle = false, repeat1 = false;
};
#define AUDIO_BANDS 19     // Spektrum-Balken

#if WITH_AUDIO
void audio_begin();
AudioCfg audio_cfg();
void audio_set_cfg(const AudioCfg &c);
uint32_t audio_version();
AudioState audio_state();
std::vector<std::string> audio_files();    // Musik (Dateinamen in /music, sortiert)
std::vector<std::string> audio_sounds();   // Ansagen (Dateinamen in /sounds)
void audio_play(int idx);                  // Titel aus der Liste (-1 = letzter bzw. erster)
void audio_toggle();                       // Abspielen / Pause
void audio_stop();
void audio_next();
void audio_prev();
void audio_set_volume(int vol);
void audio_seek(int permille);             // springen (0..1000 der Dauer)
void audio_shuffle(bool on);
void audio_repeat_one(bool on);
void audio_spectrum_on(bool on);           // Spektrum rechnen (nur solange CampAmp offen ist)
void audio_spectrum(uint8_t *bands);       // AUDIO_BANDS Werte 0..100
void audio_tone(AudioTone t);              // Hinweiston (unterbricht Musik kurz)
void audio_sound(const std::string &name); // Ansage aus /sounds (unterbricht Musik kurz); "" = Gong
bool audio_get(const std::string &key, VictronInfo &v);   // Kachel "Musik"
std::vector<VictronInfo> audio_list();
#else
inline void audio_begin() {}
inline uint32_t audio_version() { return 0; }
inline bool audio_get(const std::string &, VictronInfo &) { return false; }
inline std::vector<VictronInfo> audio_list() { return {}; }
inline void audio_toggle() {}
inline void audio_tone(AudioTone) {}
inline void audio_sound(const std::string &) {}
#endif
