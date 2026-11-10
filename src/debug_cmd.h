// Befehle ueber die serielle Schnittstelle (USB des Panels bzw. UART0) – nur auf Anfrage, kein Dauer-Log.
// Ein Befehl pro Zeile (Enter), Gross/Klein egal:
//   debug | info | ram   Speicher, Laufzeit, Netz, MQTT, Bluetooth-Protokolle auf einmal ausgeben
//   reboot               Panel neu starten
//   help | ?             Befehle auflisten
// Kostet nichts: kein Task, kein Puffer auf dem Heap – nur 32 Byte Zeilenpuffer, abgefragt in loop().
#pragma once

void debug_poll();
