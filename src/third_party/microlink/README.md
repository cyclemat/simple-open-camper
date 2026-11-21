# microlink (Tailscale-kompatibler Client)

Quelle: https://github.com/CamM2325/microlink (Komponente `components/microlink`, Stand 1cb8d1c vom 2026-11-18, v2.3.0).
Lizenz: MIT (`LICENSE`), mitgeliefertes wireguard-lwip: BSD-3-Clause (`wireguard_lwip/LICENSE*`),
X25519 nach D. J. Bernstein (public domain, `x25519-license.txt`).

Unveraendert uebernommen; weggelassen: Mobilfunk (ml_cellular, ml_at_socket), Netzwechsel (ml_net_switch) und die eigene
Einstellungsseite (ml_config_httpd). Einstellungen statt Kconfig: `include/ml_kconfig.h` (eingebunden am Anfang von microlink.h, ml_filter.h, ml_nat66.h und
wireguard-platform.h – die einzige Aenderung am Fremdcode). Nur im P4 gebaut (WITH_TAILSCALE),
Anbindung: `src/tailscale.*`.
