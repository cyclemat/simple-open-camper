// Eingebauter Mini-MQTT-Broker (nur Panels mit viel RAM/PSRAM, z. B. ESP32-P4: -DWITH_BROKER=1)
//
// MQTT 3.1.1: CONNECT (Benutzer/Passwort), SUBSCRIBE/UNSUBSCRIBE mit + und #, PUBLISH QoS 0/1/2 eingehend
// (bestaetigt), ausgehend immer QoS 0, gespeicherte Nachrichten (retained, im PSRAM – auch die grosse Geraeteliste
// von Zigbee2MQTT), Letzter Wille, Keepalive. Keine Warteschlangen fuer abwesende Teilnehmer: immer nur der
// aktuelle Stand. Ab Werk aus (Einstellungen > MQTT > "Eingebauter Broker"), Port 1883.
// Eigener Task nur, solange der Broker an ist. UDP-Wege der Satelliten/Zusatzdisplays laufen unabhaengig weiter.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef WITH_BROKER
#define WITH_BROKER 0
#endif

struct BrokerInfo {
  bool running = false;
  int clients = 0;
  size_t retained = 0, retained_bytes = 0;
  uint32_t msgs = 0;   // weitergeleitete Nachrichten seit dem Start
};

#if WITH_BROKER
void broker_apply();          // Einstellungen uebernehmen: starten bzw. stoppen (nach settings_save aufrufen)
BrokerInfo broker_info();
#else
inline void broker_apply() {}
inline BrokerInfo broker_info() { return BrokerInfo(); }
#endif
