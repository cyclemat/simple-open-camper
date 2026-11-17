// ESP32-P4 (Revision v1.x, Bibliotheken esp32p4_es): Die Schluesselrechnung des Bluetooth-Stacks (NimBLE/TinyCrypt mit
// dem ECC-Rechenwerk des Chips) rechnet falsch – Selbsttest "smtest": a*B != b*A. Folge: jede Kopplung mit Secure
// Connections scheitert mit "DHKey Check Failed" (z. B. Super B Epsilon V2 mit PIN). Abhilfe ohne Framework-Neubau:
// die beiden Funktionen werden beim Linken umgeleitet (-Wl,--wrap=..., p4/platformio.ini) und hier in Software
// gerechnet (micro-ecc, P-256). Gleiche Schnittstelle wie ble_sm_alg.c: Schluessel little-endian, 17 = Fehler.
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "uecc_p256.h"
#include "micro-ecc/uECC.c.inc"
#include <esp_random.h>
#include <string.h>

#define P256_ERR 17   // BLE_HS_EUNKNOWN, wie im Original

static int p256_rng(uint8_t *dest, unsigned size) {
  esp_fill_random(dest, size);
  return 1;
}
static void swap_buf(uint8_t *dst, const uint8_t *src, int n) {
  for (int i = 0; i < n; i++) dst[i] = src[n - 1 - i];
}

int __wrap_ble_sm_alg_gen_key_pair(uint8_t *pub, uint8_t *priv) {
  uint8_t pk[64], k[32];
  uECC_set_rng(p256_rng);
  if (!uECC_make_key(pk, k, uECC_secp256r1())) return P256_ERR;
  swap_buf(pub, pk, 32);
  swap_buf(pub + 32, pk + 32, 32);
  swap_buf(priv, k, 32);
  return 0;
}

int __wrap_ble_sm_alg_gen_dhkey(const uint8_t *peer_pub_key_x, const uint8_t *peer_pub_key_y, const uint8_t *our_priv_key,
                                uint8_t *out_dhkey) {
  uint8_t pk[64], k[32], dh[32];
  swap_buf(pk, peer_pub_key_x, 32);
  swap_buf(pk + 32, peer_pub_key_y, 32);
  swap_buf(k, our_priv_key, 32);
  uECC_set_rng(p256_rng);   // micro-ecc verschleiert die Rechnung mit Zufall
  if (!uECC_valid_public_key(pk, uECC_secp256r1())) return P256_ERR;
  if (!uECC_shared_secret(pk, k, dh, uECC_secp256r1())) return P256_ERR;
  swap_buf(out_dhkey, dh, 32);
  return 0;
}
#endif
