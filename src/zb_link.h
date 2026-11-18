// Zigbee direkt: Verbindung zwischen Panel (ESP32-P4) und Zigbee-Funkmodul (ESP32-H2 im Elecrow-Steckplatz).
//
// Hardware (CrowPanel Advanced 7" V1.2, Schaltplan + Datenblatt des Moduls):
//   P4 GPIO 27 (TXD2) -> Steckplatz J9 Pin 1  -> Modul "U1RXD"
//   P4 GPIO 28 (RXD2) <- Steckplatz J11 Pin 1 <- Modul "U1TXD"
//   Welche H2-GPIOs hinter U1TXD/U1RXD stecken, nennt das Datenblatt nicht – vermutlich die Chip-UART-Pins
//   GPIO 24 (TX) / GPIO 23 (RX). Einstellbar ueber ZB_H2_TX / ZB_H2_RX (h2/platformio.ini).
//
// Protokoll: je Nachricht eine JSON-Zeile (UTF-8, '\n' am Ende), 115200 Baud. Zeilen, die nicht mit '{' beginnen
// (z. B. die Startmeldung des H2-ROM auf derselben Leitung), werden ignoriert.
//
// Panel -> Modul   {"c":"<befehl>", ...}
//   info                                  Stand abfragen -> {"t":"info"}
//   permit   {"s":180}                    Anlernen oeffnen (Sekunden, 0 = schliessen)
//   devices                               alle bekannten Geraete -> je {"t":"dev"}, danach {"t":"devs_end"}
//   remove   {"ieee":"…"}                 Geraet abmelden und vergessen
//   read     {"ieee","ep","cl","at":[…]}  Attribute lesen -> {"t":"attr"}
//   onoff    {"ieee","ep","v":0|1|2}      Aus / Ein / Umschalten (Cluster 0x0006)
//   level    {"ieee","ep","v":0..254,"tt":Zehntelsekunden}
//   cmd      {"ieee","ep","cl","id","pl":"hex","mf":Herstellercode,"dir":0|1}   beliebiger ZCL-Befehl (Treiber)
//   reset_net                             Netz neu bilden (alle Geraete muessen neu anlernen!)
//
// Modul -> Panel   {"t":"<art>", ...}
//   hello    {"fw","ieee"}                nach dem Start
//   info     {"fw","ready","pan","ch","ieee","open","devs"}
//   joined   {"ieee","nwk"}               neues/zurueckgekehrtes Geraet (Abfrage der Endpunkte laeuft)
//   dev      {"ieee","nwk","mf","model","eps":[{"ep","prof","dev","in":[…],"out":[…]}],"done"}
//   left     {"ieee"}
//   attr     {"ieee","ep","cl","at","type","v"}   Meldung oder Leseantwort (v: Zahl, Text oder Hex)
//   open     {"s"}                        Anlernen offen (Restsekunden) / 0 = zu
//   log      {"m"}                        Klartext fuer die Fehlersuche
//   err      {"m"}
#pragma once

#define ZB_LINK_BAUD 115200
#define ZB_LINK_MAX_LINE 1024   // laengste Zeile (Geraet mit vielen Clustern)
#define ZB_MAX_DEVICES 32
