/*
 * DOAU Save Transfer - core routines
 * Dead or Alive Ultimate (Xbox, TitleID 54430006) ups.dat
 *
 * ups.dat layout (74056 bytes):
 *   [0x00..0x13]  XCalculateSignature (non-roamable: title key + XboxHDKey)
 *   [0x14..0x17]  seed (little endian)
 *   [0x18..    ]  encrypted payload:
 *                   MT19937 init_by_array({seed, mac[0..3], mac[4..5]})
 *                   first 14 outputs -> 56-byte Blowfish key
 *                   plaintext = BlowfishECB_Decrypt( cipher XOR mt_stream )
 *   plaintext[0x0000]  padding length (0)
 *   plaintext[0xB1E9]  6-byte MAC of the console that created the profile
 *   plaintext[end-23]  "Lightning Offering Guy\0"
 */
#ifndef DOAU_CORE_H
#define DOAU_CORE_H
#include <stdint.h>
#include <stddef.h>

#define UPS_SIZE        74056u     /* 0x12148 */
#define DOASAVE_SIZE    1476u      /* 0x5C4   */
#define BR_SIZE         531800u    /* 0x81D58 */
#define UPS_MAC_OFFSET  (0x18 + 0xB1E9)

/* returns 1 on success */
int  parse_hex(const char *s, uint8_t *out, int n);   /* accepts separators ':' '-' ' ' */
void format_mac(const uint8_t mac[6], char *out);     /* "00:50:F2:12:34:56" */

/* signature */
void sig_roamable(const uint8_t *data, size_t len, uint8_t out[20]);
void sig_nonroamable(const uint8_t *data, size_t len, const uint8_t hdkey[16], uint8_t out[20]);
int  sig_check(const uint8_t *file, size_t len, const uint8_t hdkey[16]); /* 1 = matches this HD key */

/* crypto: buf = whole ups.dat, operates on buf+0x18 in place */
void ups_decrypt(uint8_t *buf, const uint8_t mac[6]);
void ups_encrypt(uint8_t *buf, const uint8_t mac[6]);
int  ups_plain_ok(const uint8_t *plainbuf);            /* checks trailing magic */
int  ups_try_mac(const uint8_t *buf, const uint8_t mac[6]); /* fast: last block only */

/* brute force: tries oui:xx:xx:xx for x in [lo,hi). returns 1 and fills mac if found.
   stop may be set asynchronously; progress receives the count done. */
int  ups_search(const uint8_t *buf, const uint8_t oui[3], uint32_t lo, uint32_t hi,
                volatile int *stop, volatile uint32_t *progress, uint8_t mac_out[6]);

/* full transfer. src = source ups.dat (unchanged), out = new ups.dat.
   returns 0 ok, -1 wrong source MAC, -2 bad size */
int  ups_transfer(const uint8_t *src, const uint8_t src_mac[6],
                  const uint8_t dst_mac[6], const uint8_t dst_hdkey[16], uint8_t *out,
                  uint8_t embedded_mac_out[6]);

/* XAPI save folder name from the save name (DOA2.xbe 0x2b9d02):
   h = (h<<16 | c) mod (2^48-59) over UTF-16 units; out = 12 uppercase hex digits.
   DOA profile names are stored in SaveMeta.xbx with a trailing U+200B; pass add_zwsp=1 for those. */
void save_folder_name(const uint16_t *name, int n, int add_zwsp, char out[13]);

#endif
