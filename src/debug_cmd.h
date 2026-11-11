// Befehle ueber die serielle Schnittstelle (USB des Panels bzw. UART0) – nur auf Anfrage, kein Dauer-Log.
// Ein Befehl pro Zeile (Enter), Gross/Klein egal:
//   debug | info | ram   Speicher, Laufzeit, Netz, MQTT, Bluetooth-Protokolle auf einmal ausgeben
//   tasks                alle Tasks mit Stack-Reserve (nie benutzter Stack) – zeigt zu grosse Stacks
//   boot                 freier interner RAM nach jedem Startschritt (siehe debug_mark)
//   ble                  20 s lang alle Bluetooth-Geraete in Reichweite zaehlen (Pakete/s, Hersteller)
//   hymer                Hymer-Verbindung: Kopplung, Status, gesehene SCU-Kandidaten
//   screenshot           nur Display-Tests: Bildspeicher roh senden (tools/screenshot.py)
//   reboot               Panel neu starten
//   help | ?             Befehle auflisten
// Kostet nichts: kein Task, kein Puffer auf dem Heap – nur 32 Byte Zeilenpuffer, abgefragt in loop().
#pragma once

void debug_poll();
// Startschritt merken (nur Zahlen in einer festen Tabelle, Ausgabe erst mit "boot"). name muss konstant sein.
void debug_mark(const char *name);
