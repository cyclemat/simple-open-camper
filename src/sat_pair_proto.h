// Satelliten per Knopf einrichten (gemeinsam fuer Satellit und Panel)
//
// Ablauf: Am Satelliten BOOT 3 s halten (oder 3x schnell neu starten) -> 3 Minuten Einrichtungsmodus: er wirbt per
// Bluetooth mit dem Dienst SAT_PAIR_SVC und ist verbindbar. Das Panel sieht ihn ("Neuer Satellit gefunden"),
// verbindet sich auf Knopfdruck (ohne Bluetooth-Pairing), liest den oeffentlichen Schluessel aus SAT_PAIR_PUB und
// schreibt in SAT_PAIR_CRED die verschluesselten Daten (sat_pair_crypto.h):
//   {"s":"<WLAN-Name>","p":"<WLAN-Passwort>","k":"<Satelliten-Passwort>"}
// Welches WLAN, entscheidet das Panel nach seiner Betriebsart (Camp-WLAN -> dieses Netz, Router/Kombi -> Panel-WLAN,
// WLAN aus -> keine Einrichtung). Der Satellit speichert, verbindet sich und meldet in SAT_PAIR_STAT (lesen):
//   {"st":"warte|verbinde|ok|fehler","ip":"192.168.1.23","msg":"…"}
// Danach startet er neu und spricht das Panel per UDP an (Bluetooth bleibt als Ersatzweg).
#pragma once

#define SAT_PAIR_SVC  "8e3f0001-5b8a-4c56-9a3c-534f43534154"
#define SAT_PAIR_CRED "8e3f0002-5b8a-4c56-9a3c-534f43534154"
#define SAT_PAIR_STAT "8e3f0003-5b8a-4c56-9a3c-534f43534154"
#define SAT_PAIR_PUB  "8e3f0004-5b8a-4c56-9a3c-534f43534154"   // oeffentlicher P-256-Schluessel des Satelliten (65 Byte)
#define SAT_PAIR_SECONDS 180   // so lange bleibt der Satellit im Einrichtungsmodus
