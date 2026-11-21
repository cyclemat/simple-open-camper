// Einstellungen fuer microlink (sonst aus dem Kconfig-Menue von ESP-IDF). Das Panel baut mit den fertigen Arduino-
// Bibliotheken – darum hier fest, per -include in p4/platformio.ini. Zugangsdaten stehen NICHT hier, sondern kommen zur
// Laufzeit aus den Einstellungen (NVS "tailscale"). Boolesche Schalter: definiert = an (wie sdkconfig.h).
#pragma once

#define CONFIG_ML_MAX_PEERS 16                // gleichzeitige WireGuard-Gegenstellen
#define CONFIG_ML_DERP_VERIFY_CERT 1          // TLS-Zertifikat der DERP-Relais pruefen (Zertifikatsbuendel ist da)
#define CONFIG_ML_DERP_REGION 4               // bevorzugtes Relais: 4 = Frankfurt (Vorgabe von microlink: 9 = Dallas)
#define CONFIG_ML_PRIORITY_PEER_IP ""
#define CONFIG_ML_NVS_MAX_PEERS 64
#define CONFIG_ML_H2_BUFFER_SIZE_KB 512       // im PSRAM
#define CONFIG_ML_JSON_BUFFER_SIZE_KB 512     // im PSRAM
#define CONFIG_ML_PACKET_FILTER_ENFORCE 1     // Regeln (ACL) des Tailnets einhalten
#define CONFIG_ML_FILTER_MAX_ENTRIES 4096
#define CONFIG_ML_UDP_RX_TASK_PRIO 7
#define CONFIG_ML_STOP_TIMEOUT_MS 30000
#define CONFIG_ML_LOG_LEVEL_WARN 1            // nur Warnungen ins Log (Debug-Log bleibt lesbar)
#define CONFIG_ML_LOG_LEVEL 2
#define CONFIG_ML_WIFI_SSID ""
#define CONFIG_ML_WIFI_PASSWORD ""
#define CONFIG_ML_TAILSCALE_AUTH_KEY ""
#define CONFIG_ML_DEVICE_NAME ""
#define CONFIG_ML_CTRL_HOST "controlplane.tailscale.com"
#define CONFIG_ML_CTRL_NOISE_PUBKEY_HEX ""
// Nicht benutzt (bleiben undefiniert = aus): Mobilfunk, Netzwechsel, eigene Einstellungsseite, Subnetz-Router,
// Zero-Copy, OTA-Rueckfall
