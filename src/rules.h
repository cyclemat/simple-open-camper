// Regel-Engine des Panels: "Wenn … und falls … dann …" (Bearbeitung im Web-Editor mit Blockly, Reiter "Regeln").
//
// Regeln liegen als JSON in LittleFS (/rules.json, zusammen mit dem Blockly-Arbeitsbereich fuer den Editor).
// Ausgewertet wird nur bei Ereignissen (Zigbee-Meldung, Kachel aendert sich, Uhrzeit, Zeitgeber) – keine Dauerschleife.
//
// Regel (vom Editor erzeugt):
//   {"name":"Flurlicht","on":true,
//    "trig":[ {"k":"zb","dev":"0x…","p":"action","eq":"1_single"}   Zigbee-Wert (eq optional: nur bei diesem Wert)
//             {"k":"tile","key":"…"}                                 Kachel aendert sich (an/aus, Pegel, Wert)
//             {"k":"time","at":"07:30"}                              taeglich um
//             {"k":"every","s":300}                                  alle n Sekunden
//             {"k":"start"} ],                                       nach dem Start des Panels
//    "cond": Ausdruck oder null,
//    "act":[  {"k":"zb","dev":"0x…","p":"state","v":"TOGGLE"}       Zigbee-Wert setzen
//             {"k":"tile","key":"…","do":"toggle|on|off"}            Kachel schalten (wie Antippen)
//             {"k":"bri","key":"…","v":50}                           Helligkeit %
//             {"k":"dimstep","key":"…","step":20,"min":10}           je Ausloesung dunkler, unter min wieder 100 %
//             {"k":"rule","name":"…","do":"toggle|on|off"}           andere Regel ein-/ausschalten
//             {"k":"wait","s":120}                                   warten (neuer Ausloeser startet die Regel neu)
//             {"k":"msg","text":"…"}                                 Hinweis auf dem Panel
//             {"k":"sound","tone":0|1|2} | {"k":"sound","file":"…"}  Hinweiston (Gong/Piep/Alarm) bzw. Ansage aus /sounds
//             {"k":"music","do":"play|pause|toggle|stop|next|prev"} | {"k":"music","vol":40} ]}   Musik (nur P4)
// Ausdruck: {"op":"and|or","a":[…]} | {"op":"not","a":[x]} | {"op":"cmp","l":W,"c":"=|!=|<|>|<=|>=","r":W}
//           | {"op":"time","from":"20:00","to":"06:00"}
// Wert W:   {"n":Zahl} | {"s":"Text"} | {"b":true} | {"zb":"0x…","p":"temperature"} | {"tile":"…","p":"lit|fill|value"}
#pragma once
#include <ArduinoJson.h>
#include <string>

#ifndef WITH_RULES
#define WITH_RULES 0
#endif

// Verschachtelungstiefe beim Einlesen (Blockly: jede Aktion einer Kette eine Ebene tiefer, ArduinoJson-Vorgabe 10)
#define RULES_NESTING 64   // reicht fuer Ketten mit ~25 Aktionen; jede Ebene kostet Stack

void rules_begin();
void rules_loop();   // aus loop() des Panels (gleicher Task wie die Oberflaeche)
// Zigbee-Meldung (aus zb_direct, beliebiger Task): Werte als JSON-Objekt {"name":wert,…}
void rules_zb_event(const std::string &ieee, const std::string &json_values);

// Fuer Webserver (/api/rules) und Panel-Einstellungen
void rules_json(JsonObject out);             // Regeln, Arbeitsbereich, Zustand, Geraete/Kacheln zur Auswahl
std::string rules_save(const char *body, size_t len);   // {"rules":[…],"ws":{…}} speichern; Fehlertext oder ""
int rules_count();
std::string rules_name(int i);
bool rules_enabled(int i);
void rules_set_enabled(int i, bool on);
std::string rules_status(int i);             // "zuletzt vor 3 min", "wartet 1:20", Fehler …
void rules_run(int i);                       // zum Testen sofort ausfuehren (ohne Ausloeser/Bedingung)
void rules_debug();                          // serieller Befehl "rules": Regeln, Stand, letzte Ereignisse
