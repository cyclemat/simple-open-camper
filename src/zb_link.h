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
//   driver   {"ieee"}                     Treiber eines Geraets -> {"t":"drv"}
//   set      {"ieee","p":{name:wert,…}}   Werte setzen wie bei Zigbee2MQTT -> {"t":"set","ok":{…},"err":{…}}
//   config   {"ieee"}                     einstellbare Werte -> je {"t":"cfg",…}, dann {"t":"cfg_end"}
//   values                                letzte Werte aller Geraete -> je {"t":"state"}
//   rename   {"ieee","name"}              eigener Name (im Modul gespeichert, "" = loeschen) -> {"t":"dev"}
//   diag     {"on"}                       jede eingehende Zigbee-Nachricht als {"t":"log"} melden
//   time     {"utc","tz"}                 Uhrzeit (fuer den Tuya-Zeitabgleich)
//   drv_lookup {"mf","model"}             Treiberliste durchsuchen (Test ohne Geraet) -> {"t":"drv"}
//
// Modul -> Panel   {"t":"<art>", ...}
//   hello    {"fw","ieee"}                nach dem Start
//   info     {"fw","ready","pan","ch","ieee","open","devs","heap…","drv":{ok,defs,src,size}}
//   joined   {"ieee","nwk"}               neues/zurueckgekehrtes Geraet (Abfrage der Endpunkte laeuft)
//   dev      {"ieee","nwk","name","mf","model","eps":[{"ep","prof","dev","in":[…],"out":[…]}],"done"}
//   left     {"ieee"}
//   state    {"ieee","p":{name:wert,…}}   umgerechnete Werte (wie Zigbee2MQTT: temperature, state, battery …)
//   attr     {"ieee","ep","cl","at","type","v"}   unbekannte Meldung roh (v: Zahl, Text oder Hex)
//   cfg      {"ieee","n","ty","u","mi","ma","st","v":[…],"cur"}   ein einstellbarer Wert (Antwort auf config)
//   cfg_end  {"ieee","n"}
//   open     {"s"}                        Anlernen offen (Restsekunden) / 0 = zu
//   drv      {"ieee"?,"found","vendor","model","desc","flags","exp":[[name,typ,einheit,zugriff,gruppe],…],"dps":n}
//   log      {"m"}                        Klartext fuer die Fehlersuche
//   err      {"m"}
#pragma once

#define ZB_LINK_BAUD 115200
#define ZB_LINK_MAX_LINE 4096   // laengste Zeile (Treiber eines Geraets mit vielen Werten)
#define ZB_MAX_DEVICES 32
