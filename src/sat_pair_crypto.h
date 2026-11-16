// Satelliten-Einrichtung: eigene Verschluesselung der WLAN-Daten (ohne Bluetooth-Pairing)
//
// Warum: Das Bluetooth-Pairing (LE Secure Connections) scheitert am P4 – dort funkt der ESP32-C6 (ESP-Hosted) und die
// Pruefung mit den Geraeteadressen schlaegt fehl ("DHKey check failed"). Deshalb verschluesselt die Software selbst:
//  1. Satellit erzeugt im Einrichtungsmodus ein P-256-Schluesselpaar, der oeffentliche Schluessel (65 Byte) ist lesbar.
//  2. Panel erzeugt ebenfalls eins, beide rechnen per ECDH denselben geheimen Wert z aus.
//  3. Aus z und beiden oeffentlichen Schluesseln: AES-256-Schluessel und HMAC-Schluessel (SHA-256).
//  4. Panel schreibt: eigener oeffentlicher Schluessel (65) | IV (16) | AES-256-CTR(Daten) | HMAC-SHA256 (erste 16 Byte).
// Schutz wie Bluetooth "Just Works": gegen Mithoeren sicher; der Knopfdruck am Satelliten begrenzt das Zeitfenster.
#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include "mbedtls/ecp.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "esp_random.h"

namespace satpair {

static inline int rng(void *, unsigned char *b, size_t n) {
  esp_fill_random(b, n);
  return 0;
}

// Neues Schluesselpaar: privat 32 Byte, oeffentlich 65 Byte (unkomprimiert, 0x04 | X | Y)
static inline bool make_key(uint8_t priv[32], uint8_t pub[65]) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d;
  mbedtls_ecp_point Q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&Q);
  size_t ol = 0;
  bool ok = !mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) && !mbedtls_ecp_gen_keypair(&grp, &d, &Q, rng, nullptr) &&
            !mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &ol, pub, 65) && ol == 65 &&
            !mbedtls_mpi_write_binary(&d, priv, 32);
  mbedtls_ecp_point_free(&Q);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

// Gemeinsamer geheimer Wert (X-Koordinate, 32 Byte) aus eigenem privaten und fremdem oeffentlichen Schluessel
static inline bool ecdh(const uint8_t priv[32], const uint8_t peer[65], uint8_t z[32]) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d, Z;
  mbedtls_ecp_point P;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&Z);
  mbedtls_ecp_point_init(&P);
  bool ok = !mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) && !mbedtls_mpi_read_binary(&d, priv, 32) &&
            !mbedtls_ecp_point_read_binary(&grp, &P, peer, 65) && !mbedtls_ecp_check_pubkey(&grp, &P) &&
            !mbedtls_ecdh_compute_shared(&grp, &Z, &P, &d, rng, nullptr) && !mbedtls_mpi_write_binary(&Z, z, 32);
  mbedtls_ecp_point_free(&P);
  mbedtls_mpi_free(&Z);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

static inline void sha256(const uint8_t *a, size_t an, const char *label, const uint8_t *b, size_t bn, const uint8_t *c,
                          size_t cn, uint8_t out[32]) {
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, md, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, a, an);
  mbedtls_md_update(&ctx, (const uint8_t *)label, strlen(label));
  mbedtls_md_update(&ctx, b, bn);
  mbedtls_md_update(&ctx, c, cn);
  mbedtls_md_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}

// Schluessel aus z und beiden oeffentlichen Schluesseln (Reihenfolge: Satellit, dann Panel)
static inline void derive(const uint8_t z[32], const uint8_t sat_pub[65], const uint8_t pan_pub[65], uint8_t enc[32],
                          uint8_t mac[32]) {
  sha256(z, 32, "SOC-SAT-PAIR-ENC", sat_pub, 65, pan_pub, 65, enc);
  sha256(z, 32, "SOC-SAT-PAIR-MAC", sat_pub, 65, pan_pub, 65, mac);
}

static inline void hmac16(const uint8_t mac[32], const uint8_t *data, size_t n, uint8_t tag[16]) {
  uint8_t full[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), mac, 32, data, n, full);
  memcpy(tag, full, 16);
}

static inline void ctr(const uint8_t enc[32], const uint8_t iv[16], const uint8_t *in, uint8_t *out, size_t n) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, enc, 256);
  uint8_t nonce[16], stream[16];
  memcpy(nonce, iv, 16);
  size_t off = 0;
  mbedtls_aes_crypt_ctr(&aes, n, &off, nonce, stream, in, out);
  mbedtls_aes_free(&aes);
}

// Verschluesseln: IV | Geheimtext | Pruefwert(16)
static inline std::string seal(const uint8_t enc[32], const uint8_t mac[32], const std::string &plain) {
  std::string out(16 + plain.size() + 16, '\0');
  uint8_t *p = (uint8_t *)&out[0];
  esp_fill_random(p, 16);
  ctr(enc, p, (const uint8_t *)plain.data(), p + 16, plain.size());
  hmac16(mac, p, 16 + plain.size(), p + 16 + plain.size());
  return out;
}

// Entschluesseln und pruefen; false = falscher Schluessel oder veraendert
static inline bool open(const uint8_t enc[32], const uint8_t mac[32], const uint8_t *in, size_t n, std::string &plain) {
  if (n < 32) return false;
  uint8_t tag[16];
  hmac16(mac, in, n - 16, tag);
  uint8_t diff = 0;
  for (int i = 0; i < 16; i++) diff |= tag[i] ^ in[n - 16 + i];
  if (diff) return false;
  plain.assign(n - 32, '\0');
  ctr(enc, in, in + 16, (uint8_t *)&plain[0], n - 32);
  return true;
}

}  // namespace satpair
