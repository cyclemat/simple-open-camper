// Satellit: Werte verschicken – MQTT (mit Home-Assistant-Discovery), UDP und Bluetooth-Werbepakete
#pragma once
#include <Arduino.h>

void sat_send_begin();            // nach WLAN-Start; Einstellungen aus g_sat
void sat_send_loop();             // aus loop(), etwa jede Sekunde arbeiten
void sat_send_reconfigure();      // nach geaenderten Einstellungen: Verbindungen neu aufbauen

bool sat_mqtt_connected();
String sat_mqtt_status();
uint32_t sat_udp_sent();          // gesendete UDP-Pakete (fuer die Statusanzeige)
uint32_t sat_adv_count();         // Geraete in der Bluetooth-Rotation
String sat_ble_name_now();        // "Satellite-<MAC>"
void sat_send_adv_suspend(bool on);   // Einrichtungsmodus: normale Werbepakete pausieren
uint32_t sat_cmd_count();         // angenommene Schaltbefehle (UDP/Bluetooth)
