#ifdef ANT_MBEDTLS_TLS_BACKEND
#include <compat.h> // IWYU pragma: keep
#include <uv.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include <mbedtls/bignum.h>
#include <mbedtls/cipher.h>
#include <mbedtls/constant_time.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/nist_kw.h>
#include <mbedtls/pk.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#include <uuidv7.h>
#pragma GCC diagnostic pop

#include "ant.h"
#include "cfunc.h"
#include "internal.h"
#include "list.h"
#include "ascii.h"
#include "ptr.h"
#include "gc.h"
#include "utf8.h"
#include "base64.h"
#include "errors.h"
#include "gc/modules.h"
#include "gc/roots.h"
#include "gc/uv.h"
#include "silver/call.h"
#include "sync.h"
#include "gc/strings.h"
#include "modules/crypto.h"
#include "modules/buffer.h"
#include "modules/domexception.h"
#include "modules/microtasks.h"
#include "modules/process.h"

/* Minimal OpenSSL-compatibility shims over mbedTLS 3.6.3.
 * These keep the JS-facing logic shared with the OpenSSL branch while using
 * only mbedTLS primitives in this build. */
typedef const mbedtls_md_info_t EVP_MD;
typedef const mbedtls_cipher_info_t EVP_CIPHER;

#define EVP_MAX_MD_SIZE MBEDTLS_MD_MAX_SIZE
#define EVP_MAX_BLOCK_LENGTH 16
#define EVP_CTRL_GCM_SET_IVLEN 0x09
#define EVP_CTRL_GCM_GET_TAG 0x10
#define EVP_CTRL_GCM_SET_TAG 0x11
#define EVP_CIPH_GCM_MODE MBEDTLS_MODE_GCM
#define NID_sha1 MBEDTLS_MD_SHA1
#define NID_sha224 MBEDTLS_MD_SHA224
#define NID_sha256 MBEDTLS_MD_SHA256
#define NID_sha384 MBEDTLS_MD_SHA384
#define NID_sha512 MBEDTLS_MD_SHA512

#define CRYPTO_MD_NAME_MATCH(str) \
  (strlen(str) == len && strncasecmp(name, str, len) == 0)

static const EVP_MD *crypto_md_from_name_n(const char *name, size_t len) {
  if (!name) return NULL;
  if (CRYPTO_MD_NAME_MATCH("sha1") || CRYPTO_MD_NAME_MATCH("sha-1") ||
      CRYPTO_MD_NAME_MATCH("sha") || CRYPTO_MD_NAME_MATCH("dsa-sha") ||
      CRYPTO_MD_NAME_MATCH("dsa-sha1") || CRYPTO_MD_NAME_MATCH("rsa-sha1") ||
      CRYPTO_MD_NAME_MATCH("sha1withrsaencryption") ||
      CRYPTO_MD_NAME_MATCH("ecdsa-with-sha1"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (CRYPTO_MD_NAME_MATCH("sha224") || CRYPTO_MD_NAME_MATCH("sha-224") ||
      CRYPTO_MD_NAME_MATCH("rsa-sha224") || CRYPTO_MD_NAME_MATCH("sha224withrsaencryption"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_SHA224);
  if (CRYPTO_MD_NAME_MATCH("sha256") || CRYPTO_MD_NAME_MATCH("sha-256") ||
      CRYPTO_MD_NAME_MATCH("rsa-sha256") || CRYPTO_MD_NAME_MATCH("sha256withrsaencryption"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (CRYPTO_MD_NAME_MATCH("sha384") || CRYPTO_MD_NAME_MATCH("sha-384") ||
      CRYPTO_MD_NAME_MATCH("rsa-sha384") || CRYPTO_MD_NAME_MATCH("sha384withrsaencryption"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
  if (CRYPTO_MD_NAME_MATCH("sha512") || CRYPTO_MD_NAME_MATCH("sha-512") ||
      CRYPTO_MD_NAME_MATCH("rsa-sha512") || CRYPTO_MD_NAME_MATCH("sha512withrsaencryption"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
  if (CRYPTO_MD_NAME_MATCH("md5") || CRYPTO_MD_NAME_MATCH("rsa-md5") ||
      CRYPTO_MD_NAME_MATCH("md5withrsaencryption") || CRYPTO_MD_NAME_MATCH("md5-sha1"))
    return mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
  return NULL;
}
#undef CRYPTO_MD_NAME_MATCH

static const EVP_MD *EVP_get_digestbyname(const char *name) {
  return name ? crypto_md_from_name_n(name, strlen(name)) : NULL;
}
static const EVP_MD *EVP_sha1(void) { return mbedtls_md_info_from_type(MBEDTLS_MD_SHA1); }
static const EVP_MD *EVP_sha256(void) { return mbedtls_md_info_from_type(MBEDTLS_MD_SHA256); }
static const EVP_MD *EVP_sha384(void) { return mbedtls_md_info_from_type(MBEDTLS_MD_SHA384); }
static const EVP_MD *EVP_sha512(void) { return mbedtls_md_info_from_type(MBEDTLS_MD_SHA512); }
static int EVP_MD_type(const EVP_MD *md) { return md ? (int)mbedtls_md_get_type(md) : 0; }

static int EVP_MD_block_size(const EVP_MD *md) {
  if (!md) return 64;
  switch (mbedtls_md_get_type(md)) {
    case MBEDTLS_MD_SHA384:
    case MBEDTLS_MD_SHA512: return 128;
    default: return 64;
  }
}

typedef struct {
  mbedtls_md_context_t ctx;
  const EVP_MD *md;
} EVP_MD_CTX;

static EVP_MD_CTX *EVP_MD_CTX_new(void) {
  EVP_MD_CTX *c = (EVP_MD_CTX *)calloc(1, sizeof(*c));
  if (c) mbedtls_md_init(&c->ctx);
  return c;
}

static void EVP_MD_CTX_free(EVP_MD_CTX *c) {
  if (!c) return;
  mbedtls_md_free(&c->ctx);
  free(c);
}

static int EVP_DigestInit_ex(EVP_MD_CTX *c, const EVP_MD *md, void *impl) {
  (void)impl;
  if (!c || !md) return 0;
  mbedtls_md_free(&c->ctx);
  if (mbedtls_md_setup(&c->ctx, md, 0) != 0) return 0;
  if (mbedtls_md_starts(&c->ctx) != 0) return 0;
  c->md = md;
  return 1;
}

static int EVP_DigestUpdate(EVP_MD_CTX *c, const void *data, size_t len) {
  if (!c) return 0;
  return mbedtls_md_update(&c->ctx, (const unsigned char *)data, len) == 0 ? 1 : 0;
}

static int EVP_DigestFinal_ex(EVP_MD_CTX *c, unsigned char *out, unsigned int *len) {
  if (!c || !out || !len) return 0;
  if (mbedtls_md_finish(&c->ctx, out) != 0) return 0;
  *len = mbedtls_md_get_size(c->md);
  return 1;
}

static int EVP_Digest(const void *data, size_t len, unsigned char *out,
                      unsigned int *out_len, const EVP_MD *md, void *impl) {
  (void)impl;
  if (!md || !out || !out_len) return 0;
  if (mbedtls_md(md, (const unsigned char *)data, len, out) != 0) return 0;
  *out_len = mbedtls_md_get_size(md);
  return 1;
}

typedef struct {
  mbedtls_md_context_t ctx;
  const EVP_MD *md;
} HMAC_CTX;

static HMAC_CTX *HMAC_CTX_new(void) {
  HMAC_CTX *c = (HMAC_CTX *)calloc(1, sizeof(*c));
  if (c) mbedtls_md_init(&c->ctx);
  return c;
}

static void HMAC_CTX_free(HMAC_CTX *c) {
  if (!c) return;
  mbedtls_md_free(&c->ctx);
  free(c);
}

static int HMAC_Init_ex(HMAC_CTX *c, const void *key, int key_len,
                        const EVP_MD *md, void *impl) {
  (void)impl;
  if (!c || !md) return 0;
  mbedtls_md_free(&c->ctx);
  if (mbedtls_md_setup(&c->ctx, md, 1) != 0) return 0;
  if (mbedtls_md_hmac_starts(&c->ctx, (const unsigned char *)key,
                             key_len > 0 ? (size_t)key_len : 0) != 0) return 0;
  c->md = md;
  return 1;
}

static int HMAC_Update(HMAC_CTX *c, const unsigned char *data, size_t len) {
  if (!c) return 0;
  return mbedtls_md_hmac_update(&c->ctx, data, len) == 0 ? 1 : 0;
}

static int HMAC_Final(HMAC_CTX *c, unsigned char *out, unsigned int *len) {
  if (!c || !out || !len) return 0;
  if (mbedtls_md_hmac_finish(&c->ctx, out) != 0) return 0;
  *len = mbedtls_md_get_size(c->md);
  return 1;
}

static unsigned char *HMAC(const EVP_MD *md, const void *key, int key_len,
                           const unsigned char *data, size_t data_len,
                           unsigned char *out, unsigned int *out_len) {
  if (!md || !out || !out_len) return NULL;
  if (mbedtls_md_hmac(md, (const unsigned char *)key,
                      key_len > 0 ? (size_t)key_len : 0,
                      data, data_len, out) != 0) return NULL;
  *out_len = mbedtls_md_get_size(md);
  return out;
}

static const EVP_CIPHER *crypto_cipher_info_from_openssl_name(const char *name) {
  static const struct { const char *openssl; const char *mbedtls; } names[] = {
    { "aes-128-cbc", "AES-128-CBC" }, { "aes-128-ctr", "AES-128-CTR" },
    { "aes-128-ecb", "AES-128-ECB" }, { "aes-128-gcm", "AES-128-GCM" },
    { "aes-128-ofb", "AES-128-OFB" }, { "aes-192-cbc", "AES-192-CBC" },
    { "aes-192-ctr", "AES-192-CTR" }, { "aes-192-ecb", "AES-192-ECB" },
    { "aes-192-gcm", "AES-192-GCM" }, { "aes-192-ofb", "AES-192-OFB" },
    { "aes-256-cbc", "AES-256-CBC" }, { "aes-256-ctr", "AES-256-CTR" },
    { "aes-256-ecb", "AES-256-ECB" }, { "aes-256-gcm", "AES-256-GCM" },
    { "aes-256-ofb", "AES-256-OFB" }, { "des-cbc", "DES-CBC" },
    { "des-ecb", "DES-ECB" }, { "des-ede", "DES-EDE-ECB" },
    { "des-ede-cbc", "DES-EDE-CBC" }, { "des-ede3-cbc", "DES-EDE3-CBC" },
  };
  if (!name) return NULL;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    if (strcasecmp(name, names[i].openssl) == 0)
      return mbedtls_cipher_info_from_string(names[i].mbedtls);
  }
  return NULL;
}

static const EVP_CIPHER *EVP_get_cipherbyname(const char *name) {
  return crypto_cipher_info_from_openssl_name(name);
}
static const EVP_CIPHER *EVP_aes_128_gcm(void) {
  return mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_GCM);
}
static const EVP_CIPHER *EVP_aes_192_gcm(void) {
  return mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_192_GCM);
}
static const EVP_CIPHER *EVP_aes_256_gcm(void) {
  return mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_GCM);
}
static int EVP_CIPHER_mode(const EVP_CIPHER *cipher) {
  return cipher ? (int)cipher->mode : 0;
}

typedef struct {
  mbedtls_cipher_context_t ctx;
  const EVP_CIPHER *info;
  size_t iv_len;
  unsigned char tag[16];
  size_t tag_len;
  bool tag_set;
  unsigned char computed_tag[16];
  bool computed;
  bool finished;
} EVP_CIPHER_CTX;

static EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void) {
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX *)calloc(1, sizeof(*c));
  if (c) {
    mbedtls_cipher_init(&c->ctx);
    c->iv_len = 12; /* GCM default; SET_IVLEN overrides */
  }
  return c;
}

static void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *c) {
  if (!c) return;
  mbedtls_cipher_free(&c->ctx);
  mbedtls_platform_zeroize(c, sizeof(*c));
  free(c);
}

static int crypto_cipher_init_ex(EVP_CIPHER_CTX *c, const EVP_CIPHER *type,
                                 const unsigned char *key, const unsigned char *iv,
                                 mbedtls_operation_t op) {
  if (!c) return 0;
  if (type) {
    c->info = type;
    if (mbedtls_cipher_setup(&c->ctx, type) != 0) return 0;
    if (type->mode == MBEDTLS_MODE_CBC &&
        mbedtls_cipher_set_padding_mode(&c->ctx, MBEDTLS_PADDING_PKCS7) != 0) return 0;
  }
  if (!c->info) return 0;
  if (key) {
    int bits = (int)(c->info->key_bitlen << MBEDTLS_KEY_BITLEN_SHIFT);
    if (mbedtls_cipher_setkey(&c->ctx, key, bits, op) != 0) return 0;
  }
  if (iv) {
    static const unsigned char zero_iv[1] = { 0 };
    size_t len = c->iv_len;
    if (len == 0) { iv = zero_iv; len = 1; }
    if (mbedtls_cipher_set_iv(&c->ctx, iv, len) != 0) return 0;
  }
  return 1;
}

static int EVP_EncryptInit_ex(EVP_CIPHER_CTX *c, const EVP_CIPHER *type,
                              void *impl, const unsigned char *key,
                              const unsigned char *iv) {
  (void)impl;
  return crypto_cipher_init_ex(c, type, key, iv, MBEDTLS_ENCRYPT);
}

static int EVP_DecryptInit_ex(EVP_CIPHER_CTX *c, const EVP_CIPHER *type,
                              void *impl, const unsigned char *key,
                              const unsigned char *iv) {
  (void)impl;
  return crypto_cipher_init_ex(c, type, key, iv, MBEDTLS_DECRYPT);
}

static size_t crypto_cipher_block_size(EVP_CIPHER_CTX *c) {
  if (!c) return 16;
  size_t n = mbedtls_cipher_get_block_size(&c->ctx);
  return n ? n : 16;
}

static int crypto_cipher_update_ex(EVP_CIPHER_CTX *c, unsigned char *out,
                                   int *out_len, const unsigned char *in,
                                   int in_len) {
  if (!c || !out_len || (in_len > 0 && !in)) return 0;
  if (c->info && c->info->mode == MBEDTLS_MODE_GCM && out == NULL) {
    if (mbedtls_cipher_update_ad(&c->ctx, in, in_len > 0 ? (size_t)in_len : 0) != 0) return 0;
    *out_len = in_len;
    return 1;
  }
  size_t cap = (size_t)(in_len > 0 ? in_len : 0) + crypto_cipher_block_size(c);
  unsigned char *dst = out ? out : (unsigned char *)&c->computed_tag;
  size_t olen = 0;
  if (mbedtls_cipher_update(&c->ctx, in, in_len > 0 ? (size_t)in_len : 0, dst, &olen) != 0)
    return 0;
  if (olen > cap) return 0;
  *out_len = (int)olen;
  return 1;
}

static int EVP_EncryptUpdate(EVP_CIPHER_CTX *c, unsigned char *out, int *out_len,
                             const unsigned char *in, int in_len) {
  return crypto_cipher_update_ex(c, out, out_len, in, in_len);
}

static int EVP_DecryptUpdate(EVP_CIPHER_CTX *c, unsigned char *out, int *out_len,
                             const unsigned char *in, int in_len) {
  return crypto_cipher_update_ex(c, out, out_len, in, in_len);
}

static bool crypto_cipher_is_gcm(EVP_CIPHER_CTX *c) {
  return c && c->info && c->info->mode == MBEDTLS_MODE_GCM;
}

static int crypto_gcm_compute_tag(EVP_CIPHER_CTX *c, unsigned char tag[16]) {
  size_t ignored = 0;
  return mbedtls_gcm_finish((mbedtls_gcm_context *)c->ctx.cipher_ctx,
                            NULL, 0, &ignored, tag, 16) == 0;
}

static int crypto_cipher_finish_ex(EVP_CIPHER_CTX *c, unsigned char *out,
                                   int *out_len, bool encrypt) {
  if (!c || !out_len) return 0;
  size_t olen = 0;
  if (mbedtls_cipher_finish(&c->ctx, out, &olen) != 0) return 0;
  *out_len = (int)olen;
  c->finished = true;

  if (crypto_cipher_is_gcm(c)) {
    unsigned char full[16];
    if (!crypto_gcm_compute_tag(c, full)) return 0;
    memcpy(c->computed_tag, full, sizeof(full));
    c->computed = true;
    if (!encrypt && c->tag_set) {
      if (c->tag_len == 0 || c->tag_len > sizeof(full)) return 0;
      if (mbedtls_ct_memcmp(full, c->tag, c->tag_len) != 0) return 0;
    }
  }
  return 1;
}

static int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *c, unsigned char *out, int *out_len) {
  return crypto_cipher_finish_ex(c, out, out_len, true);
}

static int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *c, unsigned char *out, int *out_len) {
  return crypto_cipher_finish_ex(c, out, out_len, false);
}

static int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr) {
  if (!c) return 0;
  switch (type) {
    case EVP_CTRL_GCM_SET_IVLEN:
      if (arg < 0) return 0;
      c->iv_len = (size_t)arg;
      return 1;
    case EVP_CTRL_GCM_SET_TAG:
      if (!ptr || arg <= 0 || (size_t)arg > sizeof(c->tag)) return 0;
      memcpy(c->tag, ptr, (size_t)arg);
      c->tag_len = (size_t)arg;
      c->tag_set = true;
      return 1;
    case EVP_CTRL_GCM_GET_TAG:
      if (!ptr || arg <= 0 || (size_t)arg > sizeof(c->computed_tag) || !c->computed) return 0;
      memcpy(ptr, c->computed_tag, (size_t)arg);
      return 1;
    default: return 0;
  }
}

static int EVP_CIPHER_CTX_block_size(EVP_CIPHER_CTX *c) {
  return (int)crypto_cipher_block_size(c);
}

static int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *c, int padding) {
  if (!c) return 0;
  if (!c->info || c->info->mode != MBEDTLS_MODE_CBC) return 1;
  return mbedtls_cipher_set_padding_mode(&c->ctx,
           padding ? MBEDTLS_PADDING_PKCS7 : MBEDTLS_PADDING_NONE) == 0 ? 1 : 0;
}

static bool crypto_curve_supported(const char *name) {
  if (!name) return false;
  if (strcasecmp(name, "prime256v1") == 0) name = "secp256r1";
  const mbedtls_ecp_curve_info *info = mbedtls_ecp_curve_info_from_name(name);
  if (!info) return false;
  mbedtls_ecp_group group;
  mbedtls_ecp_group_init(&group);
  int rc = mbedtls_ecp_group_load(&group, info->grp_id);
  mbedtls_ecp_group_free(&group);
  return rc == 0;
}

static int PKCS5_PBKDF2_HMAC(const char *pass, int passlen,
                             const unsigned char *salt, int saltlen,
                             int iter, const EVP_MD *md, int keylen,
                             unsigned char *out) {
  static const unsigned char empty = 0;
  if (!md || iter < 0 || keylen < 0 || !out) return 0;
  size_t plen = passlen < 0 ? (pass ? strlen(pass) : 0) : (size_t)passlen;
  size_t slen = saltlen > 0 ? (size_t)saltlen : 0;
  const unsigned char *p = pass ? (const unsigned char *)pass : &empty;
  const unsigned char *s = salt ? salt : &empty;
  return mbedtls_pkcs5_pbkdf2_hmac_ext(
    mbedtls_md_get_type(md), p, plen, s, slen,
    (uint32_t)iter, (uint32_t)keylen, out) == 0 ? 1 : 0;
}

/* RFC 7914 scrypt (mbedTLS has no built-in scrypt primitive). */
#define CRYPTO_SCRYPT_ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static uint32_t crypto_scrypt_load32(const unsigned char *p) {
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void crypto_scrypt_store32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)(v & 0xff);
  p[1] = (unsigned char)((v >> 8) & 0xff);
  p[2] = (unsigned char)((v >> 16) & 0xff);
  p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void crypto_salsa20_8(uint8_t block[64]) {
  uint32_t x[16], z[16];
  for (int i = 0; i < 16; i++) {
    x[i] = crypto_scrypt_load32(block + (size_t)i * 4);
    z[i] = x[i];
  }
  for (int i = 0; i < 8; i += 2) {
    z[4] ^= CRYPTO_SCRYPT_ROTL32(z[0] + z[12], 7);
    z[8] ^= CRYPTO_SCRYPT_ROTL32(z[4] + z[0], 9);
    z[12] ^= CRYPTO_SCRYPT_ROTL32(z[8] + z[4], 13);
    z[0] ^= CRYPTO_SCRYPT_ROTL32(z[12] + z[8], 18);
    z[9] ^= CRYPTO_SCRYPT_ROTL32(z[5] + z[1], 7);
    z[13] ^= CRYPTO_SCRYPT_ROTL32(z[9] + z[5], 9);
    z[1] ^= CRYPTO_SCRYPT_ROTL32(z[13] + z[9], 13);
    z[5] ^= CRYPTO_SCRYPT_ROTL32(z[1] + z[13], 18);
    z[14] ^= CRYPTO_SCRYPT_ROTL32(z[10] + z[6], 7);
    z[2] ^= CRYPTO_SCRYPT_ROTL32(z[14] + z[10], 9);
    z[6] ^= CRYPTO_SCRYPT_ROTL32(z[2] + z[14], 13);
    z[10] ^= CRYPTO_SCRYPT_ROTL32(z[6] + z[2], 18);
    z[3] ^= CRYPTO_SCRYPT_ROTL32(z[15] + z[11], 7);
    z[7] ^= CRYPTO_SCRYPT_ROTL32(z[3] + z[15], 9);
    z[11] ^= CRYPTO_SCRYPT_ROTL32(z[7] + z[3], 13);
    z[15] ^= CRYPTO_SCRYPT_ROTL32(z[11] + z[7], 18);
    z[1] ^= CRYPTO_SCRYPT_ROTL32(z[0] + z[3], 7);
    z[2] ^= CRYPTO_SCRYPT_ROTL32(z[1] + z[0], 9);
    z[3] ^= CRYPTO_SCRYPT_ROTL32(z[2] + z[1], 13);
    z[0] ^= CRYPTO_SCRYPT_ROTL32(z[3] + z[2], 18);
    z[6] ^= CRYPTO_SCRYPT_ROTL32(z[5] + z[4], 7);
    z[7] ^= CRYPTO_SCRYPT_ROTL32(z[6] + z[5], 9);
    z[4] ^= CRYPTO_SCRYPT_ROTL32(z[7] + z[6], 13);
    z[5] ^= CRYPTO_SCRYPT_ROTL32(z[4] + z[7], 18);
    z[11] ^= CRYPTO_SCRYPT_ROTL32(z[10] + z[9], 7);
    z[8] ^= CRYPTO_SCRYPT_ROTL32(z[11] + z[10], 9);
    z[9] ^= CRYPTO_SCRYPT_ROTL32(z[8] + z[11], 13);
    z[10] ^= CRYPTO_SCRYPT_ROTL32(z[9] + z[8], 18);
    z[12] ^= CRYPTO_SCRYPT_ROTL32(z[15] + z[14], 7);
    z[13] ^= CRYPTO_SCRYPT_ROTL32(z[12] + z[15], 9);
    z[14] ^= CRYPTO_SCRYPT_ROTL32(z[13] + z[12], 13);
    z[15] ^= CRYPTO_SCRYPT_ROTL32(z[14] + z[13], 18);
  }
  for (int i = 0; i < 16; i++)
    crypto_scrypt_store32(block + (size_t)i * 4, x[i] + z[i]);
}
#undef CRYPTO_SCRYPT_ROTL32

static void crypto_blockmix_salsa8(uint8_t *b, uint8_t *y, uint8_t *x, size_t r) {
  memcpy(x, b + (2 * r - 1) * 64, 64);
  for (size_t i = 0; i < 2 * r; i++) {
    for (size_t j = 0; j < 64; j++) x[j] ^= b[i * 64 + j];
    crypto_salsa20_8(x);
    memcpy(y + ((i % 2) ? r + i / 2 : i / 2) * 32, x, 32);
  }
  memcpy(b, y, 128 * r);
}

static void crypto_scrypt_romix(uint8_t *b, uint8_t *v, uint8_t *y,
                                uint8_t *x, size_t n, size_t r) {
  size_t block_len = 128 * r;
  for (size_t i = 0; i < n; i++) {
    memcpy(v + i * block_len, b, block_len);
    crypto_blockmix_salsa8(b, y, x, r);
  }
  for (size_t i = 0; i < n; i++) {
    const uint8_t *last = b + (2 * r - 1) * 64;
    uint64_t j = 0;
    for (int k = 0; k < 8; k++) j |= ((uint64_t)last[k]) << (k * 8);
    j &= (uint64_t)(n - 1);
    for (size_t k = 0; k < block_len; k++) b[k] ^= v[j * block_len + k];
    crypto_blockmix_salsa8(b, y, x, r);
  }
}

static int crypto_scrypt_bytes(const char *pass, size_t passlen,
                               const uint8_t *salt, size_t saltlen,
                               uint64_t N, uint64_t r, uint64_t p,
                               uint64_t maxmem, uint8_t *out, size_t outlen) {
  static const unsigned char empty = 0;
  if (!N || !r || !p || (N & (N - 1)) != 0 || N > SIZE_MAX || r > SIZE_MAX / 128 ||
      p > SIZE_MAX / (128 * r) || N > SIZE_MAX / (128 * r)) return -1;

  size_t block_len = (size_t)(128 * r);
  size_t b_len = block_len * (size_t)p;
  size_t v_len = block_len * (size_t)N;
  if (maxmem < (uint64_t)b_len + (uint64_t)v_len + block_len + 64) return -1;

  uint8_t *b = (uint8_t *)calloc(1, b_len ? b_len : 1);
  uint8_t *v = (uint8_t *)malloc(v_len ? v_len : 1);
  uint8_t *y = (uint8_t *)malloc(block_len);
  uint8_t x[64];
  if (!b || !v || !y) { free(b); free(v); free(y); return -1; }

  const unsigned char *pw = pass ? (const unsigned char *)pass : &empty;
  const unsigned char *sl = salt ? salt : &empty;
  int rc = -1;
  if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, pw, passlen, sl, saltlen,
                                    1, (uint32_t)b_len, b) != 0) goto done;
  for (uint64_t i = 0; i < p; i++)
    crypto_scrypt_romix(b + (size_t)i * block_len, v, y, x, (size_t)N, (size_t)r);
  if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, pw, passlen, b, b_len,
                                    1, (uint32_t)outlen, out) != 0) goto done;
  rc = 0;

done:
  mbedtls_platform_zeroize(b, b_len);
  mbedtls_platform_zeroize(v, v_len);
  free(b); free(v); free(y);
  return rc;
}

static int EVP_PBE_scrypt(const char *pass, size_t passlen,
                          const uint8_t *salt, size_t saltlen,
                          uint64_t N, uint64_t r, uint64_t p, uint64_t maxmem,
                          uint8_t *out, size_t outlen) {
  return crypto_scrypt_bytes(pass, passlen, salt, saltlen,
                             N, r, p, maxmem, out, outlen) == 0 ? 1 : 0;
}

static constexpr double CRYPTO_MAX_SAFE_INTEGER = 9007199254740991.0;
static constexpr double CRYPTO_RANDOM_FILL_MAX_LENGTH = 2147483647.0;
static constexpr double CRYPTO_RANDOM_INT_MAX_RANGE = 281474976710655.0;
static constexpr uint64_t CRYPTO_RANDOM_INT_MASK = 281474976710655ULL;
static constexpr size_t CRYPTO_RANDOM_INT_CACHE_WORDS = 64;

struct crypto_random_job_s {
  crypto_random_job_t *next;
  crypto_random_job_t **pprev;
  uv_work_t work;
  ant_t *js;
  ant_value_t target;
  ant_value_t callback;
  ArrayBufferData *storage;
  uint8_t *bytes;
  size_t size;
  bool failed;
};

static _Thread_local struct {
  uint64_t words[CRYPTO_RANDOM_INT_CACHE_WORDS];
  size_t remaining;
} crypto_random_int_cache;

typedef enum {
  CRYPTO_TEXT_UTF8 = 0,
  CRYPTO_TEXT_HEX,
  CRYPTO_TEXT_BASE64,
  CRYPTO_TEXT_LATIN1
} crypto_text_encoding_t;

typedef struct {
  EVP_MD_CTX *ctx;
  const EVP_MD *md;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  bool finalized;
} ant_hash_state_t;

typedef struct {
  HMAC_CTX *ctx;
  const EVP_MD *md;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  bool finalized;
} ant_hmac_state_t;

typedef enum {
  CRYPTO_KEY_HMAC = 1,
  CRYPTO_KEY_AES_GCM,
  CRYPTO_KEY_PBKDF2,
  CRYPTO_KEY_AES_KW,
  CRYPTO_KEY_RSA_PKCS1,
  CRYPTO_KEY_RSA_PSS,
  CRYPTO_KEY_RSA_OAEP,
  CRYPTO_KEY_ECDSA,
  CRYPTO_KEY_ECDH,
} ant_crypto_key_kind_t;

typedef enum {
  CRYPTO_KEY_TYPE_SECRET = 0,
  CRYPTO_KEY_TYPE_PUBLIC,
  CRYPTO_KEY_TYPE_PRIVATE,
} ant_crypto_key_type_t;

typedef enum {
  CRYPTO_KEY_USAGE_ENCRYPT     = 1u << 0,
  CRYPTO_KEY_USAGE_DECRYPT     = 1u << 1,
  CRYPTO_KEY_USAGE_SIGN        = 1u << 2,
  CRYPTO_KEY_USAGE_VERIFY      = 1u << 3,
  CRYPTO_KEY_USAGE_DERIVE_KEY  = 1u << 4,
  CRYPTO_KEY_USAGE_DERIVE_BITS = 1u << 5,
  CRYPTO_KEY_USAGE_WRAP_KEY    = 1u << 6,
  CRYPTO_KEY_USAGE_UNWRAP_KEY  = 1u << 7,
} ant_crypto_key_usage_t;

typedef struct {
  ant_crypto_key_kind_t kind;
  const EVP_MD *md;
  uint8_t *key;
  size_t key_len;
  uint8_t usages;
  uint8_t key_type;
  bool extractable;
  void *pkey;          /* mbedTLS mbedtls_pk_context* for asymmetric keys. */
  uint32_t bits;       /* RSA modulus length or EC curve size. */
  uint32_t salt_len;   /* RSA-PSS salt length in bytes. */
  char curve[16];      /* Named curve for EC keys. */
} ant_crypto_key_t;

#ifdef ANT_MBEDTLS_TLS_BACKEND
/* Asymmetric WebCrypto helpers are defined near the end of this backend so the
 * shared secret-key logic above can stay compact. */
static ant_crypto_key_kind_t crypto_asym_kind_from_name(const char *name, size_t len);
static bool crypto_is_asym_kind(ant_crypto_key_kind_t kind);
static ant_value_t crypto_check_asym_usages(
  ant_t *js, ant_crypto_key_kind_t kind, uint8_t key_type, uint8_t usages
);
static ant_value_t crypto_make_key_pair(
  ant_t *js, ant_crypto_key_t *private_key, uint32_t private_bits,
  ant_crypto_key_t *public_key, uint32_t public_bits
);
static ant_value_t crypto_make_asym_key_object(
  ant_t *js, ant_crypto_key_t *key, uint32_t bits
);
static ant_value_t crypto_import_raw_key(
  ant_t *js, const uint8_t *bytes, size_t len, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_import_jwk_key(
  ant_t *js, ant_value_t jwk, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_import_der_key(
  ant_t *js, bool private_key, const uint8_t *bytes, size_t len,
  ant_value_t algorithm, ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_export_jwk_key(ant_t *js, ant_crypto_key_t *key);
static ant_value_t crypto_export_der_key(ant_t *js, bool private_key, ant_crypto_key_t *key);
static ant_value_t crypto_asym_sign(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm, ant_value_t data
);
static ant_value_t crypto_asym_verify(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm,
  ant_value_t signature, ant_value_t data
);
static ant_value_t crypto_asym_encrypt(
  ant_t *js, bool encrypt, ant_crypto_key_t *key,
  ant_value_t algorithm, ant_value_t data
);
static ant_value_t crypto_nist_kw(
  ant_t *js, bool wrap, ant_crypto_key_t *key,
  const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len
);
static ant_value_t crypto_ecdh_derive_bits(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm
);
static ant_value_t crypto_generate_asym_key(
  ant_t *js, ant_value_t algorithm, ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_wrap_key_impl(ant_params_t);
static ant_value_t crypto_unwrap_key_impl(ant_params_t);
static ant_value_t crypto_derive_key_impl(ant_params_t);
#endif

typedef struct {
  EVP_CIPHER_CTX *ctx;
  bool encrypt;
  bool finalized;
  bool is_gcm;
  uint8_t auth_tag[16];
  size_t auth_tag_len;
} ant_cipher_state_t;

static constexpr uint32_t CRYPTO_HASH_NATIVE_TAG   = Ant_NativeTag("HASH");
static constexpr uint32_t CRYPTO_HMAC_NATIVE_TAG   = Ant_NativeTag("HMAC");
static constexpr uint32_t CRYPTO_KEY_NATIVE_TAG    = Ant_NativeTag("CKEY");
static constexpr uint32_t CRYPTO_CIPHER_NATIVE_TAG = Ant_NativeTag("CIPH");

static const char *const k_crypto_cipher_names[] = {
  "aes-128-cbc",
  "aes-128-ctr",
  "aes-128-ecb",
  "aes-128-gcm",
  "aes-128-ofb",
  "aes-192-cbc",
  "aes-192-ctr",
  "aes-192-ecb",
  "aes-192-gcm",
  "aes-192-ofb",
  "aes-256-cbc",
  "aes-256-ctr",
  "aes-256-ecb",
  "aes-256-gcm",
  "aes-256-ofb",
  "des-cbc",
  "des-ecb",
  "des-ede",
  "des-ede-cbc",
  "des-ede3-cbc",
  "rc2-cbc",
  "rc4",
};

static const char *const k_crypto_hash_names[] = {
  "DSA-SHA",
  "DSA-SHA1",
  "MD4",
  "MD5",
  "MD5-SHA1",
  "RSA-MD5",
  "RSA-SHA1",
  "RSA-SHA224",
  "RSA-SHA256",
  "RSA-SHA384",
  "RSA-SHA512",
  "dsaWithSHA",
  "dsaWithSHA1",
  "ecdsa-with-SHA1",
  "md4",
  "md5",
  "md5-sha1",
  "md5WithRSAEncryption",
  "sha1",
  "sha1WithRSAEncryption",
  "sha224",
  "sha224WithRSAEncryption",
  "sha256",
  "sha256WithRSAEncryption",
  "sha384",
  "sha384WithRSAEncryption",
  "sha512",
  "sha512-256",
  "sha512WithRSAEncryption",
};

static const char *const k_crypto_curve_names[] = {
  "prime256v1",
  "secp224r1",
  "secp384r1",
  "secp521r1",
};

typedef enum {
  CRYPTO_NAME_CIPHER = 0,
  CRYPTO_NAME_HASH,
  CRYPTO_NAME_CURVE
} crypto_name_kind_t;

static ant_value_t crypto_webcrypto_string(ant_t *js, ant_value_t value) {
  if (vtype(value) == kTypeString) return value;
  if (is_object_type(value) || vtype(value) == kTypeBuiltin)
    value = js_to_primitive(js, value, 1);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeSymbol)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Cannot convert a Symbol to a string");
  return js_tostring_val(js, value);
}

static ant_value_t crypto_algorithm_member(ant_t *js, ant_value_t object, const char *interned_name) {
  if (!interned_name) return js_mkerr(js, "oom");
  if (vtype(object) != kTypeObject) return js_get(js, object, interned_name);

  ant_object_t *obj = js_obj_ptr(object);
  if (obj->flags.is_exotic) return js_get(js, object, interned_name);

  int32_t slot = ant_shape_lookup_interned(obj->shape, interned_name);
  if (slot >= 0) {
    const ant_shape_prop_t *prop = ant_shape_prop_at(obj->shape, (uint32_t)slot);
    if (prop->has_getter || prop->has_setter) return js_get(js, object, interned_name);
    return ant_object_prop_get_unchecked(obj, (uint32_t)slot);
  }

  if (js_get_proto(js, object) != js->sym.object_proto) return js_get(js, object, interned_name);
  ant_object_t *proto = js_obj_ptr(js->sym.object_proto);
  
  if (proto->flags.is_exotic || ant_shape_lookup_interned(proto->shape, interned_name) >= 0 ||
      vtype(js_get_proto(js, js->sym.object_proto)) != kTypeNull)
    return js_get(js, object, interned_name);

  return Ant_Value_Undefined;
}

static ant_value_t crypto_subtle_get_algorithm_name(ant_t *js, ant_value_t algorithm) {
  if (vtype(algorithm) == kTypeString) return algorithm;
  if (is_object_type(algorithm)) {
    ant_value_t name = crypto_algorithm_member(js, algorithm, js->intern.name);
    if (is_err(name)) return name;
    if (vtype(name) == kTypeUndefined)
      return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.name is required");
    return crypto_webcrypto_string(js, name);
  }
  return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm must be a string or object with a name");
}

static void crypto_hash_state_free(ant_hash_state_t *state) {
  if (!state) return;
  if (state->ctx) EVP_MD_CTX_free(state->ctx);
  free(state);
}

static void crypto_hash_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_hash_state_t *state = (ant_hash_state_t *)js_get_native(value, CRYPTO_HASH_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_HASH_NATIVE_TAG);
  crypto_hash_state_free(state);
}

static void crypto_hmac_state_free(ant_hmac_state_t *state) {
  if (!state) return;
  if (state->ctx) HMAC_CTX_free(state->ctx);
  free(state);
}

static void crypto_hmac_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_hmac_state_t *state = (ant_hmac_state_t *)js_get_native(value, CRYPTO_HMAC_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_HMAC_NATIVE_TAG);
  crypto_hmac_state_free(state);
}

static void crypto_key_free(ant_crypto_key_t *key) {
  if (!key) return;
  if (key->pkey) {
    mbedtls_pk_free((mbedtls_pk_context *)key->pkey);
    free(key->pkey);
    key->pkey = NULL;
  }
  mbedtls_platform_zeroize(key->key, key->key_len);
  free(key);
}

static void crypto_key_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_crypto_key_t *key = (ant_crypto_key_t *)js_get_native(value, CRYPTO_KEY_NATIVE_TAG);

  if (!key) return;
  js_clear_native(value, CRYPTO_KEY_NATIVE_TAG);
  crypto_key_free(key);
}

static void crypto_cipher_state_free(ant_cipher_state_t *state) {
  if (!state) return;
  if (state->ctx) EVP_CIPHER_CTX_free(state->ctx);
  free(state);
}

static void crypto_cipher_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_cipher_state_t *state = (ant_cipher_state_t *)js_get_native(value, CRYPTO_CIPHER_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_CIPHER_NATIVE_TAG);
  crypto_cipher_state_free(state);
}

int crypto_fill_random(void *buf, size_t len) {
  if (len == 0) return 0;
  if (!buf || len > (size_t)INT_MAX) return -1;
#ifdef _WIN32
  NTSTATUS st = BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return st == 0 ? 0 : -1;
#else
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) return -1;
  size_t off = 0;
  while (off < len) {
    ssize_t n = read(fd, (unsigned char *)buf + off, len - off);
    if (n <= 0) { close(fd); return -1; }
    off += (size_t)n;
  }
  close(fd);
  return 0;
#endif
}

static inline ant_value_t crypto_random_error(ant_t *js) {
  return js_mkerr(js, "secure random generation failed");
}

#define UUID_HEX_DIGIT(n) ((char)((n) < 10 ? '0' + (n) : 'a' - 10 + (n)))
#define UUID_HEX2(i) { UUID_HEX_DIGIT((i) >> 4), UUID_HEX_DIGIT((i) & 15) }
#define UUID_HEX16(h) \
  UUID_HEX2((h) * 16 + 0), UUID_HEX2((h) * 16 + 1), UUID_HEX2((h) * 16 + 2), UUID_HEX2((h) * 16 + 3),   \
  UUID_HEX2((h) * 16 + 4), UUID_HEX2((h) * 16 + 5), UUID_HEX2((h) * 16 + 6), UUID_HEX2((h) * 16 + 7),   \
  UUID_HEX2((h) * 16 + 8), UUID_HEX2((h) * 16 + 9), UUID_HEX2((h) * 16 + 10), UUID_HEX2((h) * 16 + 11), \
  UUID_HEX2((h) * 16 + 12), UUID_HEX2((h) * 16 + 13), UUID_HEX2((h) * 16 + 14), UUID_HEX2((h) * 16 + 15)

static const char uuid_hex_lut[256][2] = {
  UUID_HEX16(0), UUID_HEX16(1), UUID_HEX16(2), UUID_HEX16(3),
  UUID_HEX16(4), UUID_HEX16(5), UUID_HEX16(6), UUID_HEX16(7),
  UUID_HEX16(8), UUID_HEX16(9), UUID_HEX16(10), UUID_HEX16(11),
  UUID_HEX16(12), UUID_HEX16(13), UUID_HEX16(14), UUID_HEX16(15),
};

static inline void crypto_format_uuid_v4(const uint8_t uuid[16], char out[37]) {
  const char (*lut)[2] = uuid_hex_lut;

  memcpy(out + 0,  lut[uuid[0]], 2);
  memcpy(out + 2,  lut[uuid[1]], 2);
  memcpy(out + 4,  lut[uuid[2]], 2);
  memcpy(out + 6,  lut[uuid[3]], 2);
  
  out[8] = '-';
  memcpy(out + 9,  lut[uuid[4]], 2);
  memcpy(out + 11, lut[uuid[5]], 2);
  
  out[13] = '-';
  memcpy(out + 14, lut[uuid[6]], 2);
  memcpy(out + 16, lut[uuid[7]], 2);
  
  out[18] = '-';
  memcpy(out + 19, lut[uuid[8]], 2);
  memcpy(out + 21, lut[uuid[9]], 2);
  
  out[23] = '-';
  memcpy(out + 24, lut[uuid[10]], 2);
  memcpy(out + 26, lut[uuid[11]], 2);
  memcpy(out + 28, lut[uuid[12]], 2);
  memcpy(out + 30, lut[uuid[13]], 2);
  memcpy(out + 32, lut[uuid[14]], 2);
  memcpy(out + 34, lut[uuid[15]], 2);
  out[36] = '\0';
}

int crypto_random_uuid(char out[37]) {
  uint8_t uuid[16];
  if (!out || crypto_fill_random(uuid, sizeof(uuid)) < 0) return -1;

  uuid[6] = (uint8_t)((uuid[6] & 0x0f) | 0x40);
  uuid[8] = (uint8_t)((uuid[8] & 0x3f) | 0x80);

  crypto_format_uuid_v4(uuid, out);
  return 0;
}

static ant_value_t crypto_make_buffer(ant_t *js, const uint8_t *data, size_t len) {
  ArrayBufferData *buffer = create_array_buffer_data(js, len);
  if (!buffer) return js_mkerr(js, "Failed to allocate Buffer");
  if (len > 0 && data) memcpy(buffer->data, data, len);
  return create_typed_array(js, TYPED_ARRAY_UINT8, buffer, 0, len, js->builtins.buffer_proto);
}

static bool crypto_get_mutable_bytes(
  ant_value_t value, uint8_t **out, size_t *len, 
  size_t *element_size, ArrayBufferData **storage
) {
  *element_size = 1;

  TypedArrayData *ta = buffer_get_typedarray_data(value);
  if (ta) {
    *element_size = get_element_size(ta->type);
    *storage = ta->buffer;
    Ant_Buffer_GetViewBytes(ta, out, len);
    return true;
  }

  DataViewData *dv = buffer_get_dataview_data(value);
  if (dv) {
    *storage = dv->buffer;
    Ant_Buffer_GetViewBytes(dv, out, len);
    return true;
  }

  ArrayBufferData *ab = buffer_get_arraybuffer_data(value);
  if (!ab) return false;
  
  *storage = ab;
  bool live = Ant_Buffer_IsLive(ab);
  *out = live ? ab->data : NULL;
  *len = live ? ab->length : 0;
  
  return true;
}

static crypto_text_encoding_t crypto_parse_encoding(const char *str, size_t len) {
  if (len == 3 && strncasecmp(str, "hex", 3) == 0) return CRYPTO_TEXT_HEX;
  if (len == 6 && strncasecmp(str, "base64", 6) == 0) return CRYPTO_TEXT_BASE64;
  if (len == 9 && strncasecmp(str, "base64url", 9) == 0) return CRYPTO_TEXT_BASE64;
  
  if (
    (len == 5 && strncasecmp(str, "ascii", 5)  == 0) ||
    (len == 6 && strncasecmp(str, "latin1", 6) == 0) ||
    (len == 6 && strncasecmp(str, "binary", 6) == 0)
  ) return CRYPTO_TEXT_LATIN1;
  
  return CRYPTO_TEXT_UTF8;
}

static uint8_t *crypto_decode_hex(const char *str, size_t len, size_t *out_len) {
  if ((len & 1u) != 0) return NULL;

  uint8_t *buf = malloc(len / 2u);
  if (!buf) return NULL;

  for (size_t i = 0; i < len; i += 2u) {
    int hi = ant_ascii_hex_value((unsigned char)str[i]);
    int lo = ant_ascii_hex_value((unsigned char)str[i + 1u]);
    
    if (hi < 0 || lo < 0) {
      free(buf);
      return NULL;
    }
    
    buf[i / 2u] = (uint8_t)((hi << 4u) | lo);
  }

  *out_len = len / 2u;
  return buf;
}

static ant_value_t crypto_get_input_bytes(
  ant_t *js, ant_value_t value, ant_value_t encoding_val,
  const uint8_t **out_bytes, size_t *out_len, uint8_t **owned
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *buf = NULL;
  
  ant_value_t string_val;
  size_t str_len = 0;
  
  const char *str = NULL;
  crypto_text_encoding_t enc = CRYPTO_TEXT_UTF8;

  if (buffer_source_get_bytes(js, value, &bytes, &len)) {
    *out_bytes = bytes;
    *out_len = len;
    *owned = NULL;
    return Ant_Value_Undefined;
  }

  string_val = js_tostring_val(js, value);
  if (is_err(string_val)) return string_val;

  str = js_getstr(js, string_val, &str_len);
  if (!str) return js_mkerr(js, "Failed to convert hash input to string");

  if (vtype(encoding_val) == kTypeString) {
    size_t enc_len = 0;
    const char *enc_str = js_getstr(js, encoding_val, &enc_len);
    if (enc_str) enc = crypto_parse_encoding(enc_str, enc_len);
  }

  switch (enc) {
    case CRYPTO_TEXT_HEX:
      buf = crypto_decode_hex(str, str_len, &len);
      if (!buf) return js_mkerr(js, "Invalid hex string");
      bytes = buf;
      break;
    case CRYPTO_TEXT_BASE64:
      buf = ant_base64_decode_loose(str, str_len, &len);
      if (!buf) return Ant_Error_CreateOutOfMemory(js);
      bytes = buf;
      break;
    case CRYPTO_TEXT_LATIN1: {
      size_t units = (size_t)str_utf16_len(js, string_val);
      buf = malloc(units == 0 ? 1 : units);
      if (!buf) return Ant_Error_CreateOutOfMemory(js);
      for (size_t i = 0; i < units; i++) buf[i] = (uint8_t)utf16_code_unit_at(str, str_len, i);
      bytes = buf;
      len = units;
      break;
    }
    case CRYPTO_TEXT_UTF8:
    default:
      bytes = (const uint8_t *)str;
      len = str_len;
      break;
  }

  *out_bytes = bytes;
  *out_len = len;
  *owned = buf;
  return Ant_Value_Undefined;
}

static const EVP_MD *crypto_digest_from_name(const char *algo, size_t algo_len) {
  if (!algo || algo_len == 0) return NULL;
  char *algo_name = strndup(algo, algo_len);
  if (!algo_name) return NULL;
  const EVP_MD *md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  return md;
}

static const EVP_MD *crypto_digest_from_algorithm(ant_t *js, ant_value_t algorithm) {
  ant_value_t algo_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(algo_val)) return NULL;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return NULL;

  if (
    (algo_len == 5 && strncasecmp(algo, "SHA-1", 5) == 0) ||
    (algo_len == 4 && strncasecmp(algo, "SHA1", 4) == 0)
  ) return EVP_get_digestbyname("sha1");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-256", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA256", 6) == 0)
  ) return EVP_get_digestbyname("sha256");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-384", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA384", 6) == 0)
  ) return EVP_get_digestbyname("sha384");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-512", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA512", 6) == 0)
  ) return EVP_get_digestbyname("sha512");

  return crypto_digest_from_name(algo, algo_len);
}

static unsigned crypto_hash_record_index(const EVP_MD *md) {
  if (!md) return 0;
  switch (EVP_MD_type(md)) {
    case NID_sha1: return 4;
    case NID_sha256: return 5;
    case NID_sha384: return 6;
    case NID_sha512: return 7;
    default: return 0;
  }
}

static ant_value_t crypto_dom_exception_error(ant_t *js, const char *message, const char *name) {
  return js_throw(js, make_dom_exception(js, message, name));
}

static ant_value_t crypto_read_key_length(
  ant_t *js, ant_value_t object, bool required, uint32_t maximum, bool *present, uint32_t *result
) {
  ant_value_t value = crypto_algorithm_member(js, object, js->intern.length);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeUndefined) {
    *present = false;
    if (required) return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length is required");
    return Ant_Value_Undefined;
  }

  if (is_object_type(value) || vtype(value) == kTypeBuiltin)
    value = js_to_primitive(js, value, 2);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeBigInt || vtype(value) == kTypeSymbol)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length must be a number");

  double number = trunc(js_to_number(js, value));
  if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);

  if (!isfinite(number) || number < 0 || number > (double)maximum)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length is outside its accepted range");

  *present = true;
  *result = (uint32_t)number;

  return Ant_Value_Undefined;
}

static ant_value_t crypto_get_hmac_digest(
  ant_t *js, ant_value_t algorithm, const EVP_MD **result
) {
  if (!is_object_type(algorithm))
    return js_mkerr_typed(js, JS_ERR_TYPE, "HMAC algorithm must be an object");

  ant_value_t hash = crypto_algorithm_member(js, algorithm, intern_string(js, "hash", 4));
  if (is_err(hash)) return hash;
  
  if (vtype(hash) == kTypeUndefined)
    return js_mkerr_typed(js, JS_ERR_TYPE, "HMAC algorithm.hash is required");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, hash);
  if (is_err(name_value)) return name_value;
  size_t len = 0;
  
  const char *name = js_getstr(js, name_value, &len);
  const EVP_MD *md = NULL;
  
  if (len == 5 && strncasecmp(name, "SHA-1", len) == 0) md = EVP_sha1();
  else if (len == 7 && strncasecmp(name, "SHA-256", len) == 0) md = EVP_sha256();
  else if (len == 7 && strncasecmp(name, "SHA-384", len) == 0) md = EVP_sha384();
  else if (len == 7 && strncasecmp(name, "SHA-512", len) == 0) md = EVP_sha512();
  if (!md) return crypto_dom_exception_error(js, "Unsupported HMAC hash algorithm", "NotSupportedError");

  *result = md;
  return Ant_Value_Undefined;
}

static bool crypto_algorithm_is(ant_t *js, ant_value_t algorithm, const char *expected) {
  ant_value_t name_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_val)) return false;
  size_t len = 0;
  const char *name = js_getstr(js, name_val, &len);
  return name && strlen(expected) == len && strncasecmp(name, expected, len) == 0;
}

static const EVP_CIPHER *crypto_aes_gcm_cipher_for_key(size_t key_len) {
  switch (key_len) {
    case 16: return EVP_aes_128_gcm();
    case 24: return EVP_aes_192_gcm();
    case 32: return EVP_aes_256_gcm();
    default: return NULL;
  }
}

static const EVP_CIPHER *crypto_cipher_from_name(const char *name, size_t len) {
  char *copy = strndup(name, len);
  if (!copy) return NULL;
  const EVP_CIPHER *cipher = EVP_get_cipherbyname(copy);
  free(copy);
  return cipher;
}

static ant_hash_state_t *crypto_get_hash_state(ant_value_t value) {
  ant_hash_state_t *state = (ant_hash_state_t *)js_get_native(value, CRYPTO_HASH_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_hash_state(
  ant_t *js, ant_value_t value, ant_hash_state_t **out_state
) {
  ant_hash_state_t *state = crypto_get_hash_state(value);
  if (!state) return js_mkerr(js, "Invalid Hash state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_digest_result(
  ant_t *js, const uint8_t *digest, size_t digest_len, ant_value_t encoding_val
) {
  if (vtype(encoding_val) != kTypeString) return crypto_make_buffer(js, digest, digest_len);

  size_t enc_len = 0;
  const char *enc = js_getstr(js, encoding_val, &enc_len);
  if (!enc) return crypto_make_buffer(js, digest, digest_len);

  ant_value_t result = Ant_Value_Undefined;
  if (buffer_bytes_to_string(js, digest, digest_len, enc, enc_len, &result)) return result;

  return crypto_make_buffer(js, digest, digest_len);
}

static ant_mutex_t uuidv7_lock = ANT_MUTEX_INIT;
static uint8_t uuidv7_prev[16] = {0};
static uint8_t uuidv7_rand[256] = {0};
static size_t uuidv7_rand_consumed = sizeof(uuidv7_rand);

int uuidv7_new(uint8_t *uuid_out) {
  struct timespec tp;
  
  clock_gettime(CLOCK_REALTIME, &tp);
  uint64_t unix_ts_ms = (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
  
  ant_mutex_lock(&uuidv7_lock);
  if (uuidv7_rand_consumed > sizeof(uuidv7_rand) - 10) {
    if (crypto_fill_random(uuidv7_rand, sizeof(uuidv7_rand)) < 0) {
      ant_mutex_unlock(&uuidv7_lock);
      return -1;
    }
    
    uuidv7_rand_consumed = 0;
  }
  
  int8_t status = uuidv7_generate(uuidv7_prev, unix_ts_ms, &uuidv7_rand[uuidv7_rand_consumed], uuidv7_prev);
  uuidv7_rand_consumed += uuidv7_status_n_rand_consumed(status);
  
  memcpy(uuid_out, uuidv7_prev, 16);
  ant_mutex_unlock(&uuidv7_lock);
  
  return status;
}

// crypto.random()
static ant_value_t js_crypto_random(ant_params_t) {
  unsigned int value = 0;
  if (crypto_fill_random(&value, sizeof(value)) < 0) return crypto_random_error(js);
  return Ant_Value_FromNumber((double)value);
}

// crypto.randomUUID()
static ant_value_t js_crypto_random_uuid(ant_params_t) {
  char uuid[37];
  if (crypto_random_uuid(uuid) < 0) return crypto_random_error(js);
  return js_mkstr(js, uuid, 36);
}

// crypto.randomUUIDv7()
static ant_value_t js_crypto_random_uuidv7(ant_params_t) {
  uint8_t uuid[16];
  char uuid_str[37];
  
  int result = uuidv7_new(uuid);
  if (result < 0) return js_mkerr(js, "UUIDv7 generation failed");
  
  uuidv7_to_string(uuid, uuid_str);
  return js_mkstr(js, uuid_str, strlen(uuid_str));
}

// crypto.getRandomValues(typedArray)
static ant_value_t js_crypto_get_random_values(ant_params_t) {
  if (nargs < 1) return js_mkerr(js, "getRandomValues requires a TypedArray argument");

  TypedArrayData *ta = buffer_get_typedarray_data(args[0]);
  if (!ta) return js_mkerr_typed(js, JS_ERR_TYPE, "getRandomValues requires a TypedArray argument");

  uint8_t *ptr; size_t len;
  Ant_Buffer_GetViewBytes(ta, &ptr, &len);
  
  if (len > 65536) return js_mkerr(js, "TypedArray byte length exceeds 65536");
  if (crypto_fill_random(ptr, len) < 0) return crypto_random_error(js);
  
  return args[0];
}

static inline bool crypto_is_callback(ant_value_t value) {
  uint8_t type = vtype(value);
  return type == kTypeFunction || type == kTypeBuiltin;
}

static inline ant_value_t crypto_invalid_random_target(ant_t *js, ant_value_t value) {
  return Ant_Error_CreateInvalidArgType(js, "buf", "an instance of ArrayBuffer or ArrayBufferView", value);
}

static inline ant_value_t crypto_invalid_callback(ant_t *js, ant_value_t value) {
  return Ant_Error_CreateInvalidArgType(js, "callback", "of type function", value);
}

static ant_value_t crypto_random_parse_size(ant_t *js, ant_value_t size_val, size_t element_size, double *out_size) {
  if (vtype(size_val) != kTypeNumber) return Ant_Error_CreateInvalidArgType(js, "size", "of type number", size_val);
  double size = Ant_Value_AsNumberUnchecked(size_val) * (double)element_size;
  
  if (isnan(size) || size < 0 || size > CRYPTO_RANDOM_FILL_MAX_LENGTH) {
    char range[64];
    snprintf(range, sizeof(range), ">= 0 && <= %.0f", CRYPTO_RANDOM_FILL_MAX_LENGTH);
    return Ant_Error_CreateOutOfRange(js, "size", range, Ant_Value_FromNumber(size));
  }

  *out_size = size;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_random_fill_range(
  ant_t *js, ant_value_t offset_val, ant_value_t size_val,
  size_t length, size_t element_size, size_t *out_offset, size_t *out_size
) {
  char range[64];

  if (vtype(offset_val) != kTypeNumber)
    return Ant_Error_CreateInvalidArgType(js, "offset", "of type number", offset_val);

  double offset = Ant_Value_AsNumberUnchecked(offset_val) * (double)element_size;
  double max_offset = fmin((double)length, CRYPTO_RANDOM_FILL_MAX_LENGTH);
  
  if (isnan(offset) || offset < 0 || offset > max_offset) {
    snprintf(range, sizeof(range), ">= 0 && <= %.0f", max_offset);
    return Ant_Error_CreateOutOfRange(js, "offset", range, Ant_Value_FromNumber(offset));
  }

  *out_offset = (size_t)offset;
  *out_size = length - *out_offset;
  if (vtype(size_val) == kTypeUndefined) return Ant_Value_Undefined;

  double size = 0;
  ant_value_t parsed = crypto_random_parse_size(js, size_val, element_size, &size);
  if (is_err(parsed)) return parsed;

  double end = size + (double)*out_offset;
  if (end > (double)length) {
    snprintf(range, sizeof(range), "<= %zu", length);
    return Ant_Error_CreateOutOfRange(js, "size + offset", range, Ant_Value_FromNumber(end));
  }

  *out_size = (size_t)size;
  return Ant_Value_Undefined;
}

static void crypto_random_work(uv_work_t *work) {
  crypto_random_job_t *job = (crypto_random_job_t *)work->data;
  job->failed = crypto_fill_random(job->bytes, job->size) < 0;
}

static void crypto_random_after_work(uv_work_t *work, int status) {
  GC_UV_CALLBACK();
  crypto_random_job_t *job = (crypto_random_job_t *)work->data;
  
  ant_t *js = job->js;
  ant_value_t target = job->target;
  ant_value_t callback = job->callback;
  bool failed = job->failed || status < 0;

  Ant_List_Remove(job);
  free_array_buffer_data(js, job->storage);
  free(job);
  if (js->destroying) return;

  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, target);
  GC_ROOT_PIN(js, callback);

  ant_value_t error = failed ? js_take_thrown(js, crypto_random_error(js)) : Ant_Value_Null;
  GC_ROOT_PIN(js, error);

  ant_value_t cb_args[2] = { error, failed ? Ant_Value_Undefined : target };
  sv_vm_call(js->vm, js, callback, Ant_Value_Undefined, cb_args, 2, NULL, Ant_Value_Undefined);
  process_report_uncaught_exception_if_pending(js);
  process_microtasks(js);

  GC_ROOT_RESTORE(js, mark);
}

static ant_value_t crypto_random_fill_async(
  ant_t *js, ant_value_t target, ArrayBufferData *storage, 
  uint8_t *bytes, size_t size, ant_value_t callback
) {
  if (size == 0) {
    ant_value_t cb_args[2] = { Ant_Value_Null, target };
    ant_value_t cb_result = Ant_Error_CallCallback(js, callback, Ant_Value_Undefined, cb_args, 2);
    return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
  }

  crypto_random_job_t *job = calloc(1, sizeof(*job));
  if (!job) return Ant_Error_CreateOutOfMemory(js);

  ArrayBufferData *root = storage->owner ? storage->owner : storage;
  root->ref_count++;

  job->js = js;
  job->target = target;
  job->callback = callback;
  job->storage = root;
  job->bytes = bytes;
  job->size = size;
  job->work.data = job;
  Ant_List_Insert(&js->crypto_random_jobs, job);

  int rc = uv_queue_work(js->loop, &job->work, crypto_random_work, crypto_random_after_work);
  if (rc < 0) {
    Ant_List_Remove(job);
    free_array_buffer_data(js, root);
    free(job);
    return js_mkerr(js, "%s", uv_strerror(rc));
  }

  return Ant_Value_Undefined;
}

void gc_mark_crypto(ant_t *js, gc_mark_fn mark) {
  for (crypto_random_job_t *job = js->crypto_random_jobs; job; job = job->next) {
    mark(js, job->target);
    mark(js, job->callback);
  }
}

static ant_value_t js_crypto_random_bytes(ant_params_t) {
  ant_value_t size_val = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 1 ? args[1] : Ant_Value_Undefined;

  double requested = 0;
  ant_value_t parsed = crypto_random_parse_size(js, size_val, 1, &requested);
  if (is_err(parsed)) return parsed;

  bool is_sync = vtype(callback) == kTypeUndefined;
  if (!is_sync && !crypto_is_callback(callback)) return crypto_invalid_callback(js, callback);

  size_t size = (size_t)requested;
  ArrayBufferData *buffer = create_array_buffer_data(js, size);
  if (!buffer) return Ant_Error_CreateOutOfMemory(js);

  ant_value_t result = create_typed_array(js, TYPED_ARRAY_UINT8, buffer, 0, size, js->builtins.buffer_proto);
  if (is_err(result)) return result;

  if (!is_sync) return crypto_random_fill_async(js, result, buffer, buffer->data, size, callback);
  if (crypto_fill_random(buffer->data, size) < 0) return crypto_random_error(js);
  return result;
}

static ant_value_t crypto_warn_random_alias(ant_t *js, const char *message) {
  static atomic_bool warned;
  if (atomic_exchange(&warned, true)) return Ant_Value_Undefined;

  ant_value_t process = js_get(js, js_glob(js), "process");
  ant_value_t emit = is_object_type(process) ? js_get(js, process, "emitWarning") : Ant_Value_Undefined;
  if (!crypto_is_callback(emit)) return Ant_Value_Undefined;

  ant_value_t warning_args[3] = {
    js_mkstr(js, message, strlen(message)),
    js_mkstr(js, "DeprecationWarning", 18),
    js_mkstr(js, "DEP0115", 7),
  };

  return sv_vm_call(js->vm, js, emit, process, warning_args, 3, NULL, Ant_Value_Undefined);
}

#define CRYPTO_DEPRECATED_RANDOM_ALIAS(fn, name)                                           \
  static ant_value_t fn(ant_params_t) {                                                    \
    ant_value_t warned = crypto_warn_random_alias(js, "crypto." name " is deprecated.");   \
    if (is_err(warned)) return warned;                                                     \
    return js_crypto_random_bytes(js, args, nargs, call_new_target, call_this, call_func); \
  }

CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_prng, "prng")
CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_pseudo_random_bytes, "pseudoRandomBytes")
CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_rng, "rng")

static void crypto_define_random_aliases(ant_t *js, ant_value_t lib) {
  if (process_pending_deprecation()) {
    defmethod(js, lib, "prng", 4, Ant_Function_CreateWithArity(js_crypto_deprecated_prng, 2));
    defmethod(js, lib, "pseudoRandomBytes", 17, Ant_Function_CreateWithArity(js_crypto_deprecated_pseudo_random_bytes, 2));
    defmethod(js, lib, "rng", 3, Ant_Function_CreateWithArity(js_crypto_deprecated_rng, 2));
    return;
  }

  defalias_of(js, lib, "prng", "randomBytes");
  defalias_of(js, lib, "pseudoRandomBytes", "randomBytes");
  defalias_of(js, lib, "rng", "randomBytes");
}

static ant_value_t js_crypto_random_fill_sync(ant_params_t) {
  ant_value_t target = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t offset_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t size_val = nargs > 2 ? args[2] : Ant_Value_Undefined;

  uint8_t *bytes = NULL;
  ArrayBufferData *storage = NULL;
  
  size_t len = 0, element_size = 1, offset = 0, size = 0;
  if (!crypto_get_mutable_bytes(target, &bytes, &len, &element_size, &storage))
    return crypto_invalid_random_target(js, target);

  if (vtype(offset_val) == kTypeUndefined) offset_val = Ant_Value_FromNumber(0);
  ant_value_t range = crypto_random_fill_range(js, offset_val, size_val, len, element_size, &offset, &size);
  if (is_err(range)) return range;

  if (crypto_fill_random(bytes + offset, size) < 0) return crypto_random_error(js);
  return target;
}

static ant_value_t js_crypto_random_fill(ant_params_t) {
  ant_value_t target = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t offset_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t size_val = nargs > 2 ? args[2] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 3 ? args[3] : Ant_Value_Undefined;

  uint8_t *bytes = NULL;
  ArrayBufferData *storage = NULL;
  
  size_t len = 0, element_size = 1, offset = 0, size = 0;
  if (!crypto_get_mutable_bytes(target, &bytes, &len, &element_size, &storage))
    return crypto_invalid_random_target(js, target);

  if (crypto_is_callback(offset_val)) {
    callback = offset_val;
    offset_val = Ant_Value_FromNumber(0);
    size_val = Ant_Value_Undefined;
  } else if (crypto_is_callback(size_val)) {
    callback = size_val;
    size_val = Ant_Value_Undefined;
  } else if (!crypto_is_callback(callback))
    return crypto_invalid_callback(js, callback);

  ant_value_t range = crypto_random_fill_range(js, offset_val, size_val, len, element_size, &offset, &size);
  if (is_err(range)) return range;

  return crypto_random_fill_async(js, target, storage, bytes + offset, size, callback);
}

static inline bool crypto_is_safe_integer(ant_value_t value) {
  if (vtype(value) != kTypeNumber) return false;
  double number = Ant_Value_AsNumberUnchecked(value);
  return isfinite(number) && number == trunc(number) && fabs(number) <= CRYPTO_MAX_SAFE_INTEGER;
}

static inline bool crypto_random_int_sample(uint64_t *out) {
  if (crypto_random_int_cache.remaining == 0) {
    if (crypto_fill_random(crypto_random_int_cache.words, sizeof(crypto_random_int_cache.words)) < 0) return false;
    crypto_random_int_cache.remaining = CRYPTO_RANDOM_INT_CACHE_WORDS;
  }

  *out = crypto_random_int_cache.words[--crypto_random_int_cache.remaining] & CRYPTO_RANDOM_INT_MASK;
  return true;
}

static ant_value_t js_crypto_random_int(ant_params_t) {
  ant_value_t min_val = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t max_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 2 ? args[2] : Ant_Value_Undefined;

  bool min_omitted = vtype(max_val) == kTypeUndefined || crypto_is_callback(max_val);
  if (min_omitted) {
    callback = max_val;
    max_val = min_val;
    min_val = Ant_Value_FromNumber(0);
  }

  bool is_sync = vtype(callback) == kTypeUndefined;
  if (!is_sync && !crypto_is_callback(callback)) return crypto_invalid_callback(js, callback);

  if (!crypto_is_safe_integer(min_val)) return Ant_Error_CreateInvalidArgType(js, "min", "a safe integer", min_val);
  if (!crypto_is_safe_integer(max_val)) return Ant_Error_CreateInvalidArgType(js, "max", "a safe integer", max_val);

  double min = Ant_Value_AsNumberUnchecked(min_val);
  double max = Ant_Value_AsNumberUnchecked(max_val);
  char range_text[64];

  if (max <= min) {
    snprintf(range_text, sizeof(range_text), "greater than the value of \"min\" (%.0f)", min == 0 ? 0.0 : min);
    return Ant_Error_CreateOutOfRange(js, "max", range_text, max_val);
  }

  double range_size = max - min;
  if (range_size > CRYPTO_RANDOM_INT_MAX_RANGE) {
    snprintf(range_text, sizeof(range_text), "<= %.0f", CRYPTO_RANDOM_INT_MAX_RANGE);
    return Ant_Error_CreateOutOfRange(js, min_omitted ? "max" : "max - min", range_text, Ant_Value_FromNumber(range_size));
  }

  uint64_t range = (uint64_t)range_size;
  uint64_t limit = CRYPTO_RANDOM_INT_MASK - (CRYPTO_RANDOM_INT_MASK % range);
  uint64_t sample = 0;

  do {
    if (!crypto_random_int_sample(&sample)) return crypto_random_error(js);
  } while (sample >= limit);

  ant_value_t result = Ant_Value_FromNumber((double)(sample % range) + min);
  if (is_sync) return result;

  ant_value_t cb_args[2] = { Ant_Value_Undefined, result };
  queue_next_tick_with_args(js, callback, cb_args, 2);
  return Ant_Value_Undefined;
}

static ant_value_t crypto_make_filtered_name_array(
  ant_t *js, const char *const *names, size_t count, crypto_name_kind_t kind
) {
  ant_value_t result = Ant_Array_Create(js);
  if (is_err(result)) return result;

  GC_ROOT_SAVE(root_mark, js);
  GC_ROOT_PIN(js, result);

  for (size_t i = 0; i < count; i++) {
    const char *name = names[i];
    bool supported = false;
  
  switch (kind) {
    case CRYPTO_NAME_CIPHER:
      supported = EVP_get_cipherbyname(name) != NULL;
      break;
    case CRYPTO_NAME_HASH:
      supported = EVP_get_digestbyname(name) != NULL;
      break;
    case CRYPTO_NAME_CURVE: {
      supported = crypto_curve_supported(name);
      if (!supported) break;
      break;
    }}
    
    if (!supported) continue;
    ant_value_t name_val = js_mkstr(js, name, strlen(name));
    
    if (is_err(name_val)) {
      GC_ROOT_RESTORE(js, root_mark);
      return name_val;
    }
    
    js_arr_push(js, result, name_val);
  }

  GC_ROOT_RESTORE(js, root_mark);
  return result;
}

static ant_value_t js_crypto_get_ciphers(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_cipher_names,
  sizeof(k_crypto_cipher_names) / sizeof(k_crypto_cipher_names[0]),
  CRYPTO_NAME_CIPHER
);}

static ant_value_t js_crypto_get_hashes(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_hash_names,
  sizeof(k_crypto_hash_names) / sizeof(k_crypto_hash_names[0]),
  CRYPTO_NAME_HASH
);}

static ant_value_t js_crypto_get_curves(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_curve_names,
  sizeof(k_crypto_curve_names) / sizeof(k_crypto_curve_names[0]),
  CRYPTO_NAME_CURVE
);}

static ant_value_t js_crypto_timing_safe_equal(ant_params_t) {
  const uint8_t *left = NULL;
  const uint8_t *right = NULL;
  
  size_t left_len = 0;
  size_t right_len = 0;

  if (
    nargs < 2 ||
    !buffer_source_get_bytes(js, args[0], &left, &left_len) ||
    !buffer_source_get_bytes(js, args[1], &right, &right_len)
  ) return js_mkerr_typed(
    js, JS_ERR_TYPE,
    "timingSafeEqual arguments must be Buffer, TypedArray, DataView, or ArrayBuffer"
  );

  if (left_len != right_len)
    return js_mkerr_typed(js, JS_ERR_RANGE, "Input buffers must have the same byte length");

  return Ant_Value_FromBool(mbedtls_ct_memcmp(left, right, left_len) == 0);
}

static ant_value_t crypto_subtle_digest_impl(
  ant_t *js, ant_value_t algorithm, ant_value_t data
) {
  ant_value_t algo_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(algo_val)) return algo_val;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid digest algorithm");
  }

  char algo_name[16];
  if (algo_len >= sizeof(algo_name)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");
  }

  for (size_t i = 0; i < algo_len; i++) {
    char ch = algo[i];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    algo_name[i] = ch;
  }
  algo_name[algo_len] = '\0';

  const char *evp_name = NULL;
  if (strcmp(algo_name, "SHA-1") == 0 || strcmp(algo_name, "SHA1") == 0) evp_name = "sha1";
  else if (strcmp(algo_name, "SHA-256") == 0 || strcmp(algo_name, "SHA256") == 0) evp_name = "sha256";
  else if (strcmp(algo_name, "SHA-384") == 0 || strcmp(algo_name, "SHA384") == 0) evp_name = "sha384";
  else if (strcmp(algo_name, "SHA-512") == 0 || strcmp(algo_name, "SHA512") == 0) evp_name = "sha512";
  else return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");

  const EVP_MD *md = EVP_get_digestbyname(evp_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");

  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (!buffer_source_get_bytes(js, data, &bytes, &len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "digest data must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  if (EVP_Digest(bytes, len, digest, &digest_len, md, NULL) != 1) {
    return js_mkerr(js, "Digest failed");
  }

  return create_arraybuffer_copy(js, digest, digest_len);
}

static ant_value_t crypto_subtle_call(
  ant_native_params_t, ant_value_t call_this, ant_value_t call_func,
  ant_cfunc_t impl
) {
  GC_ROOT_SAVE(root_mark, js);
  ant_value_t promise = js_mkpromise(js);
  if (is_err(promise)) return promise;
  GC_ROOT_PIN(js, promise);
  ant_value_t result = impl(js, args, nargs, Ant_Value_Undefined, call_this, call_func);
  if (is_err(result) || Ant_Exception_Pending(js)) {
    ant_value_t reason = js_take_thrown(js, result);
    js_reject_promise(js, promise, reason);
  } else js_resolve_promise(js, promise, result);
  GC_ROOT_RESTORE(js, root_mark);
  return promise;
}

static ant_value_t crypto_subtle_digest_args(ant_params_t) {
  if (nargs < 2)
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.digest requires algorithm and data");
  return crypto_subtle_digest_impl(js, args[0], args[1]);
}

static ant_value_t js_crypto_subtle_digest(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_digest_args);
}

static const char *const crypto_key_usage_names[] = {
  "encrypt", "decrypt", "sign", "verify",
  "deriveKey", "deriveBits", "wrapKey", "unwrapKey"
};

static uint8_t crypto_key_usage_bit(ant_t *js, ant_value_t string) {
  size_t len = 0;
  const char *name = js_getstr(js, string, &len);
  
  for (unsigned i = 0; i < 8; i++) if (
    strlen(crypto_key_usage_names[i]) == len && 
    memcmp(name, crypto_key_usage_names[i], len) == 0
  ) return (uint8_t)(1u << i);
  
  return 0;
}

static bool crypto_read_key_usage(ant_t *js, ant_value_t value, void *ctx) {
  ant_value_t string = crypto_webcrypto_string(js, value);
  if (is_err(string)) return false;
  uint8_t bit = crypto_key_usage_bit(js, string);
  
  if (bit) {
    *(uint8_t *)ctx |= bit;
    return true;
  }
  
  js_mkerr_typed(js, JS_ERR_TYPE, "Invalid WebCrypto key usage");
  return false;
}

static bool crypto_read_dense_key_usages(ant_t *js, ant_value_t input, uint8_t *mask) {
  if (vtype(input) != kTypeArray) return false;
  ant_object_t *array = js_obj_ptr(input);
  if (!array->flags.fast_array || array->flags.is_exotic || array->flags.may_have_holes ||
      js_get_proto(js, input) != js->sym.array_proto) return false;

  ant_offset_t symbol = (ant_offset_t)vdata(js->sym.iterator_sym);
  if (lkp_sym(input, symbol).obj) return false;
  
  ant_prop_loc_t method = lkp_sym(js->sym.array_proto, symbol);
  if (!method.obj || js_prop_load(method) != js->sym.array_values_fn) return false;
  
  const ant_shape_prop_t *prop = ant_shape_prop_at(method.obj->shape, method.slot);
  if (!prop || prop->has_getter || prop->has_setter || method.obj->flags.is_exotic) return false;
  
  ant_prop_loc_t next = lkp(js, js->sym.array_iterator_proto, "next", 4);
  if (!next.obj || js_prop_load(next) != js->mutable_roots.crypto_array_iterator_next) return false;
  
  prop = ant_shape_prop_at(next.obj->shape, next.slot);
  if (!prop || prop->has_getter || prop->has_setter || next.obj->flags.is_exotic) return false;

  uint8_t usages = 0;
  for (uint32_t i = 0; i < array->u.array.len; i++) {
    ant_value_t value = array->u.array.data[i];
    if (vtype(value) != kTypeString) return false;
    uint8_t bit = crypto_key_usage_bit(js, value);
    if (!bit) return false;
    usages |= bit;
  }
  
  *mask = usages;
  return true;
}

static ant_value_t crypto_read_key_usages(ant_t *js, ant_value_t input, uint8_t *mask) {
  if (!is_object_type(input))
    return js_mkerr_typed(js, JS_ERR_TYPE, "keyUsages must be an iterable sequence");
  *mask = 0;
  if (crypto_read_dense_key_usages(js, input, mask)) return Ant_Value_Undefined;
  // TODO: align js_iter with WebCrypto sequence semantics (getters, cached next,
  // TypeError validation, and preserving conversion exceptions during IteratorClose).
  bool iterated = js_iter(js, input, crypto_read_key_usage, mask);
  if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
  if (!iterated)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Failed to iterate keyUsages");
  return Ant_Value_Undefined;
}

static ant_value_t crypto_check_key_usages(
  ant_t *js, ant_crypto_key_kind_t kind, uint8_t key_type, uint8_t usages
) {
  if (crypto_is_asym_kind(kind))
    return crypto_check_asym_usages(js, kind, key_type, usages);
  uint8_t allowed;
  switch (kind) {
    case CRYPTO_KEY_AES_GCM:
      allowed = CRYPTO_KEY_USAGE_ENCRYPT | CRYPTO_KEY_USAGE_DECRYPT |
        CRYPTO_KEY_USAGE_WRAP_KEY | CRYPTO_KEY_USAGE_UNWRAP_KEY;
      break;
    case CRYPTO_KEY_HMAC:
      allowed = CRYPTO_KEY_USAGE_SIGN | CRYPTO_KEY_USAGE_VERIFY;
      break;
    default:
      allowed = CRYPTO_KEY_USAGE_DERIVE_KEY | CRYPTO_KEY_USAGE_DERIVE_BITS;
      break;
  }
  return (usages & ~allowed)
    ? crypto_dom_exception_error(js, "Unsupported key usage for algorithm", "SyntaxError")
    : Ant_Value_Undefined;
}

static ant_value_t crypto_require_nonempty_secret_usages(
  ant_t *js, uint8_t usage_mask
) {
  return usage_mask == 0
    ? crypto_dom_exception_error(
        js, "Usages cannot be empty when creating a secret key", "SyntaxError"
      )
    : Ant_Value_Undefined;
}

static ant_crypto_key_kind_t crypto_kind_from_algorithm_name(
  const char *value, size_t len
) {
  if (value && len == 7 && strncasecmp(value, "AES-GCM", 7) == 0)
    return CRYPTO_KEY_AES_GCM;
  if (value && len == 4 && strncasecmp(value, "HMAC", 4) == 0)
    return CRYPTO_KEY_HMAC;
  if (value && len == 6 && strncasecmp(value, "PBKDF2", 6) == 0)
    return CRYPTO_KEY_PBKDF2;
  return crypto_asym_kind_from_name(value, len);
}

static ant_value_t crypto_get_key_algorithm_kind(
  ant_t *js, ant_value_t algorithm,
  ant_crypto_key_kind_t *kind
) {
  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_value)) return name_value;

  size_t len = 0;
  const char *value = js_getstr(js, name_value, &len);
  *kind = crypto_kind_from_algorithm_name(value, len);
  if (*kind) return Ant_Value_Undefined;
  return crypto_dom_exception_error(js, "Unsupported key algorithm", "NotSupportedError");
}

static ant_value_t crypto_read_key_format(ant_t *js, ant_value_t value, bool *raw) {
  ant_value_t string = crypto_webcrypto_string(js, value);
  if (is_err(string)) return string;
  size_t len = 0;
  const char *format = js_getstr(js, string, &len);
  *raw = len == 3 && memcmp(format, "raw", 3) == 0;
  if (*raw || (len == 3 && memcmp(format, "jwk", 3) == 0) ||
      (len == 4 && memcmp(format, "spki", 4) == 0) ||
      (len == 5 && memcmp(format, "pkcs8", 5) == 0))
    return Ant_Value_Undefined;
  return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid CryptoKey format");
}

static ant_value_t crypto_new_key_record(ant_t *js, unsigned index) {
  ant_value_t seed = js->mutable_roots.crypto_key_templates[index];
  if (vtype(seed) != kTypeObject) {
    seed = Ant_Object_CreateBare(js);
    if (is_err(seed)) return seed;
    GC_ROOT_SAVE(mark, js);
    GC_ROOT_PIN(js, seed);
    if (index == 0) {
      mkprop(js, seed, Ant_String_FromLiteral(js, "type"), Ant_String_FromLiteral(js, "secret"), ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "extractable"), Ant_Value_False, ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "algorithm"), Ant_Value_Undefined, ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "usages"), Ant_Value_Undefined, ANT_PROP_ATTR_ENUMERABLE);
      js_set_tostringtag(js, seed, Ant_String_FromLiteral(js, "CryptoKey"));
    } else {
      static const char *const hashes[] = { "SHA-1", "SHA-256", "SHA-384", "SHA-512" };
      ant_value_t name = index == CRYPTO_KEY_AES_GCM ? Ant_String_FromLiteral(js, "AES-GCM") :
        index == CRYPTO_KEY_HMAC ? Ant_String_FromLiteral(js, "HMAC") :
        index == CRYPTO_KEY_PBKDF2 ? Ant_String_FromLiteral(js, "PBKDF2") :
        js_mkstr(js, hashes[index - 4], strlen(hashes[index - 4]));
      js_mkprop_fast(js, seed, "name", 4, name);
      if (index == CRYPTO_KEY_AES_GCM || index == CRYPTO_KEY_HMAC)
        js_mkprop_fast(js, seed, "length", 6, Ant_Value_FromNumber(0));
      if (index == CRYPTO_KEY_HMAC)
        js_mkprop_fast(js, seed, "hash", 4, Ant_Value_Undefined);
    }
    GC_ROOT_RESTORE(js, mark);
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    js->mutable_roots.crypto_key_templates[index] = seed;
  }

  return js_mkobj_from_template(js, seed);
}

static inline void crypto_key_record_set(ant_t *js, ant_value_t obj, uint32_t slot, ant_value_t value) {
  ant_object_t *target = js_obj_ptr(obj);
  ant_object_prop_set_unchecked(target, slot, value);
  gc_write_barrier(js, target, value);
}

static ant_value_t crypto_make_key_algorithm(
  ant_t *js, ant_crypto_key_kind_t kind, const EVP_MD *md, uint32_t key_bits
) {
  ant_value_t algorithm = crypto_new_key_record(js, (unsigned)kind), hash = Ant_Value_Undefined;
  if (is_err(algorithm)) return algorithm;
  GC_ROOT_SAVE(root_mark, js);
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, hash);

  if (kind == CRYPTO_KEY_AES_GCM) {
    crypto_key_record_set(js, algorithm, 1, Ant_Value_FromNumber((double)key_bits));
  } else if (kind == CRYPTO_KEY_HMAC) {
    unsigned hash_index = crypto_hash_record_index(md);
    if (!hash_index) {
      ant_value_t error = crypto_dom_exception_error(js, "Unsupported HMAC hash algorithm", "NotSupportedError");
      GC_ROOT_RESTORE(js, root_mark);
      return error;
    }
    hash = crypto_new_key_record(js, hash_index);
    if (is_err(hash)) {
      GC_ROOT_RESTORE(js, root_mark);
      return hash;
    }
    crypto_key_record_set(js, algorithm, 1, Ant_Value_FromNumber((double)key_bits));
    crypto_key_record_set(js, algorithm, 2, hash);
  }

  GC_ROOT_RESTORE(js, root_mark);
  return algorithm;
}

static ant_crypto_key_t *crypto_key_alloc(size_t len) {
  size_t capacity = len ? len : 1;
  if (capacity > SIZE_MAX - sizeof(ant_crypto_key_t)) return NULL;
  ant_crypto_key_t *key = malloc(sizeof(*key) + capacity);
  if (!key) return NULL;
  *key = (ant_crypto_key_t){ .key = (uint8_t *)(key + 1), .key_len = len };
  return key;
}

static ant_value_t crypto_make_key_object(
  ant_t *js, ant_crypto_key_t *key, uint32_t bits
) {
  if (key->kind == CRYPTO_KEY_HMAC && bits % 8)
    key->key[key->key_len - 1] &= (uint8_t)(0xffu << (8 - bits % 8));

  GC_ROOT_SAVE(root_mark, js);
  ant_value_t algorithm = Ant_Value_Undefined, usages = Ant_Value_Undefined, obj = Ant_Value_Undefined;
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, usages);
  GC_ROOT_PIN(js, obj);
  
  algorithm = crypto_make_key_algorithm(js, key->kind, key->md, bits);
  if (is_err(algorithm)) { obj = algorithm; goto cleanup; }
  ant_value_t usage_values[8];
  uint32_t usage_count = 0;
  for (unsigned i = 0; i < 8; i++) {
    if (key->usages & (1u << i))
      usage_values[usage_count++] = js->mutable_roots.crypto_usage_strings[i];
  }
  usages = js_mkarr_dense_literal(js, usage_values, usage_count);
  if (is_err(usages)) { obj = usages; goto cleanup; }
  
  obj = crypto_new_key_record(js, 0);
  if (is_err(obj)) goto cleanup;
  crypto_key_record_set(js, obj, 1, Ant_Value_FromBool(key->extractable));
  crypto_key_record_set(js, obj, 2, algorithm);
  crypto_key_record_set(js, obj, 3, usages);
  
  if (Ant_Exception_Pending(js)) { obj = Ant_Exception_Current(js); goto cleanup; }
  js_set_native(obj, key, CRYPTO_KEY_NATIVE_TAG);
  js_set_finalizer(obj, crypto_key_finalize);
  key = NULL;

cleanup:
  crypto_key_free(key);
  GC_ROOT_RESTORE(js, root_mark);
  return obj;
}

static ant_crypto_key_t *crypto_require_key(ant_value_t value) {
  return (ant_crypto_key_t *)js_get_native(value, CRYPTO_KEY_NATIVE_TAG);
}

static ant_value_t crypto_authorize_key(
  ant_t *js, ant_value_t value, ant_crypto_key_kind_t kind,
  uint8_t usage, ant_crypto_key_t **key
) {
  *key = crypto_require_key(value);
  if (!*key)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Operation requires a CryptoKey");
  if ((*key)->kind != kind || !((*key)->usages & usage))
    return crypto_dom_exception_error(js, "CryptoKey does not permit this operation", "InvalidAccessError");
  return Ant_Value_Undefined;
}

static ant_value_t crypto_subtle_import_key_impl(ant_params_t) {
  if (nargs < 5)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.importKey requires format, keyData, algorithm, extractable, and keyUsages");

  bool raw = false;
  ant_value_t result = crypto_read_key_format(js, args[0], &raw);
  if (is_err(result)) return result;

  ant_value_t format = crypto_webcrypto_string(js, args[0]);
  if (is_err(format)) return format;
  size_t format_len = 0;
  const char *format_name = js_getstr(js, format, &format_len);

  const uint8_t *source = NULL;
  size_t len = 0;
  uint8_t *bytes = NULL;
  if (buffer_source_get_bytes(js, args[1], &source, &len)) {
    bytes = (uint8_t *)malloc(len ? len : 1);
    if (!bytes) return Ant_Error_CreateOutOfMemory(js);
    if (len) memcpy(bytes, source, len);
  }

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[2]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  ant_crypto_key_kind_t asym_kind = crypto_asym_kind_from_name(name, name_len);

  if (asym_kind) {
    if (format_name && format_len == 3 && memcmp(format_name, "jwk", 3) == 0) {
      free(bytes);
      return crypto_import_jwk_key(js, args[1], args[2], args[3], args[4]);
    }

    if (asym_kind == CRYPTO_KEY_AES_KW && raw) {
      if (!bytes) {
        free(bytes);
        return js_mkerr_typed(js, JS_ERR_TYPE, "keyData must be a BufferSource");
      }
      uint8_t usages = 0;
      result = crypto_read_key_usages(js, args[4], &usages);
      if (is_err(result)) { free(bytes); return result; }
      result = crypto_check_asym_usages(js, asym_kind, CRYPTO_KEY_TYPE_SECRET, usages);
      if (is_err(result)) { free(bytes); return result; }
      if (len != 16 && len != 24 && len != 32) {
        free(bytes);
        return crypto_dom_exception_error(js, "Invalid AES-KW key length", "DataError");
      }

      ant_crypto_key_t *key = crypto_key_alloc(len);
      if (!key) { free(bytes); return Ant_Error_CreateOutOfMemory(js); }
      memcpy(key->key, bytes, len);
      free(bytes);
      key->kind = asym_kind;
      key->key_type = CRYPTO_KEY_TYPE_SECRET;
      key->extractable = js_truthy(js, args[3]);
      key->usages = usages;
      return crypto_make_asym_key_object(js, key, (uint32_t)(len * 8));
    }

    if (format_name && ((format_len == 4 && memcmp(format_name, "spki", 4) == 0) ||
                        (format_len == 5 && memcmp(format_name, "pkcs8", 5) == 0))) {
      if (!bytes) {
        free(bytes);
        return js_mkerr_typed(js, JS_ERR_TYPE, "keyData must be a BufferSource");
      }
      result = crypto_import_der_key(
        js, format_len == 5, bytes, len, args[2], args[3], args[4]
      );
      free(bytes);
      return result;
    }

    free(bytes);
    return crypto_dom_exception_error(
      js, "Unsupported key format for asymmetric algorithm", "NotSupportedError"
    );
  }

  if (!raw)
    return crypto_dom_exception_error(js, "Only raw key import is supported", "NotSupportedError");

  uint8_t usages = 0;
  result = crypto_read_key_usages(js, args[4], &usages);
  if (is_err(result)) { free(bytes); return result; }

  if (!bytes) {
    free(bytes);
    return js_mkerr_typed(js, JS_ERR_TYPE, "keyData must be a BufferSource");
  }

  ant_crypto_key_t *key = crypto_key_alloc(len);
  if (!key) { free(bytes); return Ant_Error_CreateOutOfMemory(js); }
  if (len) memcpy(key->key, bytes, len);
  key->extractable = js_truthy(js, args[3]);
  key->usages = usages;
  uint32_t bits = 0;

  key->kind = crypto_kind_from_algorithm_name(name, name_len);
  if (!key->kind) {
    result = crypto_dom_exception_error(js, "Unsupported key algorithm", "NotSupportedError");
    goto cleanup;
  }
  bool length_present = false;
  if (key->kind == CRYPTO_KEY_HMAC) {
    result = crypto_get_hmac_digest(js, args[2], &key->md);
    if (is_err(result)) goto cleanup;
    result = crypto_read_key_length(
      js, args[2], false, UINT32_MAX, &length_present, &bits);
    if (is_err(result)) goto cleanup;
  }
  result = crypto_check_key_usages(js, key->kind, key->key_type, usages);
  if (is_err(result)) goto cleanup;

  if (key->kind == CRYPTO_KEY_AES_GCM) {
    if (!crypto_aes_gcm_cipher_for_key(len)) {
      result = crypto_dom_exception_error(js, "Invalid AES-GCM key length", "DataError");
      goto cleanup;
    }
    bits = (uint32_t)(len * 8);
  } else if (key->kind == CRYPTO_KEY_HMAC) {
    if (!len || len > UINT32_MAX / 8u) {
      result = crypto_dom_exception_error(js, "Invalid HMAC key length", "DataError");
      goto cleanup;
    }
    uint32_t data_bits = (uint32_t)(len * 8);
    if (!length_present) bits = data_bits;
    if (bits > data_bits || bits <= data_bits - 8) {
      result = crypto_dom_exception_error(js, "Invalid HMAC key length", "DataError");
      goto cleanup;
    }
  } else if (key->extractable) {
    result = crypto_dom_exception_error(js, "PBKDF2 keys must not be extractable", "SyntaxError");
    goto cleanup;
  }

  result = crypto_require_nonempty_secret_usages(js, usages);
  if (is_err(result)) goto cleanup;
  free(bytes);
  return crypto_make_key_object(js, key, bits);

cleanup:
  free(bytes);
  crypto_key_free(key);
  return result;
}

static ant_value_t js_crypto_subtle_import_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_import_key_impl);
}

static ant_value_t crypto_subtle_generate_key_impl(ant_params_t) {
  if (nargs < 3)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.generateKey requires algorithm, extractable, and keyUsages");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  if (crypto_asym_kind_from_name(name, name_len))
    return crypto_generate_asym_key(js, args[0], args[1], args[2]);

  uint8_t usages = 0;
  ant_value_t result = crypto_read_key_usages(js, args[2], &usages);
  if (is_err(result)) return result;
  ant_crypto_key_kind_t kind = 0;
  result = crypto_get_key_algorithm_kind(js, args[0], &kind);
  if (is_err(result)) return result;
  if (kind == CRYPTO_KEY_PBKDF2)
    return crypto_dom_exception_error(js, "Unsupported generateKey algorithm", "NotSupportedError");

  const EVP_MD *md = NULL;
  if (kind == CRYPTO_KEY_HMAC) {
    result = crypto_get_hmac_digest(js, args[0], &md);
    if (is_err(result)) return result;
  }
  bool length_present = false;
  uint32_t bits = 0;
  result = crypto_read_key_length(
    js, args[0], kind == CRYPTO_KEY_AES_GCM,
    kind == CRYPTO_KEY_AES_GCM ? UINT16_MAX : UINT32_MAX, &length_present, &bits);
  if (is_err(result)) return result;
  result = crypto_check_key_usages(js, kind, CRYPTO_KEY_TYPE_SECRET, usages);
  if (is_err(result)) return result;

  if (kind == CRYPTO_KEY_AES_GCM) {
    if (bits != 128 && bits != 192 && bits != 256)
      return crypto_dom_exception_error(js, "AES-GCM key length must be 128, 192, or 256", "OperationError");
  } else {
    if (!length_present) bits = (uint32_t)EVP_MD_block_size(md) * 8u;
    if (!bits)
      return crypto_dom_exception_error(js, "HMAC key length must be non-zero", "OperationError");
  }
  result = crypto_require_nonempty_secret_usages(js, usages);
  if (is_err(result)) return result;

  ant_crypto_key_t *key = crypto_key_alloc(bits / 8u + (bits % 8u != 0));
  if (!key) return Ant_Error_CreateOutOfMemory(js);
  key->kind = kind;
  key->md = md;
  key->usages = usages;
  key->extractable = js_truthy(js, args[1]);
  if (crypto_fill_random(key->key, key->key_len) != 0) {
    crypto_key_free(key);
    return crypto_dom_exception_error(js, "Key generation failed", "OperationError");
  }
  return crypto_make_key_object(js, key, bits);
}

static ant_value_t js_crypto_subtle_generate_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_generate_key_impl);
}

static ant_value_t crypto_subtle_export_key_impl(ant_params_t) {
  if (nargs < 2) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.exportKey requires format and key");
  }

  bool raw = false;
  ant_value_t result = crypto_read_key_format(js, args[0], &raw);
  if (is_err(result)) return result;
  ant_value_t format = crypto_webcrypto_string(js, args[0]);
  if (is_err(format)) return format;
  size_t format_len = 0;
  const char *format_name = js_getstr(js, format, &format_len);

  ant_crypto_key_t *key = crypto_require_key(args[1]);
  if (!key) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.exportKey requires a CryptoKey");
  }
  if (!key->extractable) {
    return crypto_dom_exception_error(js, "CryptoKey is not extractable", "InvalidAccessError");
  }

  if (crypto_is_asym_kind(key->kind)) {
    if (raw && key->kind == CRYPTO_KEY_AES_KW)
      return create_arraybuffer_copy(js, key->key, key->key_len);
    if (format_name && format_len == 3 && memcmp(format_name, "jwk", 3) == 0)
      return crypto_export_jwk_key(js, key);
    if (format_name && format_len == 4 && memcmp(format_name, "spki", 4) == 0) {
      if (!key->pkey)
        return crypto_dom_exception_error(js, "Key format is not supported", "NotSupportedError");
      return crypto_export_der_key(js, false, key);
    }
    if (format_name && format_len == 5 && memcmp(format_name, "pkcs8", 5) == 0) {
      if (!key->pkey || key->key_type != CRYPTO_KEY_TYPE_PRIVATE)
        return crypto_dom_exception_error(js, "pkcs8 export requires a private key", "InvalidAccessError");
      return crypto_export_der_key(js, true, key);
    }
    return crypto_dom_exception_error(js, "Unsupported export format for asymmetric key", "NotSupportedError");
  }

  if (key->kind != CRYPTO_KEY_AES_GCM && key->kind != CRYPTO_KEY_HMAC) {
    return crypto_dom_exception_error(js, "CryptoKey algorithm cannot be exported", "NotSupportedError");
  }
  if (!raw)
    return crypto_dom_exception_error(js, "Only raw key export is supported", "NotSupportedError");
  return create_arraybuffer_copy(js, key->key, key->key_len);
}

static ant_value_t js_crypto_subtle_export_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_export_key_impl);
}

static ant_value_t crypto_subtle_derive_bits_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.deriveBits requires algorithm, baseKey, and length");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  if (crypto_asym_kind_from_name(name, name_len) == CRYPTO_KEY_ECDH) {
    ant_crypto_key_t *key = crypto_require_key(args[1]);
    if (!key || key->kind != CRYPTO_KEY_ECDH)
      return js_mkerr_typed(js, JS_ERR_TYPE, "ECDH deriveBits requires an ECDH CryptoKey");
    if (!(key->usages & CRYPTO_KEY_USAGE_DERIVE_BITS))
      return crypto_dom_exception_error(js, "CryptoKey does not permit deriveBits", "InvalidAccessError");

    double length_num = Ant_Value_AsNumberUnchecked(args[2]);
    if (!isfinite(length_num) || length_num < 0 || length_num > (double)INT_MAX * 8.0 ||
        fmod(length_num, 8) != 0)
      return js_mkerr_typed(js, JS_ERR_TYPE, "deriveBits length must be a non-negative multiple of 8");
    size_t requested = (size_t)(length_num / 8);

    ant_value_t derived = crypto_ecdh_derive_bits(js, key, args[0]);
    if (is_err(derived)) return derived;
    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!buffer_source_get_bytes(js, derived, &bytes, &len))
      return js_mkerr(js, "ECDH deriveBits failed");
    if (requested > len)
      return js_mkerr_typed(js, JS_ERR_RANGE, "deriveBits length exceeds ECDH output");
    return create_arraybuffer_copy(js, bytes, requested);
  }

  if (!crypto_algorithm_is(js, args[0], "PBKDF2")) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported deriveBits algorithm");
  }

  ant_crypto_key_t *key = crypto_require_key(args[1]);
  if (!key || key->kind != CRYPTO_KEY_PBKDF2) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 deriveBits requires a PBKDF2 CryptoKey");
  }
  if ((key->usages & CRYPTO_KEY_USAGE_DERIVE_BITS) == 0) {
    return crypto_dom_exception_error(
      js, "CryptoKey does not permit deriveBits", "InvalidAccessError"
    );
  }

  double length_num = Ant_Value_AsNumberUnchecked(args[2]);
  if (!isfinite(length_num) || length_num < 0 || length_num > (double)INT_MAX * 8.0 || fmod(length_num, 8) != 0) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "deriveBits length must be a non-negative multiple of 8");
  }
  int keylen = (int)(length_num / 8);

  ant_value_t salt_val = js_get(js, args[0], "salt");
  const uint8_t *salt = NULL;
  size_t salt_len = 0;
  
  if (!buffer_source_get_bytes(js, salt_val, &salt, &salt_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 salt must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }
  
  if (salt_len > (size_t)INT_MAX || key->key_len > (size_t)INT_MAX) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 input is too large");
  }

  ant_value_t iterations_val = js_get(js, args[0], "iterations");
  double iterations_num = Ant_Value_AsNumberUnchecked(iterations_val);
  if (!isfinite(iterations_num) || iterations_num <= 0 || iterations_num > INT_MAX || floor(iterations_num) != iterations_num) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 iterations must be a positive integer");
  }
  int iterations = (int)iterations_num;

  ant_value_t hash_val = js_get(js, args[0], "hash");
  const EVP_MD *md = crypto_digest_from_algorithm(js, hash_val);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported PBKDF2 hash algorithm");

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) return Ant_Error_CreateOutOfMemory(js);
  if (PKCS5_PBKDF2_HMAC((const char *)key->key, (int)key->key_len, salt, (int)salt_len, iterations, md, keylen, out) != 1) {
    free(out);
    return js_mkerr(js, "PBKDF2 failed");
  }

  ant_value_t result = create_arraybuffer_copy(js, out, (size_t)keylen);
  free(out);
  return result;
}

static ant_value_t js_crypto_subtle_derive_bits(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_derive_bits_impl);
}

static ant_value_t crypto_hmac_once(
  ant_t *js, ant_crypto_key_t *key, ant_value_t data, ant_value_t encoding,
  bool arraybuffer_result
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, data, encoding, &bytes, &len, &owned);
  if (is_err(err)) return err;

  unsigned char out[EVP_MAX_MD_SIZE];
  unsigned int out_len = 0;
  unsigned char *res = HMAC(key->md, key->key, (int)key->key_len, bytes, len, out, &out_len);
  if (owned) free(owned);
  if (!res) return js_mkerr(js, "HMAC failed");
  return arraybuffer_result ? create_arraybuffer_copy(js, out, out_len) : crypto_make_buffer(js, out, out_len);
}

static ant_value_t crypto_subtle_sign_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.sign requires algorithm, key, and data");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (crypto_is_asym_kind(kind)) {
    if (kind != CRYPTO_KEY_RSA_PKCS1 && kind != CRYPTO_KEY_RSA_PSS && kind != CRYPTO_KEY_ECDSA)
      return crypto_dom_exception_error(js, "Unsupported signing algorithm", "NotSupportedError");
    ant_crypto_key_t *key = NULL;
    ant_value_t error = crypto_authorize_key(js, args[1], kind, CRYPTO_KEY_USAGE_SIGN, &key);
    if (is_err(error)) return error;
    return crypto_asym_sign(js, key, args[0], args[2]);
  }

  if (!crypto_algorithm_is(js, args[0], "HMAC")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported signing algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_HMAC, CRYPTO_KEY_USAGE_SIGN, &key);
  if (is_err(error)) return error;
  return crypto_hmac_once(js, key, args[2], Ant_Value_Undefined, true);
}

static ant_value_t js_crypto_subtle_sign(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_sign_impl);
}

static ant_value_t crypto_subtle_verify_impl(ant_params_t) {
  if (nargs < 4) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.verify requires algorithm, key, signature, and data");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (crypto_is_asym_kind(kind)) {
    if (kind != CRYPTO_KEY_RSA_PKCS1 && kind != CRYPTO_KEY_RSA_PSS && kind != CRYPTO_KEY_ECDSA)
      return crypto_dom_exception_error(js, "Unsupported verification algorithm", "NotSupportedError");
    ant_crypto_key_t *key = NULL;
    ant_value_t error = crypto_authorize_key(js, args[1], kind, CRYPTO_KEY_USAGE_VERIFY, &key);
    if (is_err(error)) return error;
    return crypto_asym_verify(js, key, args[0], args[2], args[3]);
  }

  if (!crypto_algorithm_is(js, args[0], "HMAC")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported verification algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_HMAC, CRYPTO_KEY_USAGE_VERIFY, &key);
  if (is_err(error)) return error;

  const uint8_t *sig = NULL;
  size_t sig_len = 0;
  if (!buffer_source_get_bytes(js, args[2], &sig, &sig_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "signature must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  ant_value_t expected = crypto_hmac_once(js, key, args[3], Ant_Value_Undefined, false);
  if (is_err(expected)) return expected;

  const uint8_t *expected_bytes = NULL;
  size_t expected_len = 0;
  if (!buffer_source_get_bytes(js, expected, &expected_bytes, &expected_len)) return Ant_Value_False;
  if (sig_len != expected_len) return Ant_Value_False;
  return Ant_Value_FromBool(mbedtls_ct_memcmp(sig, expected_bytes, sig_len) == 0);
}

static ant_value_t js_crypto_subtle_verify(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_verify_impl);
}

static ant_value_t crypto_aes_gcm_crypt(
  ant_t *js, bool encrypt, ant_value_t algorithm, ant_crypto_key_t *key, ant_value_t data
) {
  if (!key || key->kind != CRYPTO_KEY_AES_GCM) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM requires an AES CryptoKey");
  }

  ant_value_t iv_val = js_get(js, algorithm, "iv");
  const uint8_t *iv = NULL;
  size_t iv_len = 0;
  if (!buffer_source_get_bytes(js, iv_val, &iv, &iv_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM iv must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  const uint8_t *input = NULL;
  size_t input_len = 0;
  if (!buffer_source_get_bytes(js, data, &input, &input_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM data must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  int tag_len = 16;
  ant_value_t tag_len_val = js_get(js, algorithm, "tagLength");
  if (vtype(tag_len_val) == kTypeNumber) {
    tag_len = (int)(Ant_Value_AsNumberUnchecked(tag_len_val) / 8.0);
    if (tag_len <= 0 || tag_len > 16) return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid AES-GCM tagLength");
  }

  const uint8_t *aad = NULL;
  size_t aad_len = 0;
  ant_value_t aad_val = js_get(js, algorithm, "additionalData");
  bool has_aad = buffer_source_get_bytes(js, aad_val, &aad, &aad_len);

  if (!encrypt && input_len < (size_t)tag_len) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM ciphertext is shorter than tagLength");
  }

  const EVP_CIPHER *cipher = crypto_aes_gcm_cipher_for_key(key->key_len);
  if (!cipher) return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid AES-GCM key length");

  size_t text_len = encrypt ? input_len : input_len - (size_t)tag_len;
  size_t out_cap = encrypt ? input_len + (size_t)tag_len : text_len;
  uint8_t *out = malloc(out_cap ? out_cap : 1);
  if (!out) return Ant_Error_CreateOutOfMemory(js);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    free(out);
    return js_mkerr(js, "AES-GCM initialization failed");
  }

  int ok = encrypt
    ? EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL)
    : EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL);
  ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
  ok = ok && (encrypt
    ? EVP_EncryptInit_ex(ctx, NULL, NULL, key->key, iv)
    : EVP_DecryptInit_ex(ctx, NULL, NULL, key->key, iv));

  int out_len = 0;
  int total = 0;
  if (ok && has_aad && aad_len > 0) {
    ok = encrypt
      ? EVP_EncryptUpdate(ctx, NULL, &out_len, aad, (int)aad_len)
      : EVP_DecryptUpdate(ctx, NULL, &out_len, aad, (int)aad_len);
  }

  if (ok && text_len > 0) {
    ok = encrypt
      ? EVP_EncryptUpdate(ctx, out, &out_len, input, (int)text_len)
      : EVP_DecryptUpdate(ctx, out, &out_len, input, (int)text_len);
    total += out_len;
  }

  if (!encrypt && ok) {
    ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag_len, (void *)(input + text_len));
  }

  if (ok) {
    ok = encrypt
      ? EVP_EncryptFinal_ex(ctx, out + total, &out_len)
      : EVP_DecryptFinal_ex(ctx, out + total, &out_len);
    total += out_len;
  }

  if (ok && encrypt) {
    ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, out + total);
    total += tag_len;
  }

  EVP_CIPHER_CTX_free(ctx);
  if (!ok) {
    free(out);
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM operation failed");
  }

  ant_value_t result = create_arraybuffer_copy(js, out, (size_t)total);
  free(out);
  return result;
}

static ant_value_t crypto_subtle_encrypt_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.encrypt requires algorithm, key, and data");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (crypto_is_asym_kind(kind)) {
    if (kind != CRYPTO_KEY_RSA_OAEP)
      return crypto_dom_exception_error(js, "Unsupported encryption algorithm", "NotSupportedError");
    ant_crypto_key_t *key = NULL;
    ant_value_t error = crypto_authorize_key(js, args[1], kind, CRYPTO_KEY_USAGE_ENCRYPT, &key);
    if (is_err(error)) return error;
    return crypto_asym_encrypt(js, true, key, args[0], args[2]);
  }

  if (!crypto_algorithm_is(js, args[0], "AES-GCM")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported encryption algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_AES_GCM, CRYPTO_KEY_USAGE_ENCRYPT, &key);
  if (is_err(error)) return error;
  return crypto_aes_gcm_crypt(js, true, args[0], key, args[2]);
}

static ant_value_t crypto_subtle_decrypt_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.decrypt requires algorithm, key, and data");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, args[0]);
  if (is_err(name_value)) return name_value;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_value, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (crypto_is_asym_kind(kind)) {
    if (kind != CRYPTO_KEY_RSA_OAEP)
      return crypto_dom_exception_error(js, "Unsupported decryption algorithm", "NotSupportedError");
    ant_crypto_key_t *key = NULL;
    ant_value_t error = crypto_authorize_key(js, args[1], kind, CRYPTO_KEY_USAGE_DECRYPT, &key);
    if (is_err(error)) return error;
    return crypto_asym_encrypt(js, false, key, args[0], args[2]);
  }

  if (!crypto_algorithm_is(js, args[0], "AES-GCM")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported decryption algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_AES_GCM, CRYPTO_KEY_USAGE_DECRYPT, &key);
  if (is_err(error)) return error;
  return crypto_aes_gcm_crypt(js, false, args[0], key, args[2]);
}

static ant_value_t js_crypto_subtle_encrypt(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_encrypt_impl);
}

static ant_value_t js_crypto_subtle_decrypt(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_decrypt_impl);
}

static ant_value_t js_hash_update(ant_params_t) {
  ant_hash_state_t *state = NULL;
  
  const uint8_t *bytes = NULL;
  size_t len = 0;
  
  uint8_t *owned = NULL;
  ant_value_t err;

  err = crypto_require_hash_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hash digest already called");
  if (nargs < 1) return js_mkerr(js, "Hash.update requires data");

  ant_value_t encoding = (nargs >= 2) ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], encoding, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (EVP_DigestUpdate(state->ctx, bytes, len) != 1) {
    err = js_mkerr(js, "Hash update failed");
    goto cleanup;
  }

  err = call_this;

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_value_t js_hash_digest(ant_params_t) {
  ant_hash_state_t *state = NULL;
  ant_value_t err = crypto_require_hash_state(js, call_this, &state);
  
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hash digest already called");

  if (EVP_DigestFinal_ex(state->ctx, state->digest, &state->digest_len) != 1) {
    return js_mkerr(js, "Hash digest failed");
  }

  state->finalized = true;
  EVP_MD_CTX_free(state->ctx);
  state->ctx = NULL;

  return crypto_digest_result(js, state->digest, state->digest_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_crypto_create_hash(ant_params_t) {
  if (nargs < 1) return js_mkerr(js, "createHash requires an algorithm");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return js_mkerr(js, "Invalid hash algorithm");

  char *algo_name = strndup(algo, algo_len);
  if (!algo_name) return Ant_Error_CreateOutOfMemory(js);

  const EVP_MD *md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported hash algorithm");

  ant_hash_state_t *state = calloc(1, sizeof(*state));
  if (!state) return Ant_Error_CreateOutOfMemory(js);

  state->ctx = EVP_MD_CTX_new();
  state->md = md;
  
  if (!state->ctx || EVP_DigestInit_ex(state->ctx, md, NULL) != 1) {
    crypto_hash_state_free(state);
    return js_mkerr(js, "Failed to initialize hash");
  }

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_hash_update));
  js_set(js, obj, "digest", Ant_Function_Create(js_hash_digest));
  
  js_set_native(obj, state, CRYPTO_HASH_NATIVE_TAG);
  js_set_finalizer(obj, crypto_hash_finalize);
  js_set_tostringtag(js, obj, js_mkstr(js, "Hash", 4));
  
  return obj;
}

static ant_hmac_state_t *crypto_get_hmac_state(ant_value_t value) {
  ant_hmac_state_t *state = (ant_hmac_state_t *)js_get_native(value, CRYPTO_HMAC_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_hmac_state(
  ant_t *js, ant_value_t value, ant_hmac_state_t **out_state
) {
  ant_hmac_state_t *state = crypto_get_hmac_state(value);
  if (!state) return js_mkerr(js, "Invalid Hmac state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t js_hmac_update(ant_params_t) {
  ant_hmac_state_t *state = NULL;
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *owned = NULL;

  ant_value_t err = crypto_require_hmac_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hmac digest already called");
  if (nargs < 1) return js_mkerr(js, "Hmac.update requires data");

  ant_value_t encoding = (nargs >= 2) ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], encoding, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (HMAC_Update(state->ctx, bytes, len) != 1) {
    err = js_mkerr(js, "Hmac update failed");
    goto cleanup;
  }

  err = call_this;

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_value_t js_hmac_digest(ant_params_t) {
  ant_hmac_state_t *state = NULL;
  ant_value_t err = crypto_require_hmac_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hmac digest already called");

  if (HMAC_Final(state->ctx, state->digest, &state->digest_len) != 1) {
    return js_mkerr(js, "Hmac digest failed");
  }

  state->finalized = true;
  HMAC_CTX_free(state->ctx);
  state->ctx = NULL;

  return crypto_digest_result(js, state->digest, state->digest_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_crypto_create_hmac(ant_params_t) {
  if (nargs < 2) return js_mkerr(js, "createHmac requires an algorithm and key");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;
  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  const EVP_MD *md = crypto_digest_from_name(algo, algo_len);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported HMAC algorithm");

  const uint8_t *key = NULL;
  size_t key_len = 0;
  uint8_t *owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &key, &key_len, &owned);
  if (is_err(err)) return err;

  ant_hmac_state_t *state = calloc(1, sizeof(*state));
  if (!state) {
    if (owned) free(owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  state->ctx = HMAC_CTX_new();
  state->md = md;
  if (!state->ctx || HMAC_Init_ex(state->ctx, key, (int)key_len, md, NULL) != 1) {
    if (owned) free(owned);
    crypto_hmac_state_free(state);
    return js_mkerr(js, "Failed to initialize Hmac");
  }
  if (owned) free(owned);

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_hmac_update));
  js_set(js, obj, "digest", Ant_Function_Create(js_hmac_digest));
  js_set_native(obj, state, CRYPTO_HMAC_NATIVE_TAG);
  js_set_finalizer(obj, crypto_hmac_finalize);
  js_set_tostringtag(js, obj, js_mkstr(js, "Hmac", 4));
  return obj;
}

static ant_value_t js_crypto_hash(ant_params_t) {
  ant_value_t algo_val;
  ant_value_t output_encoding;
  
  const char *algo;
  size_t algo_len = 0;
  char *algo_name = NULL;
  
  const EVP_MD *md = NULL;
  const uint8_t *bytes = NULL;
  
  size_t len = 0;
  uint8_t *owned = NULL;
  
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  ant_value_t err;

  if (nargs < 2) return js_mkerr(js, "hash requires algorithm and data");
  output_encoding = nargs >= 3 ? args[2] : Ant_Value_Undefined;

  algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;

  algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return js_mkerr(js, "Invalid hash algorithm");

  algo_name = strndup(algo, algo_len);
  if (!algo_name) return Ant_Error_CreateOutOfMemory(js);

  md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported hash algorithm");

  err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (EVP_Digest(bytes, len, digest, &digest_len, md, NULL) != 1) {
    err = js_mkerr(js, "Hash failed");
    goto cleanup;
  }

  err = crypto_digest_result(js, digest, digest_len, output_encoding);

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_cipher_state_t *crypto_get_cipher_state(ant_value_t value) {
  ant_cipher_state_t *state = (ant_cipher_state_t *)js_get_native(value, CRYPTO_CIPHER_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_cipher_state(
  ant_t *js, ant_value_t value, ant_cipher_state_t **out_state
) {
  ant_cipher_state_t *state = crypto_get_cipher_state(value);
  if (!state) return js_mkerr(js, "Invalid Cipher state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_cipher_bytes_result(
  ant_t *js, uint8_t *bytes, size_t len, ant_value_t encoding
) {
  if (vtype(encoding) == kTypeString) return crypto_digest_result(js, bytes, len, encoding);
  return crypto_make_buffer(js, bytes, len);
}

static ant_value_t js_cipher_update(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Cipher already finalized");
  if (nargs < 1) return js_mkerr(js, "Cipher.update requires data");

  const uint8_t *input = NULL;
  size_t input_len = 0;
  uint8_t *owned = NULL;
  ant_value_t input_encoding = nargs >= 2 ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], input_encoding, &input, &input_len, &owned);
  if (is_err(err)) return err;

  int block_size = EVP_CIPHER_CTX_block_size(state->ctx);
  size_t out_cap = input_len + (block_size > 0 ? (size_t)block_size : 16u);
  uint8_t *out = malloc(out_cap ? out_cap : 1);
  if (!out) {
    if (owned) free(owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  int out_len = 0;
  int ok = state->encrypt
    ? EVP_EncryptUpdate(state->ctx, out, &out_len, input, (int)input_len)
    : EVP_DecryptUpdate(state->ctx, out, &out_len, input, (int)input_len);

  if (owned) free(owned);
  if (!ok) {
    free(out);
    return js_mkerr(js, "Cipher update failed");
  }

  ant_value_t result = crypto_cipher_bytes_result(js, out, (size_t)out_len, nargs >= 3 ? args[2] : Ant_Value_Undefined);
  free(out);
  return result;
}

static ant_value_t js_cipher_final(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Cipher already finalized");

  int block_size = EVP_CIPHER_CTX_block_size(state->ctx);
  size_t out_cap = block_size > 0 ? (size_t)block_size : 16u;
  uint8_t out[EVP_MAX_BLOCK_LENGTH + 16];
  int out_len = 0;

  int ok = state->encrypt
    ? EVP_EncryptFinal_ex(state->ctx, out, &out_len)
    : EVP_DecryptFinal_ex(state->ctx, out, &out_len);
  (void)out_cap;

  state->finalized = true;
  if (!ok) return js_mkerr(js, "Cipher final failed");

  if (state->encrypt && state->is_gcm) {
    state->auth_tag_len = 16;
    EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_GET_TAG, 16, state->auth_tag);
  }

  return crypto_cipher_bytes_result(js, out, (size_t)out_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_cipher_set_aad(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (nargs < 1) return js_mkerr(js, "setAAD requires data");

  const uint8_t *aad = NULL;
  size_t aad_len = 0;
  if (!buffer_source_get_bytes(js, args[0], &aad, &aad_len)) {
    return js_mkerr(js, "setAAD data must be a Buffer, TypedArray, DataView, or ArrayBuffer");
  }

  int out_len = 0;
  int ok = state->encrypt
    ? EVP_EncryptUpdate(state->ctx, NULL, &out_len, aad, (int)aad_len)
    : EVP_DecryptUpdate(state->ctx, NULL, &out_len, aad, (int)aad_len);
  if (!ok) return js_mkerr(js, "setAAD failed");
  return call_this;
}

static ant_value_t js_cipher_get_auth_tag(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (!state->is_gcm || !state->encrypt || state->auth_tag_len == 0) {
    return js_mkerr(js, "Auth tag is not available");
  }
  return crypto_make_buffer(js, state->auth_tag, state->auth_tag_len);
}

static ant_value_t js_cipher_set_auth_tag(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (nargs < 1) return js_mkerr(js, "setAuthTag requires a tag");

  const uint8_t *tag = NULL;
  size_t tag_len = 0;
  if (!buffer_source_get_bytes(js, args[0], &tag, &tag_len) || tag_len > sizeof(state->auth_tag)) {
    return js_mkerr(js, "Invalid authentication tag");
  }

  memcpy(state->auth_tag, tag, tag_len);
  state->auth_tag_len = tag_len;
  if (!state->encrypt && state->is_gcm) {
    EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_SET_TAG, (int)tag_len, state->auth_tag);
  }
  return call_this;
}

static ant_value_t js_cipher_set_auto_padding(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  int padding = nargs < 1 || js_truthy(js, args[0]);
  EVP_CIPHER_CTX_set_padding(state->ctx, padding);
  return call_this;
}

static ant_value_t crypto_create_cipheriv(ant_native_params_t, bool encrypt) {
  if (nargs < 3) return js_mkerr(js, "createCipheriv requires algorithm, key, and iv");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;
  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  const EVP_CIPHER *cipher = crypto_cipher_from_name(algo, algo_len);
  if (!cipher) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported cipher algorithm");

  const uint8_t *key = NULL;
  size_t key_len = 0;
  uint8_t *key_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &key, &key_len, &key_owned);
  if (is_err(err)) return err;

  const uint8_t *iv = NULL;
  size_t iv_len = 0;
  uint8_t *iv_owned = NULL;
  if (vtype(args[2]) != kTypeNull && vtype(args[2]) != kTypeUndefined) {
    err = crypto_get_input_bytes(js, args[2], Ant_Value_Undefined, &iv, &iv_len, &iv_owned);
    if (is_err(err)) {
      if (key_owned) free(key_owned);
      return err;
    }
  }

  ant_cipher_state_t *state = calloc(1, sizeof(*state));
  if (!state) {
    if (key_owned) free(key_owned);
    if (iv_owned) free(iv_owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  state->ctx = EVP_CIPHER_CTX_new();
  state->encrypt = encrypt;
  state->is_gcm = EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE;
  if (!state->ctx) {
    crypto_cipher_state_free(state);
    if (key_owned) free(key_owned);
    if (iv_owned) free(iv_owned);
    return js_mkerr(js, "Cipher initialization failed");
  }

  int ok = encrypt
    ? EVP_EncryptInit_ex(state->ctx, cipher, NULL, NULL, NULL)
    : EVP_DecryptInit_ex(state->ctx, cipher, NULL, NULL, NULL);
  if (ok && state->is_gcm && iv) {
    ok = EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
  }
  ok = ok && (encrypt
    ? EVP_EncryptInit_ex(state->ctx, NULL, NULL, key, iv)
    : EVP_DecryptInit_ex(state->ctx, NULL, NULL, key, iv));

  if (key_owned) free(key_owned);
  if (iv_owned) free(iv_owned);
  if (!ok) {
    crypto_cipher_state_free(state);
    return js_mkerr(js, "Cipher initialization failed");
  }

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_cipher_update));
  js_set(js, obj, "final", Ant_Function_Create(js_cipher_final));
  js_set(js, obj, "setAAD", Ant_Function_Create(js_cipher_set_aad));
  js_set(js, obj, "getAuthTag", Ant_Function_Create(js_cipher_get_auth_tag));
  js_set(js, obj, "setAuthTag", Ant_Function_Create(js_cipher_set_auth_tag));
  js_set(js, obj, "setAutoPadding", Ant_Function_Create(js_cipher_set_auto_padding));
  js_set_native(obj, state, CRYPTO_CIPHER_NATIVE_TAG);
  js_set_finalizer(obj, crypto_cipher_finalize);
  return obj;
}

static ant_value_t js_crypto_create_cipheriv(ant_params_t) {
  return crypto_create_cipheriv(js, args, nargs, true);
}

static ant_value_t js_crypto_create_decipheriv(ant_params_t) {
  return crypto_create_cipheriv(js, args, nargs, false);
}

static ant_value_t crypto_pbkdf2_result(
  ant_t *js, ant_value_t password_val, ant_value_t salt_val,
  ant_value_t iterations_val, ant_value_t keylen_val, ant_value_t digest_val
) {
  const uint8_t *password = NULL, *salt = NULL;
  size_t password_len = 0, salt_len = 0;
  uint8_t *password_owned = NULL, *salt_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, password_val, Ant_Value_Undefined, &password, &password_len, &password_owned);
  if (is_err(err)) return err;
  err = crypto_get_input_bytes(js, salt_val, Ant_Value_Undefined, &salt, &salt_len, &salt_owned);
  if (is_err(err)) goto cleanup_err;

  int iterations = (int)Ant_Value_AsNumberUnchecked(iterations_val);
  int keylen = (int)Ant_Value_AsNumberUnchecked(keylen_val);
  if (iterations <= 0 || keylen < 0) {
    err = js_mkerr(js, "Invalid PBKDF2 parameters");
    goto cleanup_err;
  }

  ant_value_t digest_str = js_tostring_val(js, digest_val);
  if (is_err(digest_str)) {
    err = digest_str;
    goto cleanup_err;
  }
  size_t digest_len = 0;
  const char *digest = js_getstr(js, digest_str, &digest_len);
  const EVP_MD *md = crypto_digest_from_name(digest, digest_len);
  if (!md) {
    err = js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported PBKDF2 digest");
    goto cleanup_err;
  }

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) {
    err = Ant_Error_CreateOutOfMemory(js);
    goto cleanup_err;
  }
  if (PKCS5_PBKDF2_HMAC((const char *)password, (int)password_len, salt, (int)salt_len, iterations, md, keylen, out) != 1) {
    free(out);
    err = js_mkerr(js, "PBKDF2 failed");
    goto cleanup_err;
  }

  ant_value_t result = crypto_make_buffer(js, out, (size_t)keylen);
  free(out);
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return result;

cleanup_err:
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return err;
}

static ant_value_t js_crypto_pbkdf2_sync(ant_params_t) {
  if (nargs < 5) return js_mkerr(js, "pbkdf2Sync requires password, salt, iterations, keylen, and digest");
  return crypto_pbkdf2_result(js, args[0], args[1], args[2], args[3], args[4]);
}

static ant_value_t js_crypto_pbkdf2(ant_params_t) {
  if (nargs < 6 || (vtype(args[5]) != kTypeFunction && vtype(args[5]) != kTypeBuiltin)) {
    return js_mkerr(js, "pbkdf2 requires a callback");
  }
  ant_value_t result = crypto_pbkdf2_result(js, args[0], args[1], args[2], args[3], args[4]);
  ant_value_t cb_args[2] = { is_err(result) ? result : Ant_Value_Null, is_err(result) ? Ant_Value_Undefined : result };
  ant_value_t cb_result = Ant_Error_CallCallback(js, args[5], Ant_Value_Undefined, cb_args, 2);
  return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
}

static ant_value_t crypto_scrypt_result(ant_native_params_t, int options_index) {
  const uint8_t *password = NULL, *salt = NULL;
  size_t password_len = 0, salt_len = 0;
  uint8_t *password_owned = NULL, *salt_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[0], Ant_Value_Undefined, &password, &password_len, &password_owned);
  if (is_err(err)) return err;
  err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &salt, &salt_len, &salt_owned);
  if (is_err(err)) goto cleanup_err;

  int keylen = (int)Ant_Value_AsNumberUnchecked(args[2]);
  if (keylen < 0) {
    err = js_mkerr(js, "Invalid scrypt key length");
    goto cleanup_err;
  }

  uint64_t N = 16384, r = 8, p = 1, maxmem = 32 * 1024 * 1024;
  if (options_index >= 0 && options_index < nargs && is_object_type(args[options_index])) {
    ant_value_t v = js_get(js, args[options_index], "N");
    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "cost");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) N = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "r");

    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "blockSize");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) r = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "p");

    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "parallelization");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) p = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "maxmem");

    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) maxmem = (uint64_t)Ant_Value_AsNumberUnchecked(v);
  }

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) {
    err = Ant_Error_CreateOutOfMemory(js);
    goto cleanup_err;
  }

  if (EVP_PBE_scrypt(
    (const char *)password, password_len,
    salt, salt_len, N, r, p, maxmem, out, (size_t)keylen) != 1
  ) {
    free(out);
    err = js_mkerr(js, "scrypt failed");
    goto cleanup_err;
  }

  ant_value_t result = crypto_make_buffer(js, out, (size_t)keylen);
  free(out);
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return result;

cleanup_err:
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return err;
}

static ant_value_t js_crypto_scrypt_sync(ant_params_t) {
  if (nargs < 3) return js_mkerr(js, "scryptSync requires password, salt, and keylen");
  return crypto_scrypt_result(js, args, nargs, 3);
}

static ant_value_t js_crypto_scrypt(ant_params_t) {
  if (nargs < 4) return js_mkerr(js, "scrypt requires a callback");
  int callback_index = (vtype(args[3]) == kTypeFunction || vtype(args[3]) == kTypeBuiltin) ? 3 : 4;
  if (callback_index >= nargs || (vtype(args[callback_index]) != kTypeFunction && vtype(args[callback_index]) != kTypeBuiltin))
    return js_mkerr(js, "scrypt requires a callback");

  ant_value_t result = crypto_scrypt_result(js, args, nargs, callback_index == 3 ? -1 : 3);
  ant_value_t cb_args[2] = { is_err(result) ? result : Ant_Value_Null, is_err(result) ? Ant_Value_Undefined : result };
  ant_value_t cb_result = Ant_Error_CallCallback(js, args[callback_index], Ant_Value_Undefined, cb_args, 2);

  return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
}

static ant_value_t js_crypto_subtle_wrap_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_wrap_key_impl);
}

static ant_value_t js_crypto_subtle_unwrap_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_unwrap_key_impl);
}

static ant_value_t js_crypto_subtle_derive_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_derive_key_impl);
}

static ant_value_t create_crypto_obj(ant_t *js) {
  ant_value_t crypto_obj = Ant_Object_Create(js);
  ant_value_t subtle_obj = Ant_Object_Create(js);
  
  js_set(js, crypto_obj, "random", Ant_Function_Create(js_crypto_random));
  js_set(js, crypto_obj, "randomBytes", Ant_Function_CreateWithArity(js_crypto_random_bytes, 2));
  js_set(js, crypto_obj, "randomFillSync", Ant_Function_CreateWithArity(js_crypto_random_fill_sync, 1));
  js_set(js, crypto_obj, "randomUUID", Ant_Function_CreateWithArity(js_crypto_random_uuid, 1));
  js_set(js, crypto_obj, "randomUUIDv7", Ant_Function_Create(js_crypto_random_uuidv7));
  js_set(js, crypto_obj, "getRandomValues", Ant_Function_Create(js_crypto_get_random_values));
  js_set(js, subtle_obj, "wrapKey", Ant_Function_Create(js_crypto_subtle_wrap_key));
  js_set(js, subtle_obj, "unwrapKey", Ant_Function_Create(js_crypto_subtle_unwrap_key));
  js_set(js, subtle_obj, "deriveKey", Ant_Function_Create(js_crypto_subtle_derive_key));
  js_set(js, subtle_obj, "digest", Ant_Function_Create(js_crypto_subtle_digest));
  js_set(js, subtle_obj, "importKey", Ant_Function_Create(js_crypto_subtle_import_key));
  js_set(js, subtle_obj, "generateKey", Ant_Function_Create(js_crypto_subtle_generate_key));
  js_set(js, subtle_obj, "exportKey", Ant_Function_Create(js_crypto_subtle_export_key));
  js_set(js, subtle_obj, "deriveBits", Ant_Function_Create(js_crypto_subtle_derive_bits));
  js_set(js, subtle_obj, "sign", Ant_Function_Create(js_crypto_subtle_sign));
  js_set(js, subtle_obj, "verify", Ant_Function_Create(js_crypto_subtle_verify));
  js_set(js, subtle_obj, "encrypt", Ant_Function_Create(js_crypto_subtle_encrypt));
  js_set(js, subtle_obj, "decrypt", Ant_Function_Create(js_crypto_subtle_decrypt));
  js_set(js, subtle_obj, "timingSafeEqual", Ant_Function_Create(js_crypto_timing_safe_equal));
  js_set_tostringtag(js, subtle_obj, js_mkstr(js, "SubtleCrypto", 12));
  js_set(js, crypto_obj, "subtle", subtle_obj);
  
  js_set_tostringtag(js, crypto_obj, js_mkstr(js, "Crypto", 6));
  return crypto_obj;
}

void init_crypto_module(ant_t *js) {
  js->mutable_roots.crypto_array_iterator_next = js_get(js, js->sym.array_iterator_proto, "next");
  for (unsigned i = 0; i < 8; i++)
    js->mutable_roots.crypto_usage_strings[i] = js_mkstr(js, crypto_key_usage_names[i], strlen(crypto_key_usage_names[i]));
  ant_value_t crypto_obj = create_crypto_obj(js);
  js_set(js, js_glob(js), "crypto", crypto_obj);
}

ant_value_t crypto_library(ant_t *js) {
  ant_value_t lib = Ant_Object_CreateBare(js);
  ant_value_t webcrypto = create_crypto_obj(js);
  
  js_set(js, lib, "webcrypto", webcrypto);
  js_set(js, lib, "subtle", js_get(js, webcrypto, "subtle"));
  js_set(js, lib, "hash", Ant_Function_CreateWithArity(js_crypto_hash, 3));
  js_set(js, lib, "createHash", Ant_Function_CreateWithArity(js_crypto_create_hash, 2));
  js_set(js, lib, "createHmac", Ant_Function_CreateWithArity(js_crypto_create_hmac, 3));
  js_set(js, lib, "createCipheriv", Ant_Function_CreateWithArity(js_crypto_create_cipheriv, 4));
  js_set(js, lib, "createDecipheriv", Ant_Function_CreateWithArity(js_crypto_create_decipheriv, 4));
  js_set(js, lib, "pbkdf2", Ant_Function_CreateWithArity(js_crypto_pbkdf2, 6));
  js_set(js, lib, "pbkdf2Sync", Ant_Function_CreateWithArity(js_crypto_pbkdf2_sync, 5));
  js_set(js, lib, "scrypt", Ant_Function_CreateWithArity(js_crypto_scrypt, 4));
  js_set(js, lib, "scryptSync", Ant_Function_CreateWithArity(js_crypto_scrypt_sync, 3));
  js_set(js, lib, "randomBytes", Ant_Function_CreateWithArity(js_crypto_random_bytes, 2));
  crypto_define_random_aliases(js, lib);
  js_set(js, lib, "randomFill", Ant_Function_CreateWithArity(js_crypto_random_fill, 4));
  js_set(js, lib, "randomFillSync", Ant_Function_CreateWithArity(js_crypto_random_fill_sync, 1));
  js_set(js, lib, "randomInt", Ant_Function_CreateWithArity(js_crypto_random_int, 3));
  js_set(js, lib, "randomUUID", Ant_Function_CreateWithArity(js_crypto_random_uuid, 1));
  js_set(js, lib, "getRandomValues", Ant_Function_Create(js_crypto_get_random_values));
  js_set(js, lib, "getCiphers", Ant_Function_Create(js_crypto_get_ciphers));
  js_set(js, lib, "getCurves", Ant_Function_Create(js_crypto_get_curves));
  js_set(js, lib, "getHashes", Ant_Function_Create(js_crypto_get_hashes));
  js_set(js, lib, "timingSafeEqual", Ant_Function_Create(js_crypto_timing_safe_equal));
  js_set_tostringtag(js, lib, js_mkstr(js, "crypto", 6));

  return lib;
}

/* ------------------------------------------------------------------------- *
 * WebCrypto asymmetric keys, JWK, and key wrapping (mbedTLS backend).
 * ------------------------------------------------------------------------- */

static int crypto_mbedtls_rng(void *ctx, unsigned char *buf, size_t len) {
  (void)ctx;
  return crypto_fill_random(buf, len) == 0 ? 0 : -1;
}

typedef struct {
  const char *name;
  mbedtls_ecp_group_id id;
  size_t coords;
  const uint8_t *oid;
  size_t oid_len;
} crypto_curve_info_t;

static const uint8_t k_oid_ec_public_key[] = {
  0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01
};
static const uint8_t k_oid_rsa_encryption[] = {
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
};
static const uint8_t k_oid_secp256r1[] = {
  0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};
static const uint8_t k_oid_secp384r1[] = {
  0x2b, 0x81, 0x04, 0x00, 0x22
};
static const uint8_t k_oid_secp521r1[] = {
  0x2b, 0x81, 0x04, 0x00, 0x23
};

#define CRYPTO_CURVE_ENTRY(NAME, ID, COORDS, OID) \
  { NAME, ID, COORDS, OID, sizeof(OID) }

static const crypto_curve_info_t crypto_curve_info[] = {
  CRYPTO_CURVE_ENTRY("P-256", MBEDTLS_ECP_DP_SECP256R1, 32, k_oid_secp256r1),
  CRYPTO_CURVE_ENTRY("secp256r1", MBEDTLS_ECP_DP_SECP256R1, 32, k_oid_secp256r1),
  CRYPTO_CURVE_ENTRY("prime256v1", MBEDTLS_ECP_DP_SECP256R1, 32, k_oid_secp256r1),
  CRYPTO_CURVE_ENTRY("P-384", MBEDTLS_ECP_DP_SECP384R1, 48, k_oid_secp384r1),
  CRYPTO_CURVE_ENTRY("secp384r1", MBEDTLS_ECP_DP_SECP384R1, 48, k_oid_secp384r1),
  CRYPTO_CURVE_ENTRY("P-521", MBEDTLS_ECP_DP_SECP521R1, 66, k_oid_secp521r1),
  CRYPTO_CURVE_ENTRY("secp521r1", MBEDTLS_ECP_DP_SECP521R1, 66, k_oid_secp521r1),
};

static const crypto_curve_info_t *crypto_curve_by_name(const char *name, size_t len) {
  if (!name) return NULL;
  for (size_t i = 0; i < sizeof(crypto_curve_info) / sizeof(crypto_curve_info[0]); i++) {
    const char *candidate = crypto_curve_info[i].name;
    if (strlen(candidate) == len && strncasecmp(name, candidate, len) == 0)
      return &crypto_curve_info[i];
  }
  return NULL;
}

static const crypto_curve_info_t *crypto_curve_by_id(mbedtls_ecp_group_id id) {
  for (size_t i = 0; i < sizeof(crypto_curve_info) / sizeof(crypto_curve_info[0]); i++) {
    if (crypto_curve_info[i].id == id) return &crypto_curve_info[i];
  }
  return NULL;
}

static bool crypto_name_is(const char *name, size_t len, const char *expected) {
  return name && strlen(expected) == len && strncasecmp(name, expected, len) == 0;
}

static ant_crypto_key_kind_t crypto_asym_kind_from_name(const char *name, size_t len) {
  if (crypto_name_is(name, len, "RSASSA-PKCS1-v1_5")) return CRYPTO_KEY_RSA_PKCS1;
  if (crypto_name_is(name, len, "RSA-PSS")) return CRYPTO_KEY_RSA_PSS;
  if (crypto_name_is(name, len, "RSA-OAEP")) return CRYPTO_KEY_RSA_OAEP;
  if (crypto_name_is(name, len, "ECDSA")) return CRYPTO_KEY_ECDSA;
  if (crypto_name_is(name, len, "ECDH")) return CRYPTO_KEY_ECDH;
  if (crypto_name_is(name, len, "AES-KW")) return CRYPTO_KEY_AES_KW;
  return 0;
}

static bool crypto_is_asym_kind(ant_crypto_key_kind_t kind) {
  return kind >= CRYPTO_KEY_AES_KW;
}

static mbedtls_md_type_t crypto_md_type(const EVP_MD *md) {
  return md ? (mbedtls_md_type_t)mbedtls_md_get_type(md) : MBEDTLS_MD_NONE;
}

static const EVP_MD *crypto_algorithm_hash(ant_t *js, ant_value_t algorithm) {
  if (!is_object_type(algorithm)) return NULL;
  ant_value_t hash = js_get(js, algorithm, "hash");
  if (is_err(hash) || vtype(hash) == kTypeUndefined) return NULL;
  return crypto_digest_from_algorithm(js, hash);
}

static ant_value_t crypto_check_asym_usages(
  ant_t *js, ant_crypto_key_kind_t kind, uint8_t key_type, uint8_t usages
) {
  uint8_t allowed = 0;
  switch (kind) {
    case CRYPTO_KEY_RSA_PKCS1:
    case CRYPTO_KEY_RSA_PSS:
    case CRYPTO_KEY_ECDSA:
      allowed = key_type == CRYPTO_KEY_TYPE_PRIVATE
        ? CRYPTO_KEY_USAGE_SIGN : CRYPTO_KEY_USAGE_VERIFY;
      break;
    case CRYPTO_KEY_RSA_OAEP:
      allowed = key_type == CRYPTO_KEY_TYPE_PRIVATE
        ? (CRYPTO_KEY_USAGE_DECRYPT | CRYPTO_KEY_USAGE_UNWRAP_KEY)
        : (CRYPTO_KEY_USAGE_ENCRYPT | CRYPTO_KEY_USAGE_WRAP_KEY);
      break;
    case CRYPTO_KEY_ECDH:
      allowed = key_type == CRYPTO_KEY_TYPE_PRIVATE
        ? (CRYPTO_KEY_USAGE_DERIVE_KEY | CRYPTO_KEY_USAGE_DERIVE_BITS) : 0;
      break;
    case CRYPTO_KEY_AES_KW:
      allowed = CRYPTO_KEY_USAGE_WRAP_KEY | CRYPTO_KEY_USAGE_UNWRAP_KEY;
      break;
    default:
      allowed = 0;
      break;
  }
  return (usages & ~allowed)
    ? crypto_dom_exception_error(js, "Unsupported key usage for algorithm", "SyntaxError")
    : Ant_Value_Undefined;
}

static ant_crypto_key_t *crypto_key_alloc_asym_uninitialized(
  ant_crypto_key_kind_t kind, uint8_t key_type
) {
  ant_crypto_key_t *key = crypto_key_alloc(0);
  if (!key) return NULL;
  key->kind = kind;
  key->key_type = key_type;

  if (kind == CRYPTO_KEY_AES_KW) return key;

  mbedtls_pk_context *pk = calloc(1, sizeof(*pk));
  if (!pk) { crypto_key_free(key); return NULL; }
  mbedtls_pk_init(pk);
  key->pkey = pk;
  return key;
}

static ant_crypto_key_t *crypto_key_alloc_asym(
  ant_crypto_key_kind_t kind, uint8_t key_type
) {
  ant_crypto_key_t *key = crypto_key_alloc_asym_uninitialized(kind, key_type);
  if (!key || !key->pkey) return key;

  const mbedtls_pk_info_t *info = kind == CRYPTO_KEY_ECDSA || kind == CRYPTO_KEY_ECDH
    ? mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)
    : mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
  if (!info || mbedtls_pk_setup((mbedtls_pk_context *)key->pkey, info) != 0) {
    crypto_key_free(key);
    return NULL;
  }
  return key;
}

static ant_value_t crypto_jwk_get_bytes(ant_t *js, ant_value_t jwk,
                                        const char *name, uint8_t **out, size_t *out_len) {
  *out = NULL;
  *out_len = 0;
  if (!is_object_type(jwk)) return Ant_Value_Undefined;
  ant_value_t value = js_get(js, jwk, name);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeUndefined || vtype(value) == kTypeNull) return Ant_Value_Undefined;
  if (vtype(value) != kTypeString)
    return js_mkerr_typed(js, JS_ERR_TYPE, "JWK members must be strings");

  size_t len = 0;
  const char *text = js_getstr(js, value, &len);
  uint8_t *bytes = ant_base64_decode_loose(text ? text : "", len, out_len);
  if (!bytes) return Ant_Error_CreateOutOfMemory(js);
  *out = bytes;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_jwk_get_string(ant_t *js, ant_value_t jwk,
                                         const char *name, const char **out, size_t *out_len) {
  *out = NULL;
  *out_len = 0;
  ant_value_t value = js_get(js, jwk, name);
  if (is_err(value)) return value;
  if (vtype(value) != kTypeString) return Ant_Value_Undefined;
  *out = js_getstr(js, value, out_len);
  return Ant_Value_Undefined;
}

static ant_value_t crypto_jwk_from_bytes(ant_t *js, const uint8_t *bytes, size_t len) {
  ant_value_t json = js_get(js, js_glob(js), "JSON");
  if (is_err(json)) return json;
  ant_value_t parse = js_get(js, json, "parse");
  if (is_err(parse)) return parse;

  ant_value_t text = js_mkstr(js, (const char *)bytes, len);
  if (is_err(text)) return text;
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, json);
  GC_ROOT_PIN(js, parse);
  GC_ROOT_PIN(js, text);

  ant_value_t args[1] = { text };
  ant_value_t result = sv_vm_call(js->vm, js, parse, json, args, 1, NULL, Ant_Value_Undefined);
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_jwk_to_text(ant_t *js, ant_value_t jwk,
                                      const char **out, size_t *out_len) {
  *out = NULL;
  *out_len = 0;
  ant_value_t json = js_get(js, js_glob(js), "JSON");
  if (is_err(json)) return json;
  ant_value_t stringify = js_get(js, json, "stringify");
  if (is_err(stringify)) return stringify;

  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, json);
  GC_ROOT_PIN(js, stringify);
  GC_ROOT_PIN(js, jwk);
  ant_value_t args[1] = { jwk };
  ant_value_t result = sv_vm_call(js->vm, js, stringify, json, args, 1, NULL, Ant_Value_Undefined);
  if (is_err(result) || Ant_Exception_Pending(js)) {
    GC_ROOT_RESTORE(js, mark);
    return is_err(result) ? result : Ant_Exception_Current(js);
  }
  if (vtype(result) != kTypeString) {
    GC_ROOT_RESTORE(js, mark);
    return js_mkerr(js, "JSON.stringify did not return a string");
  }
  GC_ROOT_PIN(js, result);
  *out = js_getstr(js, result, out_len);
  GC_ROOT_RESTORE(js, mark);
  return Ant_Value_Undefined;
}

static ant_value_t crypto_jwk_prop(ant_t *js, ant_value_t obj,
                                   const char *name, ant_value_t value) {
  ant_value_t key = js_mkstr(js, name, strlen(name));
  if (is_err(key)) return key;
  return mkprop(js, obj, key, value, ANT_PROP_ATTR_ENUMERABLE);
}

static ant_value_t crypto_jwk_b64_prop(ant_t *js, ant_value_t obj,
                                       const char *name, const uint8_t *data, size_t len) {
  size_t encoded_len = 0;
  char *encoded = ant_base64url_encode(data, len, &encoded_len);
  if (!encoded) return Ant_Error_CreateOutOfMemory(js);
  ant_value_t value = js_mkstr(js, encoded, encoded_len);
  free(encoded);
  if (is_err(value)) return value;
  return crypto_jwk_prop(js, obj, name, value);
}

static ant_value_t crypto_jwk_mpi_prop(ant_t *js, ant_value_t obj,
                                       const char *name, const mbedtls_mpi *mpi) {
  size_t len = mbedtls_mpi_size(mpi);
  uint8_t *bytes = malloc(len ? len : 1);
  if (!bytes) return Ant_Error_CreateOutOfMemory(js);
  int rc = mbedtls_mpi_write_binary(mpi, bytes, len);
  ant_value_t result = rc == 0
    ? crypto_jwk_b64_prop(js, obj, name, bytes, len)
    : js_mkerr(js, "JWK parameter export failed");
  free(bytes);
  return result;
}

static ant_value_t crypto_asym_algorithm(ant_t *js, ant_crypto_key_t *key, uint32_t bits) {
  const char *name = NULL;
  switch (key->kind) {
    case CRYPTO_KEY_AES_KW: name = "AES-KW"; break;
    case CRYPTO_KEY_RSA_PKCS1: name = "RSASSA-PKCS1-v1_5"; break;
    case CRYPTO_KEY_RSA_PSS: name = "RSA-PSS"; break;
    case CRYPTO_KEY_RSA_OAEP: name = "RSA-OAEP"; break;
    case CRYPTO_KEY_ECDSA: name = "ECDSA"; break;
    case CRYPTO_KEY_ECDH: name = "ECDH"; break;
    default: return js_mkerr(js, "Unsupported asymmetric algorithm");
  }

  ant_value_t obj = Ant_Object_CreateBare(js);
  if (is_err(obj)) return obj;
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, obj);
  ant_value_t result = Ant_Value_Undefined;

#define CRYPTO_ALG_PROP(cname, value) do { \
    result = js_mkprop_fast(js, obj, cname, strlen(cname), value); \
    if (is_err(result)) goto alg_done; \
  } while (0)

  CRYPTO_ALG_PROP("name", js_mkstr(js, name, strlen(name)));
  if (Ant_Exception_Pending(js)) { result = Ant_Exception_Current(js); goto alg_done; }

  if (key->kind == CRYPTO_KEY_AES_KW) {
    CRYPTO_ALG_PROP("length", Ant_Value_FromNumber((double)bits));
  } else if (key->kind == CRYPTO_KEY_ECDSA || key->kind == CRYPTO_KEY_ECDH) {
    CRYPTO_ALG_PROP("namedCurve", js_mkstr(js, key->curve, strlen(key->curve)));
    if (Ant_Exception_Pending(js)) { result = Ant_Exception_Current(js); goto alg_done; }
  } else {
    CRYPTO_ALG_PROP("modulusLength", Ant_Value_FromNumber((double)bits));

    mbedtls_rsa_context *rsa = key->pkey ? mbedtls_pk_rsa(*(mbedtls_pk_context *)key->pkey) : NULL;
    mbedtls_mpi e;
    mbedtls_mpi_init(&e);
    ant_value_t exponent = Ant_Value_Undefined;
    if (rsa && mbedtls_rsa_export(rsa, NULL, NULL, NULL, NULL, &e) == 0) {
      size_t len = mbedtls_mpi_size(&e);
      uint8_t *buf = malloc(len ? len : 1);
      if (buf && mbedtls_mpi_write_binary(&e, buf, len) == 0)
        exponent = crypto_make_buffer(js, buf, len);
      free(buf);
    }
    mbedtls_mpi_free(&e);
    if (is_err(exponent)) { result = exponent; goto alg_done; }
    if (Ant_Exception_Pending(js)) { result = Ant_Exception_Current(js); goto alg_done; }
    CRYPTO_ALG_PROP("publicExponent", exponent);

    if (key->md) {
      unsigned index = crypto_hash_record_index(key->md);
      if (!index) {
        result = crypto_dom_exception_error(js, "Unsupported key hash algorithm", "NotSupportedError");
        goto alg_done;
      }
      ant_value_t hash = crypto_new_key_record(js, index);
      if (is_err(hash)) { result = hash; goto alg_done; }
      CRYPTO_ALG_PROP("hash", hash);
    } else {
      CRYPTO_ALG_PROP("hash", Ant_Value_Undefined);
    }
    if (key->kind == CRYPTO_KEY_RSA_PSS)
      CRYPTO_ALG_PROP("saltLength", Ant_Value_FromNumber((double)key->salt_len));
  }

#undef CRYPTO_ALG_PROP

alg_done:
  if (!is_err(result)) result = obj;
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_make_asym_key_object(
  ant_t *js, ant_crypto_key_t *key, uint32_t bits
) {
  GC_ROOT_SAVE(root_mark, js);
  ant_value_t algorithm = Ant_Value_Undefined;
  ant_value_t usages = Ant_Value_Undefined;
  ant_value_t obj = Ant_Value_Undefined;
  ant_value_t type = Ant_Value_Undefined;
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, usages);
  GC_ROOT_PIN(js, obj);
  GC_ROOT_PIN(js, type);

  algorithm = crypto_asym_algorithm(js, key, bits);
  if (is_err(algorithm)) { obj = algorithm; goto asym_done; }

  ant_value_t usage_values[8];
  uint32_t usage_count = 0;
  for (unsigned i = 0; i < 8; i++) {
    if (key->usages & (1u << i))
      usage_values[usage_count++] = js->mutable_roots.crypto_usage_strings[i];
  }
  usages = js_mkarr_dense_literal(js, usage_values, usage_count);
  if (is_err(usages)) { obj = usages; goto asym_done; }

  const char *type_name = key->key_type == CRYPTO_KEY_TYPE_PUBLIC ? "public" :
    key->key_type == CRYPTO_KEY_TYPE_PRIVATE ? "private" : "secret";
  type = js_mkstr(js, type_name, strlen(type_name));
  if (is_err(type)) { obj = type; goto asym_done; }

  obj = Ant_Object_CreateBare(js);
  if (is_err(obj)) goto asym_done;
  GC_ROOT_PIN(js, obj);
  if (is_err(js_mkprop_fast(js, obj, "type", 4, type))) { obj = Ant_Exception_Current(js); goto asym_done; }
  if (is_err(js_mkprop_fast(js, obj, "extractable", 11, Ant_Value_FromBool(key->extractable)))) {
    obj = Ant_Exception_Current(js); goto asym_done;
  }
  if (is_err(js_mkprop_fast(js, obj, "algorithm", 9, algorithm))) {
    obj = Ant_Exception_Current(js); goto asym_done;
  }
  if (is_err(js_mkprop_fast(js, obj, "usages", 6, usages))) {
    obj = Ant_Exception_Current(js); goto asym_done;
  }
  js_set_tostringtag(js, obj, Ant_String_FromLiteral(js, "CryptoKey"));
  if (Ant_Exception_Pending(js)) { obj = Ant_Exception_Current(js); goto asym_done; }
  js_set_native(obj, key, CRYPTO_KEY_NATIVE_TAG);
  js_set_finalizer(obj, crypto_key_finalize);
  key = NULL;

asym_done:
  crypto_key_free(key);
  GC_ROOT_RESTORE(js, root_mark);
  return obj;
}

static ant_value_t crypto_make_key_pair(
  ant_t *js, ant_crypto_key_t *private_key, uint32_t private_bits,
  ant_crypto_key_t *public_key, uint32_t public_bits
) {
  if (!private_key || !public_key) {
    crypto_key_free(private_key);
    crypto_key_free(public_key);
    return Ant_Error_CreateOutOfMemory(js);
  }
  GC_ROOT_SAVE(mark, js);
  ant_value_t pair = Ant_Object_CreateBare(js);
  if (is_err(pair)) { crypto_key_free(public_key); crypto_key_free(private_key); GC_ROOT_RESTORE(js, mark); return pair; }
  GC_ROOT_PIN(js, pair);

  ant_value_t private_obj = crypto_make_asym_key_object(js, private_key, private_bits);
  private_key = NULL;
  if (is_err(private_obj)) { crypto_key_free(public_key); GC_ROOT_RESTORE(js, mark); return private_obj; }
  GC_ROOT_PIN(js, private_obj);

  ant_value_t public_obj = crypto_make_asym_key_object(js, public_key, public_bits);
  public_key = NULL;
  if (is_err(public_obj)) { GC_ROOT_RESTORE(js, mark); return public_obj; }
  GC_ROOT_PIN(js, public_obj);

  ant_value_t result = js_mkprop_fast(js, pair, "privateKey", 10, private_obj);
  if (is_err(result)) result = Ant_Exception_Current(js);
  if (!is_err(result)) result = js_mkprop_fast(js, pair, "publicKey", 9, public_obj);
  if (is_err(result)) result = Ant_Exception_Current(js);
  else result = pair;
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_import_raw_key(
  ant_t *js, const uint8_t *bytes, size_t len, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
) {
  ant_value_t data = create_arraybuffer_copy(js, bytes, len);
  if (is_err(data)) return data;
  ant_value_t format = Ant_String_FromLiteral(js, "raw");
  if (is_err(format)) return format;

  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, data);
  GC_ROOT_PIN(js, format);
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, extractable);
  GC_ROOT_PIN(js, usages);
  ant_value_t args[5] = { format, data, algorithm, extractable, usages };
  ant_value_t result = crypto_subtle_import_key_impl(
    js, args, 5, Ant_Value_Undefined, Ant_Value_Undefined, Ant_Value_Undefined
  );
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static bool crypto_jwk_mpi(ant_t *js, ant_value_t jwk, const char *name,
                           mbedtls_mpi *mpi, bool *present, ant_value_t *error) {
  uint8_t *bytes = NULL;
  size_t len = 0;
  *error = crypto_jwk_get_bytes(js, jwk, name, &bytes, &len);
  if (is_err(*error)) return false;
  *present = bytes != NULL;
  if (!bytes) return true;
  if (len == 0 || mbedtls_mpi_read_binary(mpi, bytes, len) != 0) {
    free(bytes);
    *error = crypto_dom_exception_error(js, "Invalid JWK key parameter", "DataError");
    return false;
  }
  free(bytes);
  return true;
}

static ant_value_t crypto_import_rsa_jwk(
  ant_t *js, ant_value_t jwk, ant_crypto_key_t *key, const EVP_MD *md
) {
  mbedtls_mpi n, e, d, p, q;
  mbedtls_mpi_init(&n); mbedtls_mpi_init(&e); mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&p); mbedtls_mpi_init(&q);
  ant_value_t error = Ant_Value_Undefined;
  bool present = false, private_key = false;

  if (!crypto_jwk_mpi(js, jwk, "n", &n, &present, &error) || !present) goto fail;
  if (!crypto_jwk_mpi(js, jwk, "e", &e, &present, &error) || !present) goto fail;
  if (!crypto_jwk_mpi(js, jwk, "d", &d, &present, &error)) goto fail;
  private_key = present;
  if (!crypto_jwk_mpi(js, jwk, "p", &p, &present, &error)) goto fail;
  if (!crypto_jwk_mpi(js, jwk, "q", &q, &present, &error)) goto fail;

  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*(mbedtls_pk_context *)key->pkey);
  if (mbedtls_rsa_import(rsa, &n, private_key && mbedtls_mpi_size(&p) ? &p : NULL,
                         private_key && mbedtls_mpi_size(&q) ? &q : NULL,
                         private_key ? &d : NULL, &e) != 0 ||
      mbedtls_rsa_complete(rsa) != 0) {
    error = crypto_dom_exception_error(js, "Invalid RSA JWK", "DataError");
    goto fail;
  }
  if (private_key ? mbedtls_rsa_check_privkey(rsa) != 0 : mbedtls_rsa_check_pubkey(rsa) != 0) {
    error = crypto_dom_exception_error(js, "Invalid RSA JWK", "DataError");
    goto fail;
  }
  mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
  if (private_key) key->key_type = CRYPTO_KEY_TYPE_PRIVATE;
  key->bits = (uint32_t)mbedtls_mpi_bitlen(&n);
  key->md = md;

  mbedtls_mpi_free(&n); mbedtls_mpi_free(&e); mbedtls_mpi_free(&d);
  mbedtls_mpi_free(&p); mbedtls_mpi_free(&q);
  return Ant_Value_Undefined;

fail:
  mbedtls_mpi_free(&n); mbedtls_mpi_free(&e); mbedtls_mpi_free(&d);
  mbedtls_mpi_free(&p); mbedtls_mpi_free(&q);
  return is_err(error) ? error : crypto_dom_exception_error(js, "Invalid RSA JWK", "DataError");
}

static ant_value_t crypto_import_ec_jwk(
  ant_t *js, ant_value_t jwk, ant_crypto_key_t *key, const EVP_MD *md
) {
  const char *curve_name = NULL;
  size_t curve_name_len = 0;
  ant_value_t error = crypto_jwk_get_string(js, jwk, "crv", &curve_name, &curve_name_len);
  if (is_err(error)) return error;
  const crypto_curve_info_t *curve = crypto_curve_by_name(curve_name, curve_name_len);
  if (!curve)
    return crypto_dom_exception_error(js, "Unsupported JWK elliptic curve", "NotSupportedError");

  uint8_t *x = NULL, *y = NULL, *d = NULL;
  size_t x_len = 0, y_len = 0, d_len = 0;
  error = crypto_jwk_get_bytes(js, jwk, "x", &x, &x_len);
  if (is_err(error)) goto done;
  error = crypto_jwk_get_bytes(js, jwk, "y", &y, &y_len);
  if (is_err(error)) goto done;
  error = crypto_jwk_get_bytes(js, jwk, "d", &d, &d_len);
  if (is_err(error)) goto done;
  if (!x || !y || x_len != curve->coords || y_len != curve->coords) {
    error = crypto_dom_exception_error(js, "Invalid EC JWK", "DataError");
    goto done;
  }

  mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*(mbedtls_pk_context *)key->pkey);
  if (mbedtls_ecp_group_load(&ec->grp, curve->id) != 0) {
    error = crypto_dom_exception_error(js, "Unsupported EC curve", "NotSupportedError");
    goto done;
  }

  uint8_t *point = malloc(1 + 2 * curve->coords);
  if (!point) { error = Ant_Error_CreateOutOfMemory(js); goto done; }
  mbedtls_ecp_point q;
  mbedtls_ecp_point_init(&q);
  point[0] = 4;
  memcpy(point + 1, x, curve->coords);
  memcpy(point + 1 + curve->coords, y, curve->coords);
  if (mbedtls_ecp_point_read_binary(&ec->grp, &q, point, 1 + 2 * curve->coords) != 0 ||
      mbedtls_ecp_check_pubkey(&ec->grp, &q) != 0 ||
      mbedtls_ecp_copy(&ec->Q, &q) != 0) {
    error = crypto_dom_exception_error(js, "Invalid EC JWK", "DataError");
  } else if (d) {
    key->key_type = CRYPTO_KEY_TYPE_PRIVATE;
    if (d_len == 0 || d_len > curve->coords || mbedtls_mpi_read_binary(&ec->d, d, d_len) != 0 ||
        mbedtls_ecp_check_privkey(&ec->grp, &ec->d) != 0) {
      error = crypto_dom_exception_error(js, "Invalid EC JWK", "DataError");
    }
  }
  mbedtls_ecp_point_free(&q);
  free(point);
  if (is_err(error)) goto done;

  memcpy(key->curve, curve->name, strlen(curve->name) + 1);
  key->bits = (uint32_t)(curve->coords * 8u);
  key->md = md;
  error = Ant_Value_Undefined;

done:
  free(x); free(y); free(d);
  return error;
}

static ant_value_t crypto_import_jwk_key(
  ant_t *js, ant_value_t jwk, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (buffer_source_get_bytes(js, jwk, &bytes, &len)) {
    jwk = crypto_jwk_from_bytes(js, bytes, len);
    if (is_err(jwk)) return jwk;
  }
  if (!is_object_type(jwk))
    return js_mkerr_typed(js, JS_ERR_TYPE, "JWK keyData must be an object");

  ant_crypto_key_t *key = NULL;
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, jwk);
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, extractable);
  GC_ROOT_PIN(js, usages);

  const char *kty = NULL;
  size_t kty_len = 0;
  ant_value_t result = crypto_jwk_get_string(js, jwk, "kty", &kty, &kty_len);
  if (is_err(result)) goto done;
  if (crypto_name_is(kty, kty_len, "oct")) {
    uint8_t *secret = NULL;
    size_t secret_len = 0;
    result = crypto_jwk_get_bytes(js, jwk, "k", &secret, &secret_len);
    if (is_err(result)) goto done;
    if (!secret) {
      result = crypto_dom_exception_error(js, "JWK oct key requires k", "DataError");
      goto done;
    }
    result = crypto_import_raw_key(js, secret, secret_len, algorithm, extractable, usages);
    free(secret);
    goto done;
  }

  ant_value_t name_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_val)) { result = name_val; goto done; }
  size_t name_len = 0;
  const char *name = js_getstr(js, name_val, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (!kind) {
    result = crypto_dom_exception_error(js, "Unsupported JWK key algorithm", "NotSupportedError");
    goto done;
  }

  uint8_t usage_mask = 0;
  result = crypto_read_key_usages(js, usages, &usage_mask);
  if (is_err(result)) goto done;

  const EVP_MD *md = crypto_algorithm_hash(js, algorithm);
  if ((kind == CRYPTO_KEY_RSA_PSS || kind == CRYPTO_KEY_RSA_OAEP) && !md) {
    result = crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");
    goto done;
  }

  key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_PUBLIC);
  if (!key) { result = Ant_Error_CreateOutOfMemory(js); goto done; }
  key->extractable = js_truthy(js, extractable);
  key->usages = usage_mask;

  if (crypto_name_is(kty, kty_len, "RSA")) {
    if (kind == CRYPTO_KEY_ECDSA || kind == CRYPTO_KEY_ECDH) {
      crypto_key_free(key);
      result = crypto_dom_exception_error(js, "JWK kty does not match algorithm", "DataError");
      goto done;
    }
    result = crypto_import_rsa_jwk(js, jwk, key, md);
  } else if (crypto_name_is(kty, kty_len, "EC")) {
    if (kind != CRYPTO_KEY_ECDSA && kind != CRYPTO_KEY_ECDH) {
      crypto_key_free(key);
      result = crypto_dom_exception_error(js, "JWK kty does not match algorithm", "DataError");
      goto done;
    }
    result = crypto_import_ec_jwk(js, jwk, key, md);
  } else {
    result = crypto_dom_exception_error(js, "Unsupported JWK kty", "NotSupportedError");
  }
  if (is_err(result)) { crypto_key_free(key); goto done; }

  result = crypto_check_asym_usages(js, key->kind, key->key_type, usage_mask);
  if (is_err(result)) { crypto_key_free(key); goto done; }
  if (kind == CRYPTO_KEY_RSA_PSS) {
    ant_value_t salt = is_object_type(algorithm) ? js_get(js, algorithm, "saltLength") : Ant_Value_Undefined;
    if (is_err(salt)) { crypto_key_free(key); result = salt; goto done; }
    if (vtype(salt) == kTypeNumber) {
      double value = Ant_Value_AsNumberUnchecked(salt);
      if (value < 0 || value > 65535 || floor(value) != value) {
        crypto_key_free(key);
        result = js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA-PSS saltLength");
        goto done;
      }
      key->salt_len = (uint32_t)value;
    } else if (md) {
      key->salt_len = mbedtls_md_get_size(mbedtls_md_info_from_type(crypto_md_type(md)));
    }
  }

  result = crypto_make_asym_key_object(js, key, key->bits);
  key = NULL;

done:
  crypto_key_free(key);
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_export_rsa_jwk(ant_t *js, ant_crypto_key_t *key) {
  ant_value_t jwk = Ant_Object_CreateBare(js);
  if (is_err(jwk)) return jwk;
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, jwk);
  ant_value_t result = Ant_Value_Undefined;

  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*(mbedtls_pk_context *)key->pkey);
  mbedtls_mpi n, e, d, p, q, dp, dq, qp;
  mbedtls_mpi_init(&n); mbedtls_mpi_init(&e); mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&p); mbedtls_mpi_init(&q); mbedtls_mpi_init(&dp);
  mbedtls_mpi_init(&dq); mbedtls_mpi_init(&qp);
  if (mbedtls_rsa_export(rsa, &n, key->key_type == CRYPTO_KEY_TYPE_PRIVATE ? &p : NULL,
                         key->key_type == CRYPTO_KEY_TYPE_PRIVATE ? &q : NULL,
                         key->key_type == CRYPTO_KEY_TYPE_PRIVATE ? &d : NULL, &e) != 0) {
    result = js_mkerr(js, "RSA JWK export failed");
    goto rsa_done;
  }
  if (key->key_type == CRYPTO_KEY_TYPE_PRIVATE &&
      mbedtls_rsa_export_crt(rsa, &dp, &dq, &qp) != 0) {
    result = js_mkerr(js, "RSA JWK export failed");
    goto rsa_done;
  }

#define JWK_MPI(name, mpi) do { \
    result = crypto_jwk_mpi_prop(js, jwk, name, mpi); \
    if (is_err(result)) goto rsa_done; \
  } while (0)

  result = crypto_jwk_prop(js, jwk, "kty", Ant_String_FromLiteral(js, "RSA"));
  if (is_err(result)) goto rsa_done;
  JWK_MPI("n", &n);
  JWK_MPI("e", &e);
  if (key->key_type == CRYPTO_KEY_TYPE_PRIVATE) {
    JWK_MPI("d", &d);
    JWK_MPI("p", &p);
    JWK_MPI("q", &q);
    JWK_MPI("dp", &dp);
    JWK_MPI("dq", &dq);
    JWK_MPI("qi", &qp);
  }
#undef JWK_MPI

rsa_done:
  mbedtls_mpi_free(&n); mbedtls_mpi_free(&e); mbedtls_mpi_free(&d);
  mbedtls_mpi_free(&p); mbedtls_mpi_free(&q); mbedtls_mpi_free(&dp);
  mbedtls_mpi_free(&dq); mbedtls_mpi_free(&qp);
  if (!is_err(result)) result = jwk;
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_export_ec_jwk(ant_t *js, ant_crypto_key_t *key) {
  ant_value_t jwk = Ant_Object_CreateBare(js);
  if (is_err(jwk)) return jwk;
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, jwk);
  ant_value_t result = Ant_Value_Undefined;

  const crypto_curve_info_t *curve = crypto_curve_by_name(key->curve, strlen(key->curve));
  if (!curve) { result = js_mkerr(js, "Unsupported EC curve"); goto ec_done; }

  mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*(mbedtls_pk_context *)key->pkey);
  uint8_t *point = malloc(1 + 2 * curve->coords);
  if (!point) { result = Ant_Error_CreateOutOfMemory(js); goto ec_done; }
  size_t point_len = 0;
  if (mbedtls_ecp_point_write_binary(&ec->grp, &ec->Q, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                     &point_len, point, 1 + 2 * curve->coords) != 0 ||
      point_len != 1 + 2 * curve->coords) {
    free(point);
    result = js_mkerr(js, "EC JWK export failed");
    goto ec_done;
  }

  result = crypto_jwk_prop(js, jwk, "kty", Ant_String_FromLiteral(js, "EC"));
  if (is_err(result)) { free(point); goto ec_done; }
  result = crypto_jwk_prop(js, jwk, "crv", js_mkstr(js, curve->name, strlen(curve->name)));
  if (is_err(result)) { free(point); goto ec_done; }
  result = crypto_jwk_b64_prop(js, jwk, "x", point + 1, curve->coords);
  if (is_err(result)) { free(point); goto ec_done; }
  result = crypto_jwk_b64_prop(js, jwk, "y", point + 1 + curve->coords, curve->coords);
  free(point);
  if (is_err(result)) goto ec_done;

  if (key->key_type == CRYPTO_KEY_TYPE_PRIVATE) {
    size_t d_len = mbedtls_mpi_size(&ec->d);
    uint8_t *d = malloc(d_len ? d_len : 1);
    if (!d) { result = Ant_Error_CreateOutOfMemory(js); goto ec_done; }
    int rc = mbedtls_mpi_write_binary(&ec->d, d, d_len);
    if (rc == 0) result = crypto_jwk_b64_prop(js, jwk, "d", d, d_len);
    else result = js_mkerr(js, "EC JWK export failed");
    free(d);
  }

ec_done:
  if (!is_err(result)) result = jwk;
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_export_jwk_key(ant_t *js, ant_crypto_key_t *key) {
  if (key->kind == CRYPTO_KEY_RSA_PKCS1 || key->kind == CRYPTO_KEY_RSA_PSS ||
      key->kind == CRYPTO_KEY_RSA_OAEP)
    return crypto_export_rsa_jwk(js, key);
  if (key->kind == CRYPTO_KEY_ECDSA || key->kind == CRYPTO_KEY_ECDH)
    return crypto_export_ec_jwk(js, key);
  return js_mkerr(js, "JWK export is not supported for this key");
}

static uint8_t *crypto_asn1_tlv(
  uint8_t *out, uint8_t tag, const uint8_t *value, size_t len
) {
  *out++ = tag;
  if (len < 128) {
    *out++ = (uint8_t)len;
  } else {
    uint8_t encoded[sizeof(size_t)];
    size_t count = 0;
    size_t remaining = len;
    while (remaining) {
      encoded[count++] = (uint8_t)(remaining & 0xff);
      remaining >>= 8;
    }
    *out++ = (uint8_t)(0x80 | count);
    for (size_t i = count; i > 0; i--) *out++ = encoded[i - 1];
  }
  if (len) memcpy(out, value, len);
  return out + len;
}

static ant_value_t crypto_export_der_key(ant_t *js, bool private_key, ant_crypto_key_t *key) {
  uint8_t buffer[16384];
  int len = private_key
    ? mbedtls_pk_write_key_der((mbedtls_pk_context *)key->pkey, buffer, sizeof(buffer))
    : mbedtls_pk_write_pubkey_der((mbedtls_pk_context *)key->pkey, buffer, sizeof(buffer));
  if (len <= 0) return js_mkerr(js, "DER key export failed");
  const uint8_t *data = buffer + sizeof(buffer) - (size_t)len;

  if (!private_key)
    return create_arraybuffer_copy(js, data, (size_t)len);

  /* WebCrypto pkcs8 requires PrivateKeyInfo, not the raw PKCS#1/SEC1 key that
   * mbedtls_pk_write_key_der emits. Wrap it with the matching algorithm OID. */
  uint8_t inner[16384];
  uint8_t *p = crypto_asn1_tlv(inner, 0x02, (const uint8_t *)"\0", 1);

  uint8_t algorithm[128];
  uint8_t *a = algorithm;
  bool ec_kind = key->kind == CRYPTO_KEY_ECDSA || key->kind == CRYPTO_KEY_ECDH;
  if (ec_kind) {
    const crypto_curve_info_t *curve = crypto_curve_by_name(key->curve, strlen(key->curve));
    if (!curve || curve->oid_len > sizeof(algorithm) - sizeof(k_oid_ec_public_key) - 4)
      return js_mkerr(js, "EC PKCS#8 export failed");
    a = crypto_asn1_tlv(a, 0x06, k_oid_ec_public_key, sizeof(k_oid_ec_public_key));
    a = crypto_asn1_tlv(a, 0x06, curve->oid, curve->oid_len);
  } else {
    if (sizeof(k_oid_rsa_encryption) + 4 > sizeof(algorithm))
      return js_mkerr(js, "RSA PKCS#8 export failed");
    a = crypto_asn1_tlv(a, 0x06, k_oid_rsa_encryption, sizeof(k_oid_rsa_encryption));
    a = crypto_asn1_tlv(a, 0x05, NULL, 0);
  }
  p = crypto_asn1_tlv(p, 0x30, algorithm, (size_t)(a - algorithm));
  p = crypto_asn1_tlv(p, 0x04, data, (size_t)len);

  uint8_t output[16384];
  uint8_t *end = crypto_asn1_tlv(output, 0x30, inner, (size_t)(p - inner));
  if ((size_t)(end - output) > sizeof(output))
    return js_mkerr(js, "PKCS#8 export failed");
  return create_arraybuffer_copy(js, output, (size_t)(end - output));
}

static uint32_t crypto_pss_salt_length(ant_t *js, ant_value_t algorithm, const EVP_MD *md) {
  ant_value_t salt = is_object_type(algorithm) ? js_get(js, algorithm, "saltLength") : Ant_Value_Undefined;
  if (is_err(salt) || vtype(salt) != kTypeNumber) {
    if (md) return (uint32_t)mbedtls_md_get_size(mbedtls_md_info_from_type(crypto_md_type(md)));
    return 0;
  }
  double value = Ant_Value_AsNumberUnchecked(salt);
  if (value < 0 || value > 65535 || floor(value) != value) return UINT32_MAX;
  return (uint32_t)value;
}

static ant_value_t crypto_import_der_key(
  ant_t *js, bool private_key, const uint8_t *bytes, size_t len,
  ant_value_t algorithm, ant_value_t extractable, ant_value_t usages
) {
  ant_value_t name_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_val)) return name_val;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_val, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (!kind)
    return crypto_dom_exception_error(js, "Unsupported DER key algorithm", "NotSupportedError");

  uint8_t usage_mask = 0;
  ant_value_t result = crypto_read_key_usages(js, usages, &usage_mask);
  if (is_err(result)) return result;

  ant_crypto_key_t *key = crypto_key_alloc_asym_uninitialized(kind,
    private_key ? CRYPTO_KEY_TYPE_PRIVATE : CRYPTO_KEY_TYPE_PUBLIC);
  if (!key) return Ant_Error_CreateOutOfMemory(js);
  key->extractable = js_truthy(js, extractable);
  key->usages = usage_mask;

  mbedtls_pk_context *pk = (mbedtls_pk_context *)key->pkey;
  int rc = private_key
    ? mbedtls_pk_parse_key(pk, bytes, len, NULL, 0, NULL, NULL)
    : mbedtls_pk_parse_public_key(pk, bytes, len);
  if (rc != 0) {
    crypto_key_free(key);
    return crypto_dom_exception_error(js, "Invalid DER key data", "DataError");
  }

  mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(pk);
  bool rsa_kind = kind == CRYPTO_KEY_RSA_PKCS1 || kind == CRYPTO_KEY_RSA_PSS ||
    kind == CRYPTO_KEY_RSA_OAEP;
  bool ec_kind = kind == CRYPTO_KEY_ECDSA || kind == CRYPTO_KEY_ECDH;
  if ((rsa_kind && pk_type != MBEDTLS_PK_RSA) ||
      (ec_kind && (pk_type != MBEDTLS_PK_ECKEY && pk_type != MBEDTLS_PK_ECKEY_DH))) {
    crypto_key_free(key);
    return crypto_dom_exception_error(js, "DER key does not match algorithm", "DataError");
  }

  const EVP_MD *md = crypto_algorithm_hash(js, algorithm);
  if ((kind == CRYPTO_KEY_RSA_PSS || kind == CRYPTO_KEY_RSA_OAEP) && !md) {
    crypto_key_free(key);
    return crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");
  }
  key->md = md;
  key->bits = (uint32_t)mbedtls_pk_get_bitlen(pk);

  if (ec_kind) {
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*pk);
    const crypto_curve_info_t *curve = crypto_curve_by_id(ec->grp.id);
    if (!curve) {
      crypto_key_free(key);
      return crypto_dom_exception_error(js, "Unsupported EC curve", "NotSupportedError");
    }
    memcpy(key->curve, curve->name, strlen(curve->name) + 1);
    key->bits = (uint32_t)(curve->coords * 8u);
  } else if (rsa_kind) {
    mbedtls_rsa_set_padding(mbedtls_pk_rsa(*pk), MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
  }

  if (kind == CRYPTO_KEY_RSA_PSS) {
    key->salt_len = crypto_pss_salt_length(js, algorithm, md);
    if (key->salt_len == UINT32_MAX) {
      crypto_key_free(key);
      return js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA-PSS saltLength");
    }
  }

  result = crypto_check_asym_usages(js, kind, key->key_type, usage_mask);
  if (is_err(result)) { crypto_key_free(key); return result; }
  result = crypto_make_asym_key_object(js, key, key->bits);
  return result;
}

static ant_value_t crypto_hash_input_bytes(
  ant_t *js, ant_value_t data, const EVP_MD *md,
  uint8_t *out, size_t out_cap, size_t *out_len
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (!buffer_source_get_bytes(js, data, &bytes, &len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "Signature input must be a BufferSource");
  unsigned int digest_len = 0;
  if (EVP_Digest(bytes, len, out, &digest_len, md, NULL) != 1 || digest_len > out_cap)
    return js_mkerr(js, "Digest failed");
  *out_len = digest_len;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_asym_sign(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm, ant_value_t data
) {
  const EVP_MD *md = crypto_algorithm_hash(js, algorithm);
  if (!md) md = key->md;
  if (!md)
    return crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");
  if (key->key_type != CRYPTO_KEY_TYPE_PRIVATE || key->pkey == NULL)
    return crypto_dom_exception_error(js, "Signing requires a private key", "InvalidAccessError");

  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len = 0;
  ant_value_t result = crypto_hash_input_bytes(js, data, md, digest, sizeof(digest), &digest_len);
  if (is_err(result)) return result;

  mbedtls_pk_context *pk = (mbedtls_pk_context *)key->pkey;
  mbedtls_md_type_t md_type = crypto_md_type(md);
  uint8_t signature[MBEDTLS_MPI_MAX_SIZE];
  size_t sig_len = 0;

  if (key->kind == CRYPTO_KEY_RSA_PKCS1) {
    if (mbedtls_pk_sign(pk, md_type, digest, digest_len, signature, sizeof(signature),
                        &sig_len, crypto_mbedtls_rng, NULL) != 0)
      return js_mkerr(js, "RSA signing failed");
  } else if (key->kind == CRYPTO_KEY_RSA_PSS) {
    uint32_t salt = crypto_pss_salt_length(js, algorithm, md);
    if (salt == UINT32_MAX) return js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA-PSS saltLength");
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, md_type);
    if (mbedtls_rsa_rsassa_pss_sign_ext(rsa, crypto_mbedtls_rng, NULL, md_type,
                                        (unsigned)digest_len, digest, (int)salt, signature) != 0)
      return js_mkerr(js, "RSA-PSS signing failed");
    sig_len = mbedtls_rsa_get_len(rsa);
  } else if (key->kind == CRYPTO_KEY_ECDSA) {
    const crypto_curve_info_t *curve = crypto_curve_by_name(key->curve, strlen(key->curve));
    if (!curve) return js_mkerr(js, "Unsupported EC curve");
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*pk);
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
    int rc = mbedtls_ecdsa_sign(&ec->grp, &r, &s, &ec->d, digest, digest_len,
                                crypto_mbedtls_rng, NULL);
    if (rc == 0) rc = mbedtls_mpi_write_binary(&r, signature, curve->coords);
    if (rc == 0) rc = mbedtls_mpi_write_binary(&s, signature + curve->coords, curve->coords);
    mbedtls_mpi_free(&r); mbedtls_mpi_free(&s);
    if (rc != 0) return js_mkerr(js, "ECDSA signing failed");
    sig_len = 2 * curve->coords;
  } else {
    return crypto_dom_exception_error(js, "Unsupported signing algorithm", "NotSupportedError");
  }

  return create_arraybuffer_copy(js, signature, sig_len);
}

static ant_value_t crypto_asym_verify(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm,
  ant_value_t signature_value, ant_value_t data
) {
  const EVP_MD *md = crypto_algorithm_hash(js, algorithm);
  if (!md) md = key->md;
  if (!md)
    return crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");
  if (key->pkey == NULL)
    return crypto_dom_exception_error(js, "Verification requires a CryptoKey", "InvalidAccessError");

  const uint8_t *sig = NULL;
  size_t sig_len = 0;
  if (!buffer_source_get_bytes(js, signature_value, &sig, &sig_len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "signature must be a BufferSource");

  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len = 0;
  ant_value_t result = crypto_hash_input_bytes(js, data, md, digest, sizeof(digest), &digest_len);
  if (is_err(result)) return result;

  mbedtls_pk_context *pk = (mbedtls_pk_context *)key->pkey;
  mbedtls_md_type_t md_type = crypto_md_type(md);
  int ok = -1;

  if (key->kind == CRYPTO_KEY_RSA_PKCS1) {
    ok = mbedtls_pk_verify(pk, md_type, digest, digest_len, sig, sig_len);
  } else if (key->kind == CRYPTO_KEY_RSA_PSS) {
    uint32_t salt = crypto_pss_salt_length(js, algorithm, md);
    if (salt == UINT32_MAX) return js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA-PSS saltLength");
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, md_type);
    if (sig_len == mbedtls_rsa_get_len(rsa))
      ok = mbedtls_rsa_rsassa_pss_verify_ext(rsa, md_type, (unsigned)digest_len, digest,
                                             md_type, (int)salt, sig);
  } else if (key->kind == CRYPTO_KEY_ECDSA) {
    const crypto_curve_info_t *curve = crypto_curve_by_name(key->curve, strlen(key->curve));
    if (!curve) return js_mkerr(js, "Unsupported EC curve");
    if (sig_len == 2 * curve->coords) {
      mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*pk);
      mbedtls_mpi r, s;
      mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
      if (mbedtls_mpi_read_binary(&r, sig, curve->coords) == 0 &&
          mbedtls_mpi_read_binary(&s, sig + curve->coords, curve->coords) == 0)
        ok = mbedtls_ecdsa_verify(&ec->grp, digest, digest_len, &ec->Q, &r, &s);
      mbedtls_mpi_free(&r); mbedtls_mpi_free(&s);
    }
  } else {
    return crypto_dom_exception_error(js, "Unsupported verification algorithm", "NotSupportedError");
  }

  return Ant_Value_FromBool(ok == 0);
}

static ant_value_t crypto_asym_encrypt(
  ant_t *js, bool encrypt, ant_crypto_key_t *key,
  ant_value_t algorithm, ant_value_t data
) {
  if (key->kind != CRYPTO_KEY_RSA_OAEP || key->pkey == NULL)
    return crypto_dom_exception_error(js, "RSA-OAEP requires an RSA key", "NotSupportedError");
  if (!key->md)
    return crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");

  const uint8_t *input = NULL;
  size_t input_len = 0;
  if (!buffer_source_get_bytes(js, data, &input, &input_len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "RSA-OAEP data must be a BufferSource");

  ant_value_t label_val = is_object_type(algorithm) ? js_get(js, algorithm, "label") : Ant_Value_Undefined;
  if (is_err(label_val)) return label_val;
  const uint8_t *label = NULL;
  size_t label_len = 0;
  if (vtype(label_val) != kTypeUndefined && vtype(label_val) != kTypeNull &&
      !buffer_source_get_bytes(js, label_val, &label, &label_len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "RSA-OAEP label must be a BufferSource");

  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*(mbedtls_pk_context *)key->pkey);
  mbedtls_md_type_t md_type = crypto_md_type(key->md);
  mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, md_type);
  size_t rsa_len = mbedtls_rsa_get_len(rsa);

  uint8_t *out = malloc(encrypt ? rsa_len : rsa_len ? rsa_len : 1);
  if (!out) return Ant_Error_CreateOutOfMemory(js);
  int rc;
  size_t out_len = rsa_len;
  if (encrypt) {
    rc = mbedtls_rsa_rsaes_oaep_encrypt(rsa, crypto_mbedtls_rng, NULL, label, label_len,
                                        input_len, input, out);
  } else {
    rc = mbedtls_rsa_rsaes_oaep_decrypt(rsa, crypto_mbedtls_rng, NULL, label, label_len,
                                        &out_len, input, out, rsa_len);
  }
  if (rc != 0) { free(out); return js_mkerr(js, "RSA-OAEP operation failed"); }
  ant_value_t result = create_arraybuffer_copy(js, out, out_len);
  free(out);
  return result;
}

static ant_value_t crypto_nist_kw(
  ant_t *js, bool wrap, ant_crypto_key_t *key,
  const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len
) {
  *output = NULL;
  *output_len = 0;
  if (key->kind != CRYPTO_KEY_AES_KW || (key->key_len != 16 && key->key_len != 24 && key->key_len != 32))
    return crypto_dom_exception_error(js, "AES-KW requires a 128, 192, or 256-bit key", "InvalidAccessError");

  mbedtls_nist_kw_context ctx;
  mbedtls_nist_kw_init(&ctx);
  int rc = mbedtls_nist_kw_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key->key,
                                  (unsigned)(key->key_len * 8), wrap ? 1 : 0);
  size_t capacity = wrap ? input_len + 8 : input_len > 8 ? input_len - 8 : 0;
  uint8_t *out = malloc(capacity ? capacity : 1);
  if (rc == 0 && out) {
    rc = wrap
      ? mbedtls_nist_kw_wrap(&ctx, MBEDTLS_KW_MODE_KW, input, input_len, out, output_len, capacity)
      : mbedtls_nist_kw_unwrap(&ctx, MBEDTLS_KW_MODE_KW, input, input_len, out, output_len, capacity);
  } else rc = -1;
  mbedtls_nist_kw_free(&ctx);
  if (rc != 0) { free(out); return js_mkerr(js, "AES-KW operation failed"); }
  *output = out;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_ecdh_derive_bits(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm
) {
  if (key->kind != CRYPTO_KEY_ECDH || key->key_type != CRYPTO_KEY_TYPE_PRIVATE)
    return crypto_dom_exception_error(js, "ECDH requires a private key", "InvalidAccessError");

  const crypto_curve_info_t *curve = crypto_curve_by_name(key->curve, strlen(key->curve));
  if (!curve) return js_mkerr(js, "Unsupported EC curve");
  ant_value_t peer_val = is_object_type(algorithm) ? js_get(js, algorithm, "public") : Ant_Value_Undefined;
  if (is_err(peer_val)) return peer_val;

  mbedtls_ecp_point peer;
  mbedtls_ecp_point_init(&peer);
  ant_crypto_key_t *peer_key = crypto_require_key(peer_val);
  int rc = -1;
  if (peer_key && peer_key->kind == CRYPTO_KEY_ECDH && peer_key->pkey) {
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*(mbedtls_pk_context *)peer_key->pkey);
    if (ec->grp.id == ((mbedtls_ecp_keypair *)mbedtls_pk_ec(
          *(mbedtls_pk_context *)key->pkey))->grp.id)
      rc = mbedtls_ecp_copy(&peer, &ec->Q);
  } else {
    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (buffer_source_get_bytes(js, peer_val, &bytes, &len)) {
      mbedtls_ecp_keypair *local = mbedtls_pk_ec(*(mbedtls_pk_context *)key->pkey);
      rc = mbedtls_ecp_point_read_binary(&local->grp, &peer, bytes, len);
    }
  }
  if (rc != 0) {
    mbedtls_ecp_point_free(&peer);
    return crypto_dom_exception_error(js, "Invalid ECDH public key", "DataError");
  }

  mbedtls_ecp_keypair *local = mbedtls_pk_ec(*(mbedtls_pk_context *)key->pkey);
  mbedtls_mpi shared;
  mbedtls_mpi_init(&shared);
  uint8_t *out = malloc(curve->coords);
  if (!out) { mbedtls_mpi_free(&shared); mbedtls_ecp_point_free(&peer); return Ant_Error_CreateOutOfMemory(js); }
  rc = mbedtls_ecdh_compute_shared(&local->grp, &shared, &peer, &local->d,
                                   crypto_mbedtls_rng, NULL);
  if (rc == 0) rc = mbedtls_mpi_write_binary(&shared, out, curve->coords);
  mbedtls_mpi_free(&shared);
  mbedtls_ecp_point_free(&peer);
  if (rc != 0) { free(out); return js_mkerr(js, "ECDH failed"); }
  ant_value_t result = create_arraybuffer_copy(js, out, curve->coords);
  free(out);
  return result;
}

static uint32_t crypto_read_number_member(ant_t *js, ant_value_t object,
                                          const char *name, uint32_t fallback) {
  if (!is_object_type(object)) return fallback;
  ant_value_t value = js_get(js, object, name);
  if (is_err(value) || vtype(value) != kTypeNumber) return fallback;
  double number = Ant_Value_AsNumberUnchecked(value);
  if (!isfinite(number) || number < 0 || number > UINT32_MAX || floor(number) != number)
    return UINT32_MAX;
  return (uint32_t)number;
}

static uint32_t crypto_read_public_exponent(ant_t *js, ant_value_t algorithm) {
  ant_value_t value = is_object_type(algorithm)
    ? js_get(js, algorithm, "publicExponent") : Ant_Value_Undefined;
  if (is_err(value) || vtype(value) == kTypeUndefined) return 65537;
  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (!buffer_source_get_bytes(js, value, &bytes, &len) || len == 0 || len > 4) return 0;
  uint32_t exponent = 0;
  for (size_t i = 0; i < len; i++) exponent = (exponent << 8) | bytes[i];
  return exponent;
}

static ant_value_t crypto_generate_asym_key(
  ant_t *js, ant_value_t algorithm, ant_value_t extractable_value, ant_value_t usages_value
) {
  ant_value_t name_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_val)) return name_val;
  size_t name_len = 0;
  const char *name = js_getstr(js, name_val, &name_len);
  ant_crypto_key_kind_t kind = crypto_asym_kind_from_name(name, name_len);
  if (!kind)
    return crypto_dom_exception_error(js, "Unsupported generateKey algorithm", "NotSupportedError");

  uint8_t usages = 0;
  ant_value_t result = crypto_read_key_usages(js, usages_value, &usages);
  if (is_err(result)) return result;
  bool extractable = js_truthy(js, extractable_value);

  const EVP_MD *md = crypto_algorithm_hash(js, algorithm);
  if ((kind == CRYPTO_KEY_RSA_PSS || kind == CRYPTO_KEY_RSA_OAEP) && !md)
    return crypto_dom_exception_error(js, "Algorithm.hash is required", "NotSupportedError");

  if (kind == CRYPTO_KEY_AES_KW) {
    result = crypto_check_asym_usages(js, kind, CRYPTO_KEY_TYPE_SECRET, usages);
    if (is_err(result)) return result;
    uint32_t bits = crypto_read_number_member(js, algorithm, "length", 128);
    if (bits == UINT32_MAX || (bits != 128 && bits != 192 && bits != 256))
      return crypto_dom_exception_error(js, "AES-KW key length must be 128, 192, or 256", "OperationError");
    ant_crypto_key_t *key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_SECRET);
    if (!key) return Ant_Error_CreateOutOfMemory(js);
    key->key_len = bits / 8u;
    key->usages = usages;
    key->extractable = extractable;
    if (crypto_fill_random(key->key, key->key_len) != 0) {
      crypto_key_free(key);
      return js_mkerr(js, "Key generation failed");
    }
    return crypto_make_asym_key_object(js, key, bits);
  }

  uint8_t private_usages = 0;
  uint8_t public_usages = 0;
  uint8_t allowed_usages = 0;
  switch (kind) {
    case CRYPTO_KEY_RSA_PKCS1:
    case CRYPTO_KEY_RSA_PSS:
    case CRYPTO_KEY_ECDSA:
      allowed_usages = CRYPTO_KEY_USAGE_SIGN | CRYPTO_KEY_USAGE_VERIFY;
      private_usages = usages & CRYPTO_KEY_USAGE_SIGN;
      public_usages = usages & CRYPTO_KEY_USAGE_VERIFY;
      break;
    case CRYPTO_KEY_RSA_OAEP:
      allowed_usages = CRYPTO_KEY_USAGE_ENCRYPT | CRYPTO_KEY_USAGE_DECRYPT |
        CRYPTO_KEY_USAGE_WRAP_KEY | CRYPTO_KEY_USAGE_UNWRAP_KEY;
      private_usages = usages & (CRYPTO_KEY_USAGE_DECRYPT | CRYPTO_KEY_USAGE_UNWRAP_KEY);
      public_usages = usages & (CRYPTO_KEY_USAGE_ENCRYPT | CRYPTO_KEY_USAGE_WRAP_KEY);
      break;
    case CRYPTO_KEY_ECDH:
      allowed_usages = CRYPTO_KEY_USAGE_DERIVE_KEY | CRYPTO_KEY_USAGE_DERIVE_BITS;
      private_usages = usages & allowed_usages;
      break;
    default:
      break;
  }
  if (usages & ~allowed_usages)
    return crypto_dom_exception_error(js, "Unsupported key usage for algorithm", "SyntaxError");

  if (kind == CRYPTO_KEY_ECDSA || kind == CRYPTO_KEY_ECDH) {
    ant_value_t curve_val = is_object_type(algorithm)
      ? js_get(js, algorithm, "namedCurve") : Ant_Value_Undefined;
    if (is_err(curve_val)) return curve_val;
    size_t curve_len = 0;
    const char *curve_name = vtype(curve_val) == kTypeString
      ? js_getstr(js, curve_val, &curve_len) : NULL;
    const crypto_curve_info_t *curve = crypto_curve_by_name(curve_name, curve_len);
    if (!curve)
      return crypto_dom_exception_error(js, "Unsupported namedCurve", "NotSupportedError");

    ant_crypto_key_t *private_key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_PRIVATE);
    ant_crypto_key_t *public_key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_PUBLIC);
    if (!private_key || !public_key) {
      crypto_key_free(private_key); crypto_key_free(public_key);
      return Ant_Error_CreateOutOfMemory(js);
    }
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*(mbedtls_pk_context *)private_key->pkey);
    if (mbedtls_ecp_group_load(&ec->grp, curve->id) != 0 ||
        mbedtls_ecp_gen_keypair(&ec->grp, &ec->d, &ec->Q, crypto_mbedtls_rng, NULL) != 0) {
      crypto_key_free(private_key); crypto_key_free(public_key);
      return js_mkerr(js, "EC key generation failed");
    }
    mbedtls_ecp_keypair *pub = mbedtls_pk_ec(*(mbedtls_pk_context *)public_key->pkey);
    if (mbedtls_ecp_group_load(&pub->grp, curve->id) != 0 ||
        mbedtls_ecp_copy(&pub->Q, &ec->Q) != 0) {
      crypto_key_free(private_key); crypto_key_free(public_key);
      return js_mkerr(js, "EC key generation failed");
    }
    private_key->usages = private_usages;
    private_key->extractable = extractable;
    private_key->md = md;
    private_key->bits = (uint32_t)(curve->coords * 8u);
    memcpy(private_key->curve, curve->name, strlen(curve->name) + 1);
    public_key->usages = public_usages;
    public_key->extractable = extractable;
    public_key->md = md;
    public_key->bits = private_key->bits;
    memcpy(public_key->curve, curve->name, strlen(curve->name) + 1);
    return crypto_make_key_pair(js, private_key, private_key->bits, public_key, public_key->bits);
  }

  uint32_t bits = crypto_read_number_member(js, algorithm, "modulusLength", 2048);
  uint32_t exponent = crypto_read_public_exponent(js, algorithm);
  if (bits == UINT32_MAX || bits < 512 || bits > 16384 || exponent < 3 || exponent > UINT32_MAX) {
    return (bits == UINT32_MAX || exponent == 0)
      ? js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA key parameters")
      : crypto_dom_exception_error(js, "Invalid RSA key parameters", "OperationError");
  }

  ant_crypto_key_t *private_key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_PRIVATE);
  ant_crypto_key_t *public_key = crypto_key_alloc_asym(kind, CRYPTO_KEY_TYPE_PUBLIC);
  if (!private_key || !public_key) {
    crypto_key_free(private_key); crypto_key_free(public_key);
    return Ant_Error_CreateOutOfMemory(js);
  }
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*(mbedtls_pk_context *)private_key->pkey);
  if (mbedtls_rsa_gen_key(rsa, crypto_mbedtls_rng, NULL, (unsigned)bits, (int)exponent) != 0) {
    crypto_key_free(private_key); crypto_key_free(public_key);
    return js_mkerr(js, "RSA key generation failed");
  }
  mbedtls_rsa_context *pub = mbedtls_pk_rsa(*(mbedtls_pk_context *)public_key->pkey);
  mbedtls_mpi n, e;
  mbedtls_mpi_init(&n); mbedtls_mpi_init(&e);
  int rc = mbedtls_rsa_export(rsa, &n, NULL, NULL, NULL, &e);
  if (rc == 0) rc = mbedtls_rsa_import(pub, &n, NULL, NULL, NULL, &e);
  if (rc == 0) rc = mbedtls_rsa_complete(pub);
  mbedtls_mpi_free(&n); mbedtls_mpi_free(&e);
  if (rc != 0) {
    crypto_key_free(private_key); crypto_key_free(public_key);
    return js_mkerr(js, "RSA public key derivation failed");
  }
  mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
  mbedtls_rsa_set_padding(pub, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);

  private_key->usages = private_usages;
  private_key->extractable = extractable;
  private_key->md = md;
  private_key->bits = bits;
  public_key->usages = public_usages;
  public_key->extractable = extractable;
  public_key->md = md;
  public_key->bits = bits;
  if (kind == CRYPTO_KEY_RSA_PSS) {
    uint32_t salt = crypto_pss_salt_length(js, algorithm, md);
    if (salt == UINT32_MAX) {
      crypto_key_free(private_key); crypto_key_free(public_key);
      return js_mkerr_typed(js, JS_ERR_RANGE, "Invalid RSA-PSS saltLength");
    }
    private_key->salt_len = salt;
    public_key->salt_len = salt;
  }
  return crypto_make_key_pair(js, private_key, bits, public_key, bits);
}

static ant_value_t crypto_export_serialized_key(ant_t *js, ant_value_t format,
                                                ant_value_t key_value, uint8_t **out, size_t *out_len) {
  *out = NULL;
  *out_len = 0;
  ant_value_t format_string = crypto_webcrypto_string(js, format);
  if (is_err(format_string)) return format_string;
  ant_value_t args[2] = { format_string, key_value };
  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, format_string);
  GC_ROOT_PIN(js, key_value);
  ant_value_t exported = crypto_subtle_export_key_impl(
    js, args, 2, Ant_Value_Undefined, Ant_Value_Undefined, Ant_Value_Undefined
  );
  if (is_err(exported)) { GC_ROOT_RESTORE(js, mark); return exported; }
  GC_ROOT_PIN(js, exported);

  size_t len = 0;
  const char *name = js_getstr(js, format_string, &len);
  if (name && len == 3 && memcmp(name, "jwk", 3) == 0) {
    const char *text = NULL;
    size_t text_len = 0;
    ant_value_t error = crypto_jwk_to_text(js, exported, &text, &text_len);
    if (is_err(error)) { GC_ROOT_RESTORE(js, mark); return error; }
    *out = malloc(text_len ? text_len : 1);
    if (!*out) { GC_ROOT_RESTORE(js, mark); return Ant_Error_CreateOutOfMemory(js); }
    memcpy(*out, text, text_len);
    *out_len = text_len;
  } else {
    const uint8_t *bytes = NULL;
    if (!buffer_source_get_bytes(js, exported, &bytes, out_len)) {
      GC_ROOT_RESTORE(js, mark);
      return js_mkerr_typed(js, JS_ERR_TYPE, "Key serialization is not a BufferSource");
    }
    *out = malloc(*out_len ? *out_len : 1);
    if (!*out) { GC_ROOT_RESTORE(js, mark); return Ant_Error_CreateOutOfMemory(js); }
    memcpy(*out, bytes, *out_len);
  }
  GC_ROOT_RESTORE(js, mark);
  return Ant_Value_Undefined;
}

static ant_value_t crypto_wrap_key_impl(ant_params_t) {
  if (nargs < 4)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.wrapKey requires format, key, wrappingKey, and wrapAlgorithm");
  ant_crypto_key_t *key = crypto_require_key(args[1]);
  if (!key) return js_mkerr_typed(js, JS_ERR_TYPE, "wrapKey requires a CryptoKey");
  if (!key->extractable)
    return crypto_dom_exception_error(js, "CryptoKey is not extractable", "InvalidAccessError");

  uint8_t *serialized = NULL;
  size_t serialized_len = 0;
  ant_value_t result = crypto_export_serialized_key(js, args[0], args[1], &serialized, &serialized_len);
  if (is_err(result)) return result;

  ant_crypto_key_t *wrapping_key = crypto_require_key(args[2]);
  if (!wrapping_key)
    { free(serialized); return js_mkerr_typed(js, JS_ERR_TYPE, "wrappingKey must be a CryptoKey"); }

  uint8_t *wrapped = NULL;
  size_t wrapped_len = 0;
  if (wrapping_key->kind == CRYPTO_KEY_AES_KW) {
    if (!(wrapping_key->usages & CRYPTO_KEY_USAGE_WRAP_KEY))
      { free(serialized); return crypto_dom_exception_error(js, "CryptoKey does not permit wrapKey", "InvalidAccessError"); }
    result = crypto_nist_kw(js, true, wrapping_key, serialized, serialized_len, &wrapped, &wrapped_len);
  } else if (wrapping_key->kind == CRYPTO_KEY_RSA_OAEP) {
    if (!(wrapping_key->usages & CRYPTO_KEY_USAGE_WRAP_KEY))
      { free(serialized); return crypto_dom_exception_error(js, "CryptoKey does not permit wrapKey", "InvalidAccessError"); }
    ant_value_t encrypted = crypto_asym_encrypt(js, true, wrapping_key, args[3],
      crypto_make_buffer(js, serialized, serialized_len));
    if (!is_err(encrypted)) {
      const uint8_t *bytes = NULL;
      if (buffer_source_get_bytes(js, encrypted, &bytes, &wrapped_len)) {
        wrapped = malloc(wrapped_len ? wrapped_len : 1);
        if (wrapped) memcpy(wrapped, bytes, wrapped_len);
        else result = Ant_Error_CreateOutOfMemory(js);
      } else result = js_mkerr(js, "RSA-OAEP wrap failed");
    } else result = encrypted;
  } else {
    result = crypto_dom_exception_error(js, "Unsupported wrapKey algorithm", "NotSupportedError");
  }
  free(serialized);
  if (is_err(result)) { free(wrapped); return result; }
  result = create_arraybuffer_copy(js, wrapped, wrapped_len);
  free(wrapped);
  return result;
}

static ant_value_t crypto_unwrap_key_impl(ant_params_t) {
  if (nargs < 7)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.unwrapKey requires format, wrappedKey, unwrappingKey, unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, and keyUsages");
  const uint8_t *wrapped = NULL;
  size_t wrapped_len = 0;
  if (!buffer_source_get_bytes(js, args[1], &wrapped, &wrapped_len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "wrappedKey must be a BufferSource");

  ant_crypto_key_t *unwrapping_key = crypto_require_key(args[2]);
  if (!unwrapping_key)
    return js_mkerr_typed(js, JS_ERR_TYPE, "unwrappingKey must be a CryptoKey");
  if (!(unwrapping_key->usages & CRYPTO_KEY_USAGE_UNWRAP_KEY))
    return crypto_dom_exception_error(js, "CryptoKey does not permit unwrapKey", "InvalidAccessError");

  uint8_t *plaintext = NULL;
  size_t plaintext_len = 0;
  ant_value_t result = Ant_Value_Undefined;
  if (unwrapping_key->kind == CRYPTO_KEY_AES_KW) {
    result = crypto_nist_kw(js, false, unwrapping_key, wrapped, wrapped_len, &plaintext, &plaintext_len);
  } else if (unwrapping_key->kind == CRYPTO_KEY_RSA_OAEP) {
    ant_value_t decrypted = crypto_asym_encrypt(js, false, unwrapping_key, args[3], args[1]);
    if (!is_err(decrypted)) {
      const uint8_t *bytes = NULL;
      if (buffer_source_get_bytes(js, decrypted, &bytes, &plaintext_len)) {
        plaintext = malloc(plaintext_len ? plaintext_len : 1);
        if (plaintext) memcpy(plaintext, bytes, plaintext_len);
        else result = Ant_Error_CreateOutOfMemory(js);
      } else result = js_mkerr(js, "RSA-OAEP unwrap failed");
    } else result = decrypted;
  } else {
    result = crypto_dom_exception_error(js, "Unsupported unwrapKey algorithm", "NotSupportedError");
  }
  if (is_err(result)) { free(plaintext); return result; }

  ant_value_t format = crypto_webcrypto_string(js, args[0]);
  if (is_err(format)) { free(plaintext); return format; }
  size_t format_len = 0;
  const char *format_name = js_getstr(js, format, &format_len);

  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, format);
  GC_ROOT_PIN(js, args[4]);
  GC_ROOT_PIN(js, args[5]);
  GC_ROOT_PIN(js, args[6]);
  if (format_name && format_len == 3 && memcmp(format_name, "jwk", 3) == 0) {
    ant_value_t jwk = crypto_jwk_from_bytes(js, plaintext, plaintext_len);
    if (is_err(jwk)) result = jwk;
    else result = crypto_import_jwk_key(js, jwk, args[4], args[5], args[6]);
  } else {
    ant_value_t data = create_arraybuffer_copy(js, plaintext, plaintext_len);
    if (is_err(data)) result = data;
    else {
      GC_ROOT_PIN(js, data);
      ant_value_t import_args[5] = { format, data, args[4], args[5], args[6] };
      result = crypto_subtle_import_key_impl(
        js, import_args, 5, Ant_Value_Undefined, Ant_Value_Undefined, Ant_Value_Undefined
      );
    }
  }
  free(plaintext);
  GC_ROOT_RESTORE(js, mark);
  return result;
}

static ant_value_t crypto_derive_key_impl(ant_params_t) {
  if (nargs < 5)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.deriveKey requires algorithm, baseKey, derivedKeyAlgorithm, extractable, and keyUsages");
  uint32_t bits = crypto_read_number_member(js, args[2], "length", 0);
  if (!bits || bits == UINT32_MAX || bits % 8u != 0)
    return js_mkerr_typed(js, JS_ERR_TYPE, "derivedKeyAlgorithm.length must be a positive multiple of 8");

  uint8_t *derived = NULL;
  size_t derived_len = 0;
  ant_value_t result = Ant_Value_Undefined;
  if (crypto_algorithm_is(js, args[0], "PBKDF2")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    ant_value_t bits_value = Ant_Value_FromNumber((double)bits);
    ant_value_t derive_args[3] = { args[0], args[1], bits_value };
    GC_ROOT_SAVE(mark, js);
    GC_ROOT_PIN(js, bits_value);
    ant_value_t bits_result = crypto_subtle_derive_bits_impl(
      js, derive_args, 3, Ant_Value_Undefined, Ant_Value_Undefined, Ant_Value_Undefined
    );
    GC_ROOT_RESTORE(js, mark);
    if (is_err(bits_result)) return bits_result;
    const uint8_t *bytes = NULL;
    if (!buffer_source_get_bytes(js, bits_result, &bytes, &derived_len))
      return js_mkerr(js, "PBKDF2 deriveKey failed");
    derived = malloc(derived_len ? derived_len : 1);
    if (!derived) return Ant_Error_CreateOutOfMemory(js);
    memcpy(derived, bytes, derived_len);
  } else if (crypto_algorithm_is(js, args[0], "ECDH")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    ant_value_t bits_result = crypto_ecdh_derive_bits(js, crypto_require_key(args[1]), args[0]);
    if (is_err(bits_result)) return bits_result;
    const uint8_t *bytes = NULL;
    if (!buffer_source_get_bytes(js, bits_result, &bytes, &derived_len))
      return js_mkerr(js, "ECDH deriveKey failed");
    derived = malloc(derived_len ? derived_len : 1);
    if (!derived) return Ant_Error_CreateOutOfMemory(js);
    memcpy(derived, bytes, derived_len);
  } else {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported deriveKey algorithm", "NotSupportedError");
  }

  result = crypto_import_raw_key(js, derived, derived_len, args[2], args[3], args[4]);
  free(derived);
  return result;
}

#else /* ANT_MBEDTLS_TLS_BACKEND */
#include <compat.h> // IWYU pragma: keep
#include <uv.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/rand.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#include <uuidv7.h>
#pragma GCC diagnostic pop

#include "ant.h"
#include "cfunc.h"
#include "internal.h"
#include "list.h"
#include "ascii.h"
#include "ptr.h"
#include "gc.h"
#include "utf8.h"
#include "base64.h"
#include "errors.h"
#include "gc/modules.h"
#include "gc/roots.h"
#include "gc/uv.h"
#include "silver/call.h"
#include "sync.h"
#include "gc/strings.h"
#include "modules/crypto.h"
#include "modules/buffer.h"
#include "modules/domexception.h"
#include "modules/microtasks.h"
#include "modules/process.h"

static constexpr double CRYPTO_MAX_SAFE_INTEGER = 9007199254740991.0;
static constexpr double CRYPTO_RANDOM_FILL_MAX_LENGTH = 2147483647.0;
static constexpr double CRYPTO_RANDOM_INT_MAX_RANGE = 281474976710655.0;
static constexpr uint64_t CRYPTO_RANDOM_INT_MASK = 281474976710655ULL;
static constexpr size_t CRYPTO_RANDOM_INT_CACHE_WORDS = 64;

struct crypto_random_job_s {
  crypto_random_job_t *next;
  crypto_random_job_t **pprev;
  uv_work_t work;
  ant_t *js;
  ant_value_t target;
  ant_value_t callback;
  ArrayBufferData *storage;
  uint8_t *bytes;
  size_t size;
  bool failed;
};

static _Thread_local struct {
  uint64_t words[CRYPTO_RANDOM_INT_CACHE_WORDS];
  size_t remaining;
} crypto_random_int_cache;

typedef enum {
  CRYPTO_TEXT_UTF8 = 0,
  CRYPTO_TEXT_HEX,
  CRYPTO_TEXT_BASE64,
  CRYPTO_TEXT_LATIN1
} crypto_text_encoding_t;

typedef struct {
  EVP_MD_CTX *ctx;
  const EVP_MD *md;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  bool finalized;
} ant_hash_state_t;

typedef struct {
  HMAC_CTX *ctx;
  const EVP_MD *md;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  bool finalized;
} ant_hmac_state_t;

typedef enum {
  CRYPTO_KEY_HMAC = 1,
  CRYPTO_KEY_AES_GCM,
  CRYPTO_KEY_PBKDF2,
  CRYPTO_KEY_AES_KW,
  CRYPTO_KEY_RSA_PKCS1,
  CRYPTO_KEY_RSA_PSS,
  CRYPTO_KEY_RSA_OAEP,
  CRYPTO_KEY_ECDSA,
  CRYPTO_KEY_ECDH,
} ant_crypto_key_kind_t;

typedef enum {
  CRYPTO_KEY_TYPE_SECRET = 0,
  CRYPTO_KEY_TYPE_PUBLIC,
  CRYPTO_KEY_TYPE_PRIVATE,
} ant_crypto_key_type_t;

typedef enum {
  CRYPTO_KEY_USAGE_ENCRYPT     = 1u << 0,
  CRYPTO_KEY_USAGE_DECRYPT     = 1u << 1,
  CRYPTO_KEY_USAGE_SIGN        = 1u << 2,
  CRYPTO_KEY_USAGE_VERIFY      = 1u << 3,
  CRYPTO_KEY_USAGE_DERIVE_KEY  = 1u << 4,
  CRYPTO_KEY_USAGE_DERIVE_BITS = 1u << 5,
  CRYPTO_KEY_USAGE_WRAP_KEY    = 1u << 6,
  CRYPTO_KEY_USAGE_UNWRAP_KEY  = 1u << 7,
} ant_crypto_key_usage_t;

typedef struct {
  ant_crypto_key_kind_t kind;
  const EVP_MD *md;
  uint8_t *key;
  size_t key_len;
  uint8_t usages;
  uint8_t key_type;
  bool extractable;
  void *pkey;          /* mbedTLS mbedtls_pk_context* for asymmetric keys. */
  uint32_t bits;       /* RSA modulus length or EC curve size. */
  uint32_t salt_len;   /* RSA-PSS salt length in bytes. */
  char curve[16];      /* Named curve for EC keys. */
} ant_crypto_key_t;

#ifdef ANT_MBEDTLS_TLS_BACKEND
/* Asymmetric WebCrypto helpers are defined near the end of this backend so the
 * shared secret-key logic above can stay compact. */
static ant_crypto_key_kind_t crypto_asym_kind_from_name(const char *name, size_t len);
static bool crypto_is_asym_kind(ant_crypto_key_kind_t kind);
static ant_value_t crypto_check_asym_usages(
  ant_t *js, ant_crypto_key_kind_t kind, uint8_t key_type, uint8_t usages
);
static ant_value_t crypto_make_key_pair(
  ant_t *js, ant_crypto_key_t *private_key, uint32_t private_bits,
  ant_crypto_key_t *public_key, uint32_t public_bits
);
static ant_value_t crypto_make_asym_key_object(
  ant_t *js, ant_crypto_key_t *key, uint32_t bits
);
static ant_value_t crypto_import_raw_key(
  ant_t *js, const uint8_t *bytes, size_t len, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_import_jwk_key(
  ant_t *js, ant_value_t jwk, ant_value_t algorithm,
  ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_import_der_key(
  ant_t *js, bool private_key, const uint8_t *bytes, size_t len,
  ant_value_t algorithm, ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_export_jwk_key(ant_t *js, ant_crypto_key_t *key);
static ant_value_t crypto_export_der_key(ant_t *js, bool private_key, ant_crypto_key_t *key);
static ant_value_t crypto_asym_sign(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm, ant_value_t data
);
static ant_value_t crypto_asym_verify(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm,
  ant_value_t signature, ant_value_t data
);
static ant_value_t crypto_asym_encrypt(
  ant_t *js, bool encrypt, ant_crypto_key_t *key,
  ant_value_t algorithm, ant_value_t data
);
static ant_value_t crypto_nist_kw(
  ant_t *js, bool wrap, ant_crypto_key_t *key,
  const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len
);
static ant_value_t crypto_ecdh_derive_bits(
  ant_t *js, ant_crypto_key_t *key, ant_value_t algorithm
);
static ant_value_t crypto_generate_asym_key(
  ant_t *js, ant_value_t algorithm, ant_value_t extractable, ant_value_t usages
);
static ant_value_t crypto_wrap_key_impl(ant_params_t);
static ant_value_t crypto_unwrap_key_impl(ant_params_t);
static ant_value_t crypto_derive_key_impl(ant_params_t);
#endif

typedef struct {
  EVP_CIPHER_CTX *ctx;
  bool encrypt;
  bool finalized;
  bool is_gcm;
  uint8_t auth_tag[16];
  size_t auth_tag_len;
} ant_cipher_state_t;

static constexpr uint32_t CRYPTO_HASH_NATIVE_TAG   = Ant_NativeTag("HASH");
static constexpr uint32_t CRYPTO_HMAC_NATIVE_TAG   = Ant_NativeTag("HMAC");
static constexpr uint32_t CRYPTO_KEY_NATIVE_TAG    = Ant_NativeTag("CKEY");
static constexpr uint32_t CRYPTO_CIPHER_NATIVE_TAG = Ant_NativeTag("CIPH");

static const char *const k_crypto_cipher_names[] = {
  "aes-128-cbc",
  "aes-128-ctr",
  "aes-128-ecb",
  "aes-128-gcm",
  "aes-128-ofb",
  "aes-192-cbc",
  "aes-192-ctr",
  "aes-192-ecb",
  "aes-192-gcm",
  "aes-192-ofb",
  "aes-256-cbc",
  "aes-256-ctr",
  "aes-256-ecb",
  "aes-256-gcm",
  "aes-256-ofb",
  "des-cbc",
  "des-ecb",
  "des-ede",
  "des-ede-cbc",
  "des-ede3-cbc",
  "rc2-cbc",
  "rc4",
};

static const char *const k_crypto_hash_names[] = {
  "DSA-SHA",
  "DSA-SHA1",
  "MD4",
  "MD5",
  "MD5-SHA1",
  "RSA-MD5",
  "RSA-SHA1",
  "RSA-SHA224",
  "RSA-SHA256",
  "RSA-SHA384",
  "RSA-SHA512",
  "dsaWithSHA",
  "dsaWithSHA1",
  "ecdsa-with-SHA1",
  "md4",
  "md5",
  "md5-sha1",
  "md5WithRSAEncryption",
  "sha1",
  "sha1WithRSAEncryption",
  "sha224",
  "sha224WithRSAEncryption",
  "sha256",
  "sha256WithRSAEncryption",
  "sha384",
  "sha384WithRSAEncryption",
  "sha512",
  "sha512-256",
  "sha512WithRSAEncryption",
};

static const char *const k_crypto_curve_names[] = {
  "prime256v1",
  "secp224r1",
  "secp384r1",
  "secp521r1",
};

typedef enum {
  CRYPTO_NAME_CIPHER = 0,
  CRYPTO_NAME_HASH,
  CRYPTO_NAME_CURVE
} crypto_name_kind_t;

static ant_value_t crypto_webcrypto_string(ant_t *js, ant_value_t value) {
  if (vtype(value) == kTypeString) return value;
  if (is_object_type(value) || vtype(value) == kTypeBuiltin)
    value = js_to_primitive(js, value, 1);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeSymbol)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Cannot convert a Symbol to a string");
  return js_tostring_val(js, value);
}

static ant_value_t crypto_algorithm_member(ant_t *js, ant_value_t object, const char *interned_name) {
  if (!interned_name) return js_mkerr(js, "oom");
  if (vtype(object) != kTypeObject) return js_get(js, object, interned_name);

  ant_object_t *obj = js_obj_ptr(object);
  if (obj->flags.is_exotic) return js_get(js, object, interned_name);

  int32_t slot = ant_shape_lookup_interned(obj->shape, interned_name);
  if (slot >= 0) {
    const ant_shape_prop_t *prop = ant_shape_prop_at(obj->shape, (uint32_t)slot);
    if (prop->has_getter || prop->has_setter) return js_get(js, object, interned_name);
    return ant_object_prop_get_unchecked(obj, (uint32_t)slot);
  }

  if (js_get_proto(js, object) != js->sym.object_proto) return js_get(js, object, interned_name);
  ant_object_t *proto = js_obj_ptr(js->sym.object_proto);
  
  if (proto->flags.is_exotic || ant_shape_lookup_interned(proto->shape, interned_name) >= 0 ||
      vtype(js_get_proto(js, js->sym.object_proto)) != kTypeNull)
    return js_get(js, object, interned_name);

  return Ant_Value_Undefined;
}

static ant_value_t crypto_subtle_get_algorithm_name(ant_t *js, ant_value_t algorithm) {
  if (vtype(algorithm) == kTypeString) return algorithm;
  if (is_object_type(algorithm)) {
    ant_value_t name = crypto_algorithm_member(js, algorithm, js->intern.name);
    if (is_err(name)) return name;
    if (vtype(name) == kTypeUndefined)
      return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.name is required");
    return crypto_webcrypto_string(js, name);
  }
  return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm must be a string or object with a name");
}

static void crypto_hash_state_free(ant_hash_state_t *state) {
  if (!state) return;
  if (state->ctx) EVP_MD_CTX_free(state->ctx);
  free(state);
}

static void crypto_hash_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_hash_state_t *state = (ant_hash_state_t *)js_get_native(value, CRYPTO_HASH_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_HASH_NATIVE_TAG);
  crypto_hash_state_free(state);
}

static void crypto_hmac_state_free(ant_hmac_state_t *state) {
  if (!state) return;
  if (state->ctx) HMAC_CTX_free(state->ctx);
  free(state);
}

static void crypto_hmac_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_hmac_state_t *state = (ant_hmac_state_t *)js_get_native(value, CRYPTO_HMAC_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_HMAC_NATIVE_TAG);
  crypto_hmac_state_free(state);
}

static void crypto_key_free(ant_crypto_key_t *key) {
  if (!key) return;
  OPENSSL_cleanse(key->key, key->key_len);
  free(key);
}

static void crypto_key_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_crypto_key_t *key = (ant_crypto_key_t *)js_get_native(value, CRYPTO_KEY_NATIVE_TAG);

  if (!key) return;
  js_clear_native(value, CRYPTO_KEY_NATIVE_TAG);
  crypto_key_free(key);
}

static void crypto_cipher_state_free(ant_cipher_state_t *state) {
  if (!state) return;
  if (state->ctx) EVP_CIPHER_CTX_free(state->ctx);
  free(state);
}

static void crypto_cipher_finalize(ant_t *js, ant_object_t *obj) {
  ant_value_t value = js_obj_from_ptr(obj);
  ant_cipher_state_t *state = (ant_cipher_state_t *)js_get_native(value, CRYPTO_CIPHER_NATIVE_TAG);

  if (!state) return;
  js_clear_native(value, CRYPTO_CIPHER_NATIVE_TAG);
  crypto_cipher_state_free(state);
}

int crypto_fill_random(void *buf, size_t len) {
  if (len == 0) return 0;
  if (len > (size_t)INT_MAX) return -1;
  return RAND_bytes((uint8_t *)buf, (int)len) == 1 ? 0 : -1;
}

static inline ant_value_t crypto_random_error(ant_t *js) {
  return js_mkerr(js, "secure random generation failed");
}

#define UUID_HEX_DIGIT(n) ((char)((n) < 10 ? '0' + (n) : 'a' - 10 + (n)))
#define UUID_HEX2(i) { UUID_HEX_DIGIT((i) >> 4), UUID_HEX_DIGIT((i) & 15) }
#define UUID_HEX16(h) \
  UUID_HEX2((h) * 16 + 0), UUID_HEX2((h) * 16 + 1), UUID_HEX2((h) * 16 + 2), UUID_HEX2((h) * 16 + 3),   \
  UUID_HEX2((h) * 16 + 4), UUID_HEX2((h) * 16 + 5), UUID_HEX2((h) * 16 + 6), UUID_HEX2((h) * 16 + 7),   \
  UUID_HEX2((h) * 16 + 8), UUID_HEX2((h) * 16 + 9), UUID_HEX2((h) * 16 + 10), UUID_HEX2((h) * 16 + 11), \
  UUID_HEX2((h) * 16 + 12), UUID_HEX2((h) * 16 + 13), UUID_HEX2((h) * 16 + 14), UUID_HEX2((h) * 16 + 15)

static const char uuid_hex_lut[256][2] = {
  UUID_HEX16(0), UUID_HEX16(1), UUID_HEX16(2), UUID_HEX16(3),
  UUID_HEX16(4), UUID_HEX16(5), UUID_HEX16(6), UUID_HEX16(7),
  UUID_HEX16(8), UUID_HEX16(9), UUID_HEX16(10), UUID_HEX16(11),
  UUID_HEX16(12), UUID_HEX16(13), UUID_HEX16(14), UUID_HEX16(15),
};

static inline void crypto_format_uuid_v4(const uint8_t uuid[16], char out[37]) {
  const char (*lut)[2] = uuid_hex_lut;

  memcpy(out + 0,  lut[uuid[0]], 2);
  memcpy(out + 2,  lut[uuid[1]], 2);
  memcpy(out + 4,  lut[uuid[2]], 2);
  memcpy(out + 6,  lut[uuid[3]], 2);
  
  out[8] = '-';
  memcpy(out + 9,  lut[uuid[4]], 2);
  memcpy(out + 11, lut[uuid[5]], 2);
  
  out[13] = '-';
  memcpy(out + 14, lut[uuid[6]], 2);
  memcpy(out + 16, lut[uuid[7]], 2);
  
  out[18] = '-';
  memcpy(out + 19, lut[uuid[8]], 2);
  memcpy(out + 21, lut[uuid[9]], 2);
  
  out[23] = '-';
  memcpy(out + 24, lut[uuid[10]], 2);
  memcpy(out + 26, lut[uuid[11]], 2);
  memcpy(out + 28, lut[uuid[12]], 2);
  memcpy(out + 30, lut[uuid[13]], 2);
  memcpy(out + 32, lut[uuid[14]], 2);
  memcpy(out + 34, lut[uuid[15]], 2);
  out[36] = '\0';
}

int crypto_random_uuid(char out[37]) {
  uint8_t uuid[16];
  if (!out || crypto_fill_random(uuid, sizeof(uuid)) < 0) return -1;

  uuid[6] = (uint8_t)((uuid[6] & 0x0f) | 0x40);
  uuid[8] = (uint8_t)((uuid[8] & 0x3f) | 0x80);

  crypto_format_uuid_v4(uuid, out);
  return 0;
}

static ant_value_t crypto_make_buffer(ant_t *js, const uint8_t *data, size_t len) {
  ArrayBufferData *buffer = create_array_buffer_data(js, len);
  if (!buffer) return js_mkerr(js, "Failed to allocate Buffer");
  if (len > 0 && data) memcpy(buffer->data, data, len);
  return create_typed_array(js, TYPED_ARRAY_UINT8, buffer, 0, len, js->builtins.buffer_proto);
}

static bool crypto_get_mutable_bytes(
  ant_value_t value, uint8_t **out, size_t *len, 
  size_t *element_size, ArrayBufferData **storage
) {
  *element_size = 1;

  TypedArrayData *ta = buffer_get_typedarray_data(value);
  if (ta) {
    *element_size = get_element_size(ta->type);
    *storage = ta->buffer;
    Ant_Buffer_GetViewBytes(ta, out, len);
    return true;
  }

  DataViewData *dv = buffer_get_dataview_data(value);
  if (dv) {
    *storage = dv->buffer;
    Ant_Buffer_GetViewBytes(dv, out, len);
    return true;
  }

  ArrayBufferData *ab = buffer_get_arraybuffer_data(value);
  if (!ab) return false;
  
  *storage = ab;
  bool live = Ant_Buffer_IsLive(ab);
  *out = live ? ab->data : NULL;
  *len = live ? ab->length : 0;
  
  return true;
}

static crypto_text_encoding_t crypto_parse_encoding(const char *str, size_t len) {
  if (len == 3 && strncasecmp(str, "hex", 3) == 0) return CRYPTO_TEXT_HEX;
  if (len == 6 && strncasecmp(str, "base64", 6) == 0) return CRYPTO_TEXT_BASE64;
  if (len == 9 && strncasecmp(str, "base64url", 9) == 0) return CRYPTO_TEXT_BASE64;
  
  if (
    (len == 5 && strncasecmp(str, "ascii", 5)  == 0) ||
    (len == 6 && strncasecmp(str, "latin1", 6) == 0) ||
    (len == 6 && strncasecmp(str, "binary", 6) == 0)
  ) return CRYPTO_TEXT_LATIN1;
  
  return CRYPTO_TEXT_UTF8;
}

static uint8_t *crypto_decode_hex(const char *str, size_t len, size_t *out_len) {
  if ((len & 1u) != 0) return NULL;

  uint8_t *buf = malloc(len / 2u);
  if (!buf) return NULL;

  for (size_t i = 0; i < len; i += 2u) {
    int hi = ant_ascii_hex_value((unsigned char)str[i]);
    int lo = ant_ascii_hex_value((unsigned char)str[i + 1u]);
    
    if (hi < 0 || lo < 0) {
      free(buf);
      return NULL;
    }
    
    buf[i / 2u] = (uint8_t)((hi << 4u) | lo);
  }

  *out_len = len / 2u;
  return buf;
}

static ant_value_t crypto_get_input_bytes(
  ant_t *js, ant_value_t value, ant_value_t encoding_val,
  const uint8_t **out_bytes, size_t *out_len, uint8_t **owned
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *buf = NULL;
  
  ant_value_t string_val;
  size_t str_len = 0;
  
  const char *str = NULL;
  crypto_text_encoding_t enc = CRYPTO_TEXT_UTF8;

  if (buffer_source_get_bytes(js, value, &bytes, &len)) {
    *out_bytes = bytes;
    *out_len = len;
    *owned = NULL;
    return Ant_Value_Undefined;
  }

  string_val = js_tostring_val(js, value);
  if (is_err(string_val)) return string_val;

  str = js_getstr(js, string_val, &str_len);
  if (!str) return js_mkerr(js, "Failed to convert hash input to string");

  if (vtype(encoding_val) == kTypeString) {
    size_t enc_len = 0;
    const char *enc_str = js_getstr(js, encoding_val, &enc_len);
    if (enc_str) enc = crypto_parse_encoding(enc_str, enc_len);
  }

  switch (enc) {
    case CRYPTO_TEXT_HEX:
      buf = crypto_decode_hex(str, str_len, &len);
      if (!buf) return js_mkerr(js, "Invalid hex string");
      bytes = buf;
      break;
    case CRYPTO_TEXT_BASE64:
      buf = ant_base64_decode_loose(str, str_len, &len);
      if (!buf) return Ant_Error_CreateOutOfMemory(js);
      bytes = buf;
      break;
    case CRYPTO_TEXT_LATIN1: {
      size_t units = (size_t)str_utf16_len(js, string_val);
      buf = malloc(units == 0 ? 1 : units);
      if (!buf) return Ant_Error_CreateOutOfMemory(js);
      for (size_t i = 0; i < units; i++) buf[i] = (uint8_t)utf16_code_unit_at(str, str_len, i);
      bytes = buf;
      len = units;
      break;
    }
    case CRYPTO_TEXT_UTF8:
    default:
      bytes = (const uint8_t *)str;
      len = str_len;
      break;
  }

  *out_bytes = bytes;
  *out_len = len;
  *owned = buf;
  return Ant_Value_Undefined;
}

static const EVP_MD *crypto_digest_from_name(const char *algo, size_t algo_len) {
  if (!algo || algo_len == 0) return NULL;
  char *algo_name = strndup(algo, algo_len);
  if (!algo_name) return NULL;
  const EVP_MD *md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  return md;
}

static const EVP_MD *crypto_digest_from_algorithm(ant_t *js, ant_value_t algorithm) {
  ant_value_t algo_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(algo_val)) return NULL;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return NULL;

  if (
    (algo_len == 5 && strncasecmp(algo, "SHA-1", 5) == 0) ||
    (algo_len == 4 && strncasecmp(algo, "SHA1", 4) == 0)
  ) return EVP_get_digestbyname("sha1");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-256", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA256", 6) == 0)
  ) return EVP_get_digestbyname("sha256");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-384", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA384", 6) == 0)
  ) return EVP_get_digestbyname("sha384");
  if (
    (algo_len == 7 && strncasecmp(algo, "SHA-512", 7) == 0) ||
    (algo_len == 6 && strncasecmp(algo, "SHA512", 6) == 0)
  ) return EVP_get_digestbyname("sha512");

  return crypto_digest_from_name(algo, algo_len);
}

static unsigned crypto_hash_record_index(const EVP_MD *md) {
  if (!md) return 0;
  switch (EVP_MD_type(md)) {
    case NID_sha1: return 4;
    case NID_sha256: return 5;
    case NID_sha384: return 6;
    case NID_sha512: return 7;
    default: return 0;
  }
}

static ant_value_t crypto_dom_exception_error(ant_t *js, const char *message, const char *name) {
  return js_throw(js, make_dom_exception(js, message, name));
}

static ant_value_t crypto_read_key_length(
  ant_t *js, ant_value_t object, bool required, uint32_t maximum, bool *present, uint32_t *result
) {
  ant_value_t value = crypto_algorithm_member(js, object, js->intern.length);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeUndefined) {
    *present = false;
    if (required) return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length is required");
    return Ant_Value_Undefined;
  }

  if (is_object_type(value) || vtype(value) == kTypeBuiltin)
    value = js_to_primitive(js, value, 2);
  if (is_err(value)) return value;
  if (vtype(value) == kTypeBigInt || vtype(value) == kTypeSymbol)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length must be a number");

  double number = trunc(js_to_number(js, value));
  if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);

  if (!isfinite(number) || number < 0 || number > (double)maximum)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Algorithm.length is outside its accepted range");

  *present = true;
  *result = (uint32_t)number;

  return Ant_Value_Undefined;
}

static ant_value_t crypto_get_hmac_digest(
  ant_t *js, ant_value_t algorithm, const EVP_MD **result
) {
  if (!is_object_type(algorithm))
    return js_mkerr_typed(js, JS_ERR_TYPE, "HMAC algorithm must be an object");

  ant_value_t hash = crypto_algorithm_member(js, algorithm, intern_string(js, "hash", 4));
  if (is_err(hash)) return hash;
  
  if (vtype(hash) == kTypeUndefined)
    return js_mkerr_typed(js, JS_ERR_TYPE, "HMAC algorithm.hash is required");

  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, hash);
  if (is_err(name_value)) return name_value;
  size_t len = 0;
  
  const char *name = js_getstr(js, name_value, &len);
  const EVP_MD *md = NULL;
  
  if (len == 5 && strncasecmp(name, "SHA-1", len) == 0) md = EVP_sha1();
  else if (len == 7 && strncasecmp(name, "SHA-256", len) == 0) md = EVP_sha256();
  else if (len == 7 && strncasecmp(name, "SHA-384", len) == 0) md = EVP_sha384();
  else if (len == 7 && strncasecmp(name, "SHA-512", len) == 0) md = EVP_sha512();
  if (!md) return crypto_dom_exception_error(js, "Unsupported HMAC hash algorithm", "NotSupportedError");

  *result = md;
  return Ant_Value_Undefined;
}

static bool crypto_algorithm_is(ant_t *js, ant_value_t algorithm, const char *expected) {
  ant_value_t name_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_val)) return false;
  size_t len = 0;
  const char *name = js_getstr(js, name_val, &len);
  return name && strlen(expected) == len && strncasecmp(name, expected, len) == 0;
}

static const EVP_CIPHER *crypto_aes_gcm_cipher_for_key(size_t key_len) {
  switch (key_len) {
    case 16: return EVP_aes_128_gcm();
    case 24: return EVP_aes_192_gcm();
    case 32: return EVP_aes_256_gcm();
    default: return NULL;
  }
}

static const EVP_CIPHER *crypto_cipher_from_name(const char *name, size_t len) {
  char *copy = strndup(name, len);
  if (!copy) return NULL;
  const EVP_CIPHER *cipher = EVP_get_cipherbyname(copy);
  free(copy);
  return cipher;
}

static ant_hash_state_t *crypto_get_hash_state(ant_value_t value) {
  ant_hash_state_t *state = (ant_hash_state_t *)js_get_native(value, CRYPTO_HASH_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_hash_state(
  ant_t *js, ant_value_t value, ant_hash_state_t **out_state
) {
  ant_hash_state_t *state = crypto_get_hash_state(value);
  if (!state) return js_mkerr(js, "Invalid Hash state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_digest_result(
  ant_t *js, const uint8_t *digest, size_t digest_len, ant_value_t encoding_val
) {
  if (vtype(encoding_val) != kTypeString) return crypto_make_buffer(js, digest, digest_len);

  size_t enc_len = 0;
  const char *enc = js_getstr(js, encoding_val, &enc_len);
  if (!enc) return crypto_make_buffer(js, digest, digest_len);

  ant_value_t result = Ant_Value_Undefined;
  if (buffer_bytes_to_string(js, digest, digest_len, enc, enc_len, &result)) return result;

  return crypto_make_buffer(js, digest, digest_len);
}

static ant_mutex_t uuidv7_lock = ANT_MUTEX_INIT;
static uint8_t uuidv7_prev[16] = {0};
static uint8_t uuidv7_rand[256] = {0};
static size_t uuidv7_rand_consumed = sizeof(uuidv7_rand);

int uuidv7_new(uint8_t *uuid_out) {
  struct timespec tp;
  
  clock_gettime(CLOCK_REALTIME, &tp);
  uint64_t unix_ts_ms = (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
  
  ant_mutex_lock(&uuidv7_lock);
  if (uuidv7_rand_consumed > sizeof(uuidv7_rand) - 10) {
    if (crypto_fill_random(uuidv7_rand, sizeof(uuidv7_rand)) < 0) {
      ant_mutex_unlock(&uuidv7_lock);
      return -1;
    }
    
    uuidv7_rand_consumed = 0;
  }
  
  int8_t status = uuidv7_generate(uuidv7_prev, unix_ts_ms, &uuidv7_rand[uuidv7_rand_consumed], uuidv7_prev);
  uuidv7_rand_consumed += uuidv7_status_n_rand_consumed(status);
  
  memcpy(uuid_out, uuidv7_prev, 16);
  ant_mutex_unlock(&uuidv7_lock);
  
  return status;
}

// crypto.random()
static ant_value_t js_crypto_random(ant_params_t) {
  unsigned int value = 0;
  if (crypto_fill_random(&value, sizeof(value)) < 0) return crypto_random_error(js);
  return Ant_Value_FromNumber((double)value);
}

// crypto.randomUUID()
static ant_value_t js_crypto_random_uuid(ant_params_t) {
  char uuid[37];
  if (crypto_random_uuid(uuid) < 0) return crypto_random_error(js);
  return js_mkstr(js, uuid, 36);
}

// crypto.randomUUIDv7()
static ant_value_t js_crypto_random_uuidv7(ant_params_t) {
  uint8_t uuid[16];
  char uuid_str[37];
  
  int result = uuidv7_new(uuid);
  if (result < 0) return js_mkerr(js, "UUIDv7 generation failed");
  
  uuidv7_to_string(uuid, uuid_str);
  return js_mkstr(js, uuid_str, strlen(uuid_str));
}

// crypto.getRandomValues(typedArray)
static ant_value_t js_crypto_get_random_values(ant_params_t) {
  if (nargs < 1) return js_mkerr(js, "getRandomValues requires a TypedArray argument");

  TypedArrayData *ta = buffer_get_typedarray_data(args[0]);
  if (!ta) return js_mkerr_typed(js, JS_ERR_TYPE, "getRandomValues requires a TypedArray argument");

  uint8_t *ptr; size_t len;
  Ant_Buffer_GetViewBytes(ta, &ptr, &len);
  
  if (len > 65536) return js_mkerr(js, "TypedArray byte length exceeds 65536");
  if (crypto_fill_random(ptr, len) < 0) return crypto_random_error(js);
  
  return args[0];
}

static inline bool crypto_is_callback(ant_value_t value) {
  uint8_t type = vtype(value);
  return type == kTypeFunction || type == kTypeBuiltin;
}

static inline ant_value_t crypto_invalid_random_target(ant_t *js, ant_value_t value) {
  return Ant_Error_CreateInvalidArgType(js, "buf", "an instance of ArrayBuffer or ArrayBufferView", value);
}

static inline ant_value_t crypto_invalid_callback(ant_t *js, ant_value_t value) {
  return Ant_Error_CreateInvalidArgType(js, "callback", "of type function", value);
}

static ant_value_t crypto_random_parse_size(ant_t *js, ant_value_t size_val, size_t element_size, double *out_size) {
  if (vtype(size_val) != kTypeNumber) return Ant_Error_CreateInvalidArgType(js, "size", "of type number", size_val);
  double size = Ant_Value_AsNumberUnchecked(size_val) * (double)element_size;
  
  if (isnan(size) || size < 0 || size > CRYPTO_RANDOM_FILL_MAX_LENGTH) {
    char range[64];
    snprintf(range, sizeof(range), ">= 0 && <= %.0f", CRYPTO_RANDOM_FILL_MAX_LENGTH);
    return Ant_Error_CreateOutOfRange(js, "size", range, Ant_Value_FromNumber(size));
  }

  *out_size = size;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_random_fill_range(
  ant_t *js, ant_value_t offset_val, ant_value_t size_val,
  size_t length, size_t element_size, size_t *out_offset, size_t *out_size
) {
  char range[64];

  if (vtype(offset_val) != kTypeNumber)
    return Ant_Error_CreateInvalidArgType(js, "offset", "of type number", offset_val);

  double offset = Ant_Value_AsNumberUnchecked(offset_val) * (double)element_size;
  double max_offset = fmin((double)length, CRYPTO_RANDOM_FILL_MAX_LENGTH);
  
  if (isnan(offset) || offset < 0 || offset > max_offset) {
    snprintf(range, sizeof(range), ">= 0 && <= %.0f", max_offset);
    return Ant_Error_CreateOutOfRange(js, "offset", range, Ant_Value_FromNumber(offset));
  }

  *out_offset = (size_t)offset;
  *out_size = length - *out_offset;
  if (vtype(size_val) == kTypeUndefined) return Ant_Value_Undefined;

  double size = 0;
  ant_value_t parsed = crypto_random_parse_size(js, size_val, element_size, &size);
  if (is_err(parsed)) return parsed;

  double end = size + (double)*out_offset;
  if (end > (double)length) {
    snprintf(range, sizeof(range), "<= %zu", length);
    return Ant_Error_CreateOutOfRange(js, "size + offset", range, Ant_Value_FromNumber(end));
  }

  *out_size = (size_t)size;
  return Ant_Value_Undefined;
}

static void crypto_random_work(uv_work_t *work) {
  crypto_random_job_t *job = (crypto_random_job_t *)work->data;
  job->failed = crypto_fill_random(job->bytes, job->size) < 0;
}

static void crypto_random_after_work(uv_work_t *work, int status) {
  GC_UV_CALLBACK();
  crypto_random_job_t *job = (crypto_random_job_t *)work->data;
  
  ant_t *js = job->js;
  ant_value_t target = job->target;
  ant_value_t callback = job->callback;
  bool failed = job->failed || status < 0;

  Ant_List_Remove(job);
  free_array_buffer_data(js, job->storage);
  free(job);
  if (js->destroying) return;

  GC_ROOT_SAVE(mark, js);
  GC_ROOT_PIN(js, target);
  GC_ROOT_PIN(js, callback);

  ant_value_t error = failed ? js_take_thrown(js, crypto_random_error(js)) : Ant_Value_Null;
  GC_ROOT_PIN(js, error);

  ant_value_t cb_args[2] = { error, failed ? Ant_Value_Undefined : target };
  sv_vm_call(js->vm, js, callback, Ant_Value_Undefined, cb_args, 2, NULL, Ant_Value_Undefined);
  process_report_uncaught_exception_if_pending(js);
  process_microtasks(js);

  GC_ROOT_RESTORE(js, mark);
}

static ant_value_t crypto_random_fill_async(
  ant_t *js, ant_value_t target, ArrayBufferData *storage, 
  uint8_t *bytes, size_t size, ant_value_t callback
) {
  if (size == 0) {
    ant_value_t cb_args[2] = { Ant_Value_Null, target };
    ant_value_t cb_result = Ant_Error_CallCallback(js, callback, Ant_Value_Undefined, cb_args, 2);
    return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
  }

  crypto_random_job_t *job = calloc(1, sizeof(*job));
  if (!job) return Ant_Error_CreateOutOfMemory(js);

  ArrayBufferData *root = storage->owner ? storage->owner : storage;
  root->ref_count++;

  job->js = js;
  job->target = target;
  job->callback = callback;
  job->storage = root;
  job->bytes = bytes;
  job->size = size;
  job->work.data = job;
  Ant_List_Insert(&js->crypto_random_jobs, job);

  int rc = uv_queue_work(js->loop, &job->work, crypto_random_work, crypto_random_after_work);
  if (rc < 0) {
    Ant_List_Remove(job);
    free_array_buffer_data(js, root);
    free(job);
    return js_mkerr(js, "%s", uv_strerror(rc));
  }

  return Ant_Value_Undefined;
}

void gc_mark_crypto(ant_t *js, gc_mark_fn mark) {
  for (crypto_random_job_t *job = js->crypto_random_jobs; job; job = job->next) {
    mark(js, job->target);
    mark(js, job->callback);
  }
}

static ant_value_t js_crypto_random_bytes(ant_params_t) {
  ant_value_t size_val = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 1 ? args[1] : Ant_Value_Undefined;

  double requested = 0;
  ant_value_t parsed = crypto_random_parse_size(js, size_val, 1, &requested);
  if (is_err(parsed)) return parsed;

  bool is_sync = vtype(callback) == kTypeUndefined;
  if (!is_sync && !crypto_is_callback(callback)) return crypto_invalid_callback(js, callback);

  size_t size = (size_t)requested;
  ArrayBufferData *buffer = create_array_buffer_data(js, size);
  if (!buffer) return Ant_Error_CreateOutOfMemory(js);

  ant_value_t result = create_typed_array(js, TYPED_ARRAY_UINT8, buffer, 0, size, js->builtins.buffer_proto);
  if (is_err(result)) return result;

  if (!is_sync) return crypto_random_fill_async(js, result, buffer, buffer->data, size, callback);
  if (crypto_fill_random(buffer->data, size) < 0) return crypto_random_error(js);
  return result;
}

static ant_value_t crypto_warn_random_alias(ant_t *js, const char *message) {
  static atomic_bool warned;
  if (atomic_exchange(&warned, true)) return Ant_Value_Undefined;

  ant_value_t process = js_get(js, js_glob(js), "process");
  ant_value_t emit = is_object_type(process) ? js_get(js, process, "emitWarning") : Ant_Value_Undefined;
  if (!crypto_is_callback(emit)) return Ant_Value_Undefined;

  ant_value_t warning_args[3] = {
    js_mkstr(js, message, strlen(message)),
    js_mkstr(js, "DeprecationWarning", 18),
    js_mkstr(js, "DEP0115", 7),
  };

  return sv_vm_call(js->vm, js, emit, process, warning_args, 3, NULL, Ant_Value_Undefined);
}

#define CRYPTO_DEPRECATED_RANDOM_ALIAS(fn, name)                                           \
  static ant_value_t fn(ant_params_t) {                                                    \
    ant_value_t warned = crypto_warn_random_alias(js, "crypto." name " is deprecated.");   \
    if (is_err(warned)) return warned;                                                     \
    return js_crypto_random_bytes(js, args, nargs, call_new_target, call_this, call_func); \
  }

CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_prng, "prng")
CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_pseudo_random_bytes, "pseudoRandomBytes")
CRYPTO_DEPRECATED_RANDOM_ALIAS(js_crypto_deprecated_rng, "rng")

static void crypto_define_random_aliases(ant_t *js, ant_value_t lib) {
  if (process_pending_deprecation()) {
    defmethod(js, lib, "prng", 4, Ant_Function_CreateWithArity(js_crypto_deprecated_prng, 2));
    defmethod(js, lib, "pseudoRandomBytes", 17, Ant_Function_CreateWithArity(js_crypto_deprecated_pseudo_random_bytes, 2));
    defmethod(js, lib, "rng", 3, Ant_Function_CreateWithArity(js_crypto_deprecated_rng, 2));
    return;
  }

  defalias_of(js, lib, "prng", "randomBytes");
  defalias_of(js, lib, "pseudoRandomBytes", "randomBytes");
  defalias_of(js, lib, "rng", "randomBytes");
}

static ant_value_t js_crypto_random_fill_sync(ant_params_t) {
  ant_value_t target = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t offset_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t size_val = nargs > 2 ? args[2] : Ant_Value_Undefined;

  uint8_t *bytes = NULL;
  ArrayBufferData *storage = NULL;
  
  size_t len = 0, element_size = 1, offset = 0, size = 0;
  if (!crypto_get_mutable_bytes(target, &bytes, &len, &element_size, &storage))
    return crypto_invalid_random_target(js, target);

  if (vtype(offset_val) == kTypeUndefined) offset_val = Ant_Value_FromNumber(0);
  ant_value_t range = crypto_random_fill_range(js, offset_val, size_val, len, element_size, &offset, &size);
  if (is_err(range)) return range;

  if (crypto_fill_random(bytes + offset, size) < 0) return crypto_random_error(js);
  return target;
}

static ant_value_t js_crypto_random_fill(ant_params_t) {
  ant_value_t target = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t offset_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t size_val = nargs > 2 ? args[2] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 3 ? args[3] : Ant_Value_Undefined;

  uint8_t *bytes = NULL;
  ArrayBufferData *storage = NULL;
  
  size_t len = 0, element_size = 1, offset = 0, size = 0;
  if (!crypto_get_mutable_bytes(target, &bytes, &len, &element_size, &storage))
    return crypto_invalid_random_target(js, target);

  if (crypto_is_callback(offset_val)) {
    callback = offset_val;
    offset_val = Ant_Value_FromNumber(0);
    size_val = Ant_Value_Undefined;
  } else if (crypto_is_callback(size_val)) {
    callback = size_val;
    size_val = Ant_Value_Undefined;
  } else if (!crypto_is_callback(callback))
    return crypto_invalid_callback(js, callback);

  ant_value_t range = crypto_random_fill_range(js, offset_val, size_val, len, element_size, &offset, &size);
  if (is_err(range)) return range;

  return crypto_random_fill_async(js, target, storage, bytes + offset, size, callback);
}

static inline bool crypto_is_safe_integer(ant_value_t value) {
  if (vtype(value) != kTypeNumber) return false;
  double number = Ant_Value_AsNumberUnchecked(value);
  return isfinite(number) && number == trunc(number) && fabs(number) <= CRYPTO_MAX_SAFE_INTEGER;
}

static inline bool crypto_random_int_sample(uint64_t *out) {
  if (crypto_random_int_cache.remaining == 0) {
    if (crypto_fill_random(crypto_random_int_cache.words, sizeof(crypto_random_int_cache.words)) < 0) return false;
    crypto_random_int_cache.remaining = CRYPTO_RANDOM_INT_CACHE_WORDS;
  }

  *out = crypto_random_int_cache.words[--crypto_random_int_cache.remaining] & CRYPTO_RANDOM_INT_MASK;
  return true;
}

static ant_value_t js_crypto_random_int(ant_params_t) {
  ant_value_t min_val = nargs > 0 ? args[0] : Ant_Value_Undefined;
  ant_value_t max_val = nargs > 1 ? args[1] : Ant_Value_Undefined;
  ant_value_t callback = nargs > 2 ? args[2] : Ant_Value_Undefined;

  bool min_omitted = vtype(max_val) == kTypeUndefined || crypto_is_callback(max_val);
  if (min_omitted) {
    callback = max_val;
    max_val = min_val;
    min_val = Ant_Value_FromNumber(0);
  }

  bool is_sync = vtype(callback) == kTypeUndefined;
  if (!is_sync && !crypto_is_callback(callback)) return crypto_invalid_callback(js, callback);

  if (!crypto_is_safe_integer(min_val)) return Ant_Error_CreateInvalidArgType(js, "min", "a safe integer", min_val);
  if (!crypto_is_safe_integer(max_val)) return Ant_Error_CreateInvalidArgType(js, "max", "a safe integer", max_val);

  double min = Ant_Value_AsNumberUnchecked(min_val);
  double max = Ant_Value_AsNumberUnchecked(max_val);
  char range_text[64];

  if (max <= min) {
    snprintf(range_text, sizeof(range_text), "greater than the value of \"min\" (%.0f)", min == 0 ? 0.0 : min);
    return Ant_Error_CreateOutOfRange(js, "max", range_text, max_val);
  }

  double range_size = max - min;
  if (range_size > CRYPTO_RANDOM_INT_MAX_RANGE) {
    snprintf(range_text, sizeof(range_text), "<= %.0f", CRYPTO_RANDOM_INT_MAX_RANGE);
    return Ant_Error_CreateOutOfRange(js, min_omitted ? "max" : "max - min", range_text, Ant_Value_FromNumber(range_size));
  }

  uint64_t range = (uint64_t)range_size;
  uint64_t limit = CRYPTO_RANDOM_INT_MASK - (CRYPTO_RANDOM_INT_MASK % range);
  uint64_t sample = 0;

  do {
    if (!crypto_random_int_sample(&sample)) return crypto_random_error(js);
  } while (sample >= limit);

  ant_value_t result = Ant_Value_FromNumber((double)(sample % range) + min);
  if (is_sync) return result;

  ant_value_t cb_args[2] = { Ant_Value_Undefined, result };
  queue_next_tick_with_args(js, callback, cb_args, 2);
  return Ant_Value_Undefined;
}

static ant_value_t crypto_make_filtered_name_array(
  ant_t *js, const char *const *names, size_t count, crypto_name_kind_t kind
) {
  ant_value_t result = Ant_Array_Create(js);
  if (is_err(result)) return result;

  GC_ROOT_SAVE(root_mark, js);
  GC_ROOT_PIN(js, result);

  for (size_t i = 0; i < count; i++) {
    const char *name = names[i];
    bool supported = false;
  
  switch (kind) {
    case CRYPTO_NAME_CIPHER:
      supported = EVP_get_cipherbyname(name) != NULL;
      break;
    case CRYPTO_NAME_HASH:
      supported = EVP_get_digestbyname(name) != NULL;
      break;
    case CRYPTO_NAME_CURVE: {
      int nid = OBJ_sn2nid(name);
      if (nid == NID_undef) break;
      EC_GROUP *group = EC_GROUP_new_by_curve_name(nid);
      if (!group) break;
      EC_GROUP_free(group);
      supported = true;
      break;
    }}
    
    if (!supported) continue;
    ant_value_t name_val = js_mkstr(js, name, strlen(name));
    
    if (is_err(name_val)) {
      GC_ROOT_RESTORE(js, root_mark);
      return name_val;
    }
    
    js_arr_push(js, result, name_val);
  }

  GC_ROOT_RESTORE(js, root_mark);
  return result;
}

static ant_value_t js_crypto_get_ciphers(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_cipher_names,
  sizeof(k_crypto_cipher_names) / sizeof(k_crypto_cipher_names[0]),
  CRYPTO_NAME_CIPHER
);}

static ant_value_t js_crypto_get_hashes(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_hash_names,
  sizeof(k_crypto_hash_names) / sizeof(k_crypto_hash_names[0]),
  CRYPTO_NAME_HASH
);}

static ant_value_t js_crypto_get_curves(ant_params_t) {
return crypto_make_filtered_name_array(
  js, k_crypto_curve_names,
  sizeof(k_crypto_curve_names) / sizeof(k_crypto_curve_names[0]),
  CRYPTO_NAME_CURVE
);}

static ant_value_t js_crypto_timing_safe_equal(ant_params_t) {
  const uint8_t *left = NULL;
  const uint8_t *right = NULL;
  
  size_t left_len = 0;
  size_t right_len = 0;

  if (
    nargs < 2 ||
    !buffer_source_get_bytes(js, args[0], &left, &left_len) ||
    !buffer_source_get_bytes(js, args[1], &right, &right_len)
  ) return js_mkerr_typed(
    js, JS_ERR_TYPE,
    "timingSafeEqual arguments must be Buffer, TypedArray, DataView, or ArrayBuffer"
  );

  if (left_len != right_len)
    return js_mkerr_typed(js, JS_ERR_RANGE, "Input buffers must have the same byte length");

  return Ant_Value_FromBool(CRYPTO_memcmp(left, right, left_len) == 0);
}

static ant_value_t crypto_subtle_digest_impl(
  ant_t *js, ant_value_t algorithm, ant_value_t data
) {
  ant_value_t algo_val = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(algo_val)) return algo_val;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid digest algorithm");
  }

  char algo_name[16];
  if (algo_len >= sizeof(algo_name)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");
  }

  for (size_t i = 0; i < algo_len; i++) {
    char ch = algo[i];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    algo_name[i] = ch;
  }
  algo_name[algo_len] = '\0';

  const char *evp_name = NULL;
  if (strcmp(algo_name, "SHA-1") == 0 || strcmp(algo_name, "SHA1") == 0) evp_name = "sha1";
  else if (strcmp(algo_name, "SHA-256") == 0 || strcmp(algo_name, "SHA256") == 0) evp_name = "sha256";
  else if (strcmp(algo_name, "SHA-384") == 0 || strcmp(algo_name, "SHA384") == 0) evp_name = "sha384";
  else if (strcmp(algo_name, "SHA-512") == 0 || strcmp(algo_name, "SHA512") == 0) evp_name = "sha512";
  else return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");

  const EVP_MD *md = EVP_get_digestbyname(evp_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported digest algorithm");

  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (!buffer_source_get_bytes(js, data, &bytes, &len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "digest data must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  if (EVP_Digest(bytes, len, digest, &digest_len, md, NULL) != 1) {
    return js_mkerr(js, "Digest failed");
  }

  return create_arraybuffer_copy(js, digest, digest_len);
}

static ant_value_t crypto_subtle_call(
  ant_native_params_t, ant_value_t call_this, ant_value_t call_func,
  ant_cfunc_t impl
) {
  GC_ROOT_SAVE(root_mark, js);
  ant_value_t promise = js_mkpromise(js);
  if (is_err(promise)) return promise;
  GC_ROOT_PIN(js, promise);
  ant_value_t result = impl(js, args, nargs, Ant_Value_Undefined, call_this, call_func);
  if (is_err(result) || Ant_Exception_Pending(js)) {
    ant_value_t reason = js_take_thrown(js, result);
    js_reject_promise(js, promise, reason);
  } else js_resolve_promise(js, promise, result);
  GC_ROOT_RESTORE(js, root_mark);
  return promise;
}

static ant_value_t crypto_subtle_digest_args(ant_params_t) {
  if (nargs < 2)
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.digest requires algorithm and data");
  return crypto_subtle_digest_impl(js, args[0], args[1]);
}

static ant_value_t js_crypto_subtle_digest(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_digest_args);
}

static const char *const crypto_key_usage_names[] = {
  "encrypt", "decrypt", "sign", "verify",
  "deriveKey", "deriveBits", "wrapKey", "unwrapKey"
};

static uint8_t crypto_key_usage_bit(ant_t *js, ant_value_t string) {
  size_t len = 0;
  const char *name = js_getstr(js, string, &len);
  
  for (unsigned i = 0; i < 8; i++) if (
    strlen(crypto_key_usage_names[i]) == len && 
    memcmp(name, crypto_key_usage_names[i], len) == 0
  ) return (uint8_t)(1u << i);
  
  return 0;
}

static bool crypto_read_key_usage(ant_t *js, ant_value_t value, void *ctx) {
  ant_value_t string = crypto_webcrypto_string(js, value);
  if (is_err(string)) return false;
  uint8_t bit = crypto_key_usage_bit(js, string);
  
  if (bit) {
    *(uint8_t *)ctx |= bit;
    return true;
  }
  
  js_mkerr_typed(js, JS_ERR_TYPE, "Invalid WebCrypto key usage");
  return false;
}

static bool crypto_read_dense_key_usages(ant_t *js, ant_value_t input, uint8_t *mask) {
  if (vtype(input) != kTypeArray) return false;
  ant_object_t *array = js_obj_ptr(input);
  if (!array->flags.fast_array || array->flags.is_exotic || array->flags.may_have_holes ||
      js_get_proto(js, input) != js->sym.array_proto) return false;

  ant_offset_t symbol = (ant_offset_t)vdata(js->sym.iterator_sym);
  if (lkp_sym(input, symbol).obj) return false;
  
  ant_prop_loc_t method = lkp_sym(js->sym.array_proto, symbol);
  if (!method.obj || js_prop_load(method) != js->sym.array_values_fn) return false;
  
  const ant_shape_prop_t *prop = ant_shape_prop_at(method.obj->shape, method.slot);
  if (!prop || prop->has_getter || prop->has_setter || method.obj->flags.is_exotic) return false;
  
  ant_prop_loc_t next = lkp(js, js->sym.array_iterator_proto, "next", 4);
  if (!next.obj || js_prop_load(next) != js->mutable_roots.crypto_array_iterator_next) return false;
  
  prop = ant_shape_prop_at(next.obj->shape, next.slot);
  if (!prop || prop->has_getter || prop->has_setter || next.obj->flags.is_exotic) return false;

  uint8_t usages = 0;
  for (uint32_t i = 0; i < array->u.array.len; i++) {
    ant_value_t value = array->u.array.data[i];
    if (vtype(value) != kTypeString) return false;
    uint8_t bit = crypto_key_usage_bit(js, value);
    if (!bit) return false;
    usages |= bit;
  }
  
  *mask = usages;
  return true;
}

static ant_value_t crypto_read_key_usages(ant_t *js, ant_value_t input, uint8_t *mask) {
  if (!is_object_type(input))
    return js_mkerr_typed(js, JS_ERR_TYPE, "keyUsages must be an iterable sequence");
  *mask = 0;
  if (crypto_read_dense_key_usages(js, input, mask)) return Ant_Value_Undefined;
  // TODO: align js_iter with WebCrypto sequence semantics (getters, cached next,
  // TypeError validation, and preserving conversion exceptions during IteratorClose).
  bool iterated = js_iter(js, input, crypto_read_key_usage, mask);
  if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
  if (!iterated)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Failed to iterate keyUsages");
  return Ant_Value_Undefined;
}

static ant_value_t crypto_check_key_usages(
  ant_t *js, ant_crypto_key_kind_t kind, uint8_t usages
) {
  uint8_t allowed;
  switch (kind) {
    case CRYPTO_KEY_AES_GCM:
      allowed = CRYPTO_KEY_USAGE_ENCRYPT | CRYPTO_KEY_USAGE_DECRYPT |
        CRYPTO_KEY_USAGE_WRAP_KEY | CRYPTO_KEY_USAGE_UNWRAP_KEY;
      break;
    case CRYPTO_KEY_HMAC:
      allowed = CRYPTO_KEY_USAGE_SIGN | CRYPTO_KEY_USAGE_VERIFY;
      break;
    default:
      allowed = CRYPTO_KEY_USAGE_DERIVE_KEY | CRYPTO_KEY_USAGE_DERIVE_BITS;
      break;
  }
  return (usages & ~allowed)
    ? crypto_dom_exception_error(js, "Unsupported key usage for algorithm", "SyntaxError")
    : Ant_Value_Undefined;
}

static ant_value_t crypto_require_nonempty_secret_usages(
  ant_t *js, uint8_t usage_mask
) {
  return usage_mask == 0
    ? crypto_dom_exception_error(
        js, "Usages cannot be empty when creating a secret key", "SyntaxError"
      )
    : Ant_Value_Undefined;
}

static ant_value_t crypto_get_key_algorithm_kind(
  ant_t *js, ant_value_t algorithm,
  ant_crypto_key_kind_t *kind
) {
  ant_value_t name_value = crypto_subtle_get_algorithm_name(js, algorithm);
  if (is_err(name_value)) return name_value;

  size_t len = 0;
  const char *value = js_getstr(js, name_value, &len);
  if (value && len == 7 && strncasecmp(value, "AES-GCM", 7) == 0) {
    *kind = CRYPTO_KEY_AES_GCM;
    return Ant_Value_Undefined;
  }
  if (value && len == 4 && strncasecmp(value, "HMAC", 4) == 0) {
    *kind = CRYPTO_KEY_HMAC;
    return Ant_Value_Undefined;
  }
  if (value && len == 6 && strncasecmp(value, "PBKDF2", 6) == 0) {
    *kind = CRYPTO_KEY_PBKDF2;
    return Ant_Value_Undefined;
  }
  return crypto_dom_exception_error(js, "Unsupported key algorithm", "NotSupportedError");
}

static ant_value_t crypto_read_key_format(ant_t *js, ant_value_t value, bool *raw) {
  ant_value_t string = crypto_webcrypto_string(js, value);
  if (is_err(string)) return string;
  size_t len = 0;
  const char *format = js_getstr(js, string, &len);
  *raw = len == 3 && memcmp(format, "raw", 3) == 0;
  if (*raw || (len == 3 && memcmp(format, "jwk", 3) == 0) ||
      (len == 4 && memcmp(format, "spki", 4) == 0) ||
      (len == 5 && memcmp(format, "pkcs8", 5) == 0))
    return Ant_Value_Undefined;
  return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid CryptoKey format");
}

static ant_value_t crypto_new_key_record(ant_t *js, unsigned index) {
  ant_value_t seed = js->mutable_roots.crypto_key_templates[index];
  if (vtype(seed) != kTypeObject) {
    seed = Ant_Object_CreateBare(js);
    if (is_err(seed)) return seed;
    GC_ROOT_SAVE(mark, js);
    GC_ROOT_PIN(js, seed);
    if (index == 0) {
      mkprop(js, seed, Ant_String_FromLiteral(js, "type"), Ant_String_FromLiteral(js, "secret"), ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "extractable"), Ant_Value_False, ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "algorithm"), Ant_Value_Undefined, ANT_PROP_ATTR_ENUMERABLE);
      mkprop(js, seed, Ant_String_FromLiteral(js, "usages"), Ant_Value_Undefined, ANT_PROP_ATTR_ENUMERABLE);
      js_set_tostringtag(js, seed, Ant_String_FromLiteral(js, "CryptoKey"));
    } else {
      static const char *const hashes[] = { "SHA-1", "SHA-256", "SHA-384", "SHA-512" };
      ant_value_t name = index == CRYPTO_KEY_AES_GCM ? Ant_String_FromLiteral(js, "AES-GCM") :
        index == CRYPTO_KEY_HMAC ? Ant_String_FromLiteral(js, "HMAC") :
        index == CRYPTO_KEY_PBKDF2 ? Ant_String_FromLiteral(js, "PBKDF2") :
        js_mkstr(js, hashes[index - 4], strlen(hashes[index - 4]));
      js_mkprop_fast(js, seed, "name", 4, name);
      if (index == CRYPTO_KEY_AES_GCM || index == CRYPTO_KEY_HMAC)
        js_mkprop_fast(js, seed, "length", 6, Ant_Value_FromNumber(0));
      if (index == CRYPTO_KEY_HMAC)
        js_mkprop_fast(js, seed, "hash", 4, Ant_Value_Undefined);
    }
    GC_ROOT_RESTORE(js, mark);
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    js->mutable_roots.crypto_key_templates[index] = seed;
  }

  return js_mkobj_from_template(js, seed);
}

static inline void crypto_key_record_set(ant_t *js, ant_value_t obj, uint32_t slot, ant_value_t value) {
  ant_object_t *target = js_obj_ptr(obj);
  ant_object_prop_set_unchecked(target, slot, value);
  gc_write_barrier(js, target, value);
}

static ant_value_t crypto_make_key_algorithm(
  ant_t *js, ant_crypto_key_kind_t kind, const EVP_MD *md, uint32_t key_bits
) {
  ant_value_t algorithm = crypto_new_key_record(js, (unsigned)kind), hash = Ant_Value_Undefined;
  if (is_err(algorithm)) return algorithm;
  GC_ROOT_SAVE(root_mark, js);
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, hash);

  if (kind == CRYPTO_KEY_AES_GCM) {
    crypto_key_record_set(js, algorithm, 1, Ant_Value_FromNumber((double)key_bits));
  } else if (kind == CRYPTO_KEY_HMAC) {
    unsigned hash_index = crypto_hash_record_index(md);
    if (!hash_index) {
      ant_value_t error = crypto_dom_exception_error(js, "Unsupported HMAC hash algorithm", "NotSupportedError");
      GC_ROOT_RESTORE(js, root_mark);
      return error;
    }
    hash = crypto_new_key_record(js, hash_index);
    if (is_err(hash)) {
      GC_ROOT_RESTORE(js, root_mark);
      return hash;
    }
    crypto_key_record_set(js, algorithm, 1, Ant_Value_FromNumber((double)key_bits));
    crypto_key_record_set(js, algorithm, 2, hash);
  }

  GC_ROOT_RESTORE(js, root_mark);
  return algorithm;
}

static ant_crypto_key_t *crypto_key_alloc(size_t len) {
  size_t capacity = len ? len : 1;
  if (capacity > SIZE_MAX - sizeof(ant_crypto_key_t)) return NULL;
  ant_crypto_key_t *key = malloc(sizeof(*key) + capacity);
  if (!key) return NULL;
  *key = (ant_crypto_key_t){ .key = (uint8_t *)(key + 1), .key_len = len };
  return key;
}

static ant_value_t crypto_make_key_object(
  ant_t *js, ant_crypto_key_t *key, uint32_t bits
) {
  if (key->kind == CRYPTO_KEY_HMAC && bits % 8)
    key->key[key->key_len - 1] &= (uint8_t)(0xffu << (8 - bits % 8));

  GC_ROOT_SAVE(root_mark, js);
  ant_value_t algorithm = Ant_Value_Undefined, usages = Ant_Value_Undefined, obj = Ant_Value_Undefined;
  GC_ROOT_PIN(js, algorithm);
  GC_ROOT_PIN(js, usages);
  GC_ROOT_PIN(js, obj);
  
  algorithm = crypto_make_key_algorithm(js, key->kind, key->md, bits);
  if (is_err(algorithm)) { obj = algorithm; goto cleanup; }
  ant_value_t usage_values[8];
  uint32_t usage_count = 0;
  for (unsigned i = 0; i < 8; i++) {
    if (key->usages & (1u << i))
      usage_values[usage_count++] = js->mutable_roots.crypto_usage_strings[i];
  }
  usages = js_mkarr_dense_literal(js, usage_values, usage_count);
  if (is_err(usages)) { obj = usages; goto cleanup; }
  
  obj = crypto_new_key_record(js, 0);
  if (is_err(obj)) goto cleanup;
  crypto_key_record_set(js, obj, 1, Ant_Value_FromBool(key->extractable));
  crypto_key_record_set(js, obj, 2, algorithm);
  crypto_key_record_set(js, obj, 3, usages);
  
  if (Ant_Exception_Pending(js)) { obj = Ant_Exception_Current(js); goto cleanup; }
  js_set_native(obj, key, CRYPTO_KEY_NATIVE_TAG);
  js_set_finalizer(obj, crypto_key_finalize);
  key = NULL;

cleanup:
  crypto_key_free(key);
  GC_ROOT_RESTORE(js, root_mark);
  return obj;
}

static ant_crypto_key_t *crypto_require_key(ant_value_t value) {
  return (ant_crypto_key_t *)js_get_native(value, CRYPTO_KEY_NATIVE_TAG);
}

static ant_value_t crypto_authorize_key(
  ant_t *js, ant_value_t value, ant_crypto_key_kind_t kind,
  uint8_t usage, ant_crypto_key_t **key
) {
  *key = crypto_require_key(value);
  if (!*key)
    return js_mkerr_typed(js, JS_ERR_TYPE, "Operation requires a CryptoKey");
  if ((*key)->kind != kind || !((*key)->usages & usage))
    return crypto_dom_exception_error(js, "CryptoKey does not permit this operation", "InvalidAccessError");
  return Ant_Value_Undefined;
}

static ant_value_t crypto_subtle_import_key_impl(ant_params_t) {
  if (nargs < 5)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.importKey requires format, keyData, algorithm, extractable, and keyUsages");

  bool raw = false;
  ant_value_t result = crypto_read_key_format(js, args[0], &raw);
  if (is_err(result)) return result;
  if (!raw)
    return crypto_dom_exception_error(js, "Only raw key import is supported", "NotSupportedError");

  uint8_t usages = 0;
  result = crypto_read_key_usages(js, args[4], &usages);
  if (is_err(result)) return result;

  const uint8_t *bytes = NULL;
  size_t len = 0;
  if (!buffer_source_get_bytes(js, args[1], &bytes, &len))
    return js_mkerr_typed(js, JS_ERR_TYPE, "keyData must be a BufferSource");

  ant_crypto_key_t *key = crypto_key_alloc(len);
  if (!key) return Ant_Error_CreateOutOfMemory(js);
  if (len) memcpy(key->key, bytes, len);
  key->extractable = js_truthy(js, args[3]);
  key->usages = usages;
  uint32_t bits = 0;

  result = crypto_get_key_algorithm_kind(js, args[2], &key->kind);
  if (is_err(result)) goto cleanup;
  bool length_present = false;
  if (key->kind == CRYPTO_KEY_HMAC) {
    result = crypto_get_hmac_digest(js, args[2], &key->md);
    if (is_err(result)) goto cleanup;
    result = crypto_read_key_length(
      js, args[2], false, UINT32_MAX, &length_present, &bits);
    if (is_err(result)) goto cleanup;
  }
  result = crypto_check_key_usages(js, key->kind, usages);
  if (is_err(result)) goto cleanup;

  if (key->kind == CRYPTO_KEY_AES_GCM) {
    if (!crypto_aes_gcm_cipher_for_key(len)) {
      result = crypto_dom_exception_error(js, "Invalid AES-GCM key length", "DataError");
      goto cleanup;
    }
    bits = (uint32_t)(len * 8);
  } else if (key->kind == CRYPTO_KEY_HMAC) {
    if (!len || len > UINT32_MAX / 8u) {
      result = crypto_dom_exception_error(js, "Invalid HMAC key length", "DataError");
      goto cleanup;
    }
    uint32_t data_bits = (uint32_t)(len * 8);
    if (!length_present) bits = data_bits;
    if (bits > data_bits || bits <= data_bits - 8) {
      result = crypto_dom_exception_error(js, "Invalid HMAC key length", "DataError");
      goto cleanup;
    }
  } else if (key->extractable) {
    result = crypto_dom_exception_error(js, "PBKDF2 keys must not be extractable", "SyntaxError");
    goto cleanup;
  }

  result = crypto_require_nonempty_secret_usages(js, usages);
  if (is_err(result)) goto cleanup;
  return crypto_make_key_object(js, key, bits);

cleanup:
  crypto_key_free(key);
  return result;
}

static ant_value_t js_crypto_subtle_import_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_import_key_impl);
}

static ant_value_t crypto_subtle_generate_key_impl(ant_params_t) {
  if (nargs < 3)
    return js_mkerr_typed(js, JS_ERR_TYPE,
      "subtle.generateKey requires algorithm, extractable, and keyUsages");

  uint8_t usages = 0;
  ant_value_t result = crypto_read_key_usages(js, args[2], &usages);
  if (is_err(result)) return result;
  ant_crypto_key_kind_t kind = 0;
  result = crypto_get_key_algorithm_kind(js, args[0], &kind);
  if (is_err(result)) return result;
  if (kind == CRYPTO_KEY_PBKDF2)
    return crypto_dom_exception_error(js, "Unsupported generateKey algorithm", "NotSupportedError");

  const EVP_MD *md = NULL;
  if (kind == CRYPTO_KEY_HMAC) {
    result = crypto_get_hmac_digest(js, args[0], &md);
    if (is_err(result)) return result;
  }
  bool length_present = false;
  uint32_t bits = 0;
  result = crypto_read_key_length(
    js, args[0], kind == CRYPTO_KEY_AES_GCM,
    kind == CRYPTO_KEY_AES_GCM ? UINT16_MAX : UINT32_MAX, &length_present, &bits);
  if (is_err(result)) return result;
  result = crypto_check_key_usages(js, kind, usages);
  if (is_err(result)) return result;

  if (kind == CRYPTO_KEY_AES_GCM) {
    if (bits != 128 && bits != 192 && bits != 256)
      return crypto_dom_exception_error(js, "AES-GCM key length must be 128, 192, or 256", "OperationError");
  } else {
    if (!length_present) bits = (uint32_t)EVP_MD_block_size(md) * 8u;
    if (!bits)
      return crypto_dom_exception_error(js, "HMAC key length must be non-zero", "OperationError");
  }
  result = crypto_require_nonempty_secret_usages(js, usages);
  if (is_err(result)) return result;

  ant_crypto_key_t *key = crypto_key_alloc(bits / 8u + (bits % 8u != 0));
  if (!key) return Ant_Error_CreateOutOfMemory(js);
  key->kind = kind;
  key->md = md;
  key->usages = usages;
  key->extractable = js_truthy(js, args[1]);
  if (crypto_fill_random(key->key, key->key_len) != 0) {
    crypto_key_free(key);
    return crypto_dom_exception_error(js, "Key generation failed", "OperationError");
  }
  return crypto_make_key_object(js, key, bits);
}

static ant_value_t js_crypto_subtle_generate_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_generate_key_impl);
}

static ant_value_t crypto_subtle_export_key_impl(ant_params_t) {
  if (nargs < 2) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.exportKey requires format and key");
  }

  bool raw = false;
  ant_value_t result = crypto_read_key_format(js, args[0], &raw);
  if (is_err(result)) return result;
  ant_crypto_key_t *key = crypto_require_key(args[1]);
  if (!key) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.exportKey requires a CryptoKey");
  }
  if (key->kind != CRYPTO_KEY_AES_GCM && key->kind != CRYPTO_KEY_HMAC) {
    return crypto_dom_exception_error(js, "CryptoKey algorithm cannot be exported", "NotSupportedError");
  }
  if (!key->extractable) {
    return crypto_dom_exception_error(js, "CryptoKey is not extractable", "InvalidAccessError");
  }

  if (!raw)
    return crypto_dom_exception_error(js, "Only raw key export is supported", "NotSupportedError");
  return create_arraybuffer_copy(js, key->key, key->key_len);
}

static ant_value_t js_crypto_subtle_export_key(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_export_key_impl);
}

static ant_value_t crypto_subtle_derive_bits_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.deriveBits requires algorithm, baseKey, and length");
  if (!crypto_algorithm_is(js, args[0], "PBKDF2")) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported deriveBits algorithm");
  }

  ant_crypto_key_t *key = crypto_require_key(args[1]);
  if (!key || key->kind != CRYPTO_KEY_PBKDF2) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 deriveBits requires a PBKDF2 CryptoKey");
  }
  if ((key->usages & CRYPTO_KEY_USAGE_DERIVE_BITS) == 0) {
    return crypto_dom_exception_error(
      js, "CryptoKey does not permit deriveBits", "InvalidAccessError"
    );
  }

  double length_num = Ant_Value_AsNumberUnchecked(args[2]);
  if (!isfinite(length_num) || length_num < 0 || length_num > (double)INT_MAX * 8.0 || fmod(length_num, 8) != 0) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "deriveBits length must be a non-negative multiple of 8");
  }
  int keylen = (int)(length_num / 8);

  ant_value_t salt_val = js_get(js, args[0], "salt");
  const uint8_t *salt = NULL;
  size_t salt_len = 0;
  
  if (!buffer_source_get_bytes(js, salt_val, &salt, &salt_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 salt must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }
  
  if (salt_len > (size_t)INT_MAX || key->key_len > (size_t)INT_MAX) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 input is too large");
  }

  ant_value_t iterations_val = js_get(js, args[0], "iterations");
  double iterations_num = Ant_Value_AsNumberUnchecked(iterations_val);
  if (!isfinite(iterations_num) || iterations_num <= 0 || iterations_num > INT_MAX || floor(iterations_num) != iterations_num) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "PBKDF2 iterations must be a positive integer");
  }
  int iterations = (int)iterations_num;

  ant_value_t hash_val = js_get(js, args[0], "hash");
  const EVP_MD *md = crypto_digest_from_algorithm(js, hash_val);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported PBKDF2 hash algorithm");

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) return Ant_Error_CreateOutOfMemory(js);
  if (PKCS5_PBKDF2_HMAC((const char *)key->key, (int)key->key_len, salt, (int)salt_len, iterations, md, keylen, out) != 1) {
    free(out);
    return js_mkerr(js, "PBKDF2 failed");
  }

  ant_value_t result = create_arraybuffer_copy(js, out, (size_t)keylen);
  free(out);
  return result;
}

static ant_value_t js_crypto_subtle_derive_bits(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_derive_bits_impl);
}

static ant_value_t crypto_hmac_once(
  ant_t *js, ant_crypto_key_t *key, ant_value_t data, ant_value_t encoding,
  bool arraybuffer_result
) {
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, data, encoding, &bytes, &len, &owned);
  if (is_err(err)) return err;

  unsigned char out[EVP_MAX_MD_SIZE];
  unsigned int out_len = 0;
  unsigned char *res = HMAC(key->md, key->key, (int)key->key_len, bytes, len, out, &out_len);
  if (owned) free(owned);
  if (!res) return js_mkerr(js, "HMAC failed");
  return arraybuffer_result ? create_arraybuffer_copy(js, out, out_len) : crypto_make_buffer(js, out, out_len);
}

static ant_value_t crypto_subtle_sign_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.sign requires algorithm, key, and data");
  if (!crypto_algorithm_is(js, args[0], "HMAC")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported signing algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_HMAC, CRYPTO_KEY_USAGE_SIGN, &key);
  if (is_err(error)) return error;
  return crypto_hmac_once(js, key, args[2], Ant_Value_Undefined, true);
}

static ant_value_t js_crypto_subtle_sign(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_sign_impl);
}

static ant_value_t crypto_subtle_verify_impl(ant_params_t) {
  if (nargs < 4) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.verify requires algorithm, key, signature, and data");
  if (!crypto_algorithm_is(js, args[0], "HMAC")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported verification algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_HMAC, CRYPTO_KEY_USAGE_VERIFY, &key);
  if (is_err(error)) return error;

  const uint8_t *sig = NULL;
  size_t sig_len = 0;
  if (!buffer_source_get_bytes(js, args[2], &sig, &sig_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "signature must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  ant_value_t expected = crypto_hmac_once(js, key, args[3], Ant_Value_Undefined, false);
  if (is_err(expected)) return expected;

  const uint8_t *expected_bytes = NULL;
  size_t expected_len = 0;
  if (!buffer_source_get_bytes(js, expected, &expected_bytes, &expected_len)) return Ant_Value_False;
  if (sig_len != expected_len) return Ant_Value_False;
  return Ant_Value_FromBool(CRYPTO_memcmp(sig, expected_bytes, sig_len) == 0);
}

static ant_value_t js_crypto_subtle_verify(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_verify_impl);
}

static ant_value_t crypto_aes_gcm_crypt(
  ant_t *js, bool encrypt, ant_value_t algorithm, ant_crypto_key_t *key, ant_value_t data
) {
  if (!key || key->kind != CRYPTO_KEY_AES_GCM) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM requires an AES CryptoKey");
  }

  ant_value_t iv_val = js_get(js, algorithm, "iv");
  const uint8_t *iv = NULL;
  size_t iv_len = 0;
  if (!buffer_source_get_bytes(js, iv_val, &iv, &iv_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM iv must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  const uint8_t *input = NULL;
  size_t input_len = 0;
  if (!buffer_source_get_bytes(js, data, &input, &input_len)) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM data must be an ArrayBuffer, TypedArray, DataView, or Buffer");
  }

  int tag_len = 16;
  ant_value_t tag_len_val = js_get(js, algorithm, "tagLength");
  if (vtype(tag_len_val) == kTypeNumber) {
    tag_len = (int)(Ant_Value_AsNumberUnchecked(tag_len_val) / 8.0);
    if (tag_len <= 0 || tag_len > 16) return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid AES-GCM tagLength");
  }

  const uint8_t *aad = NULL;
  size_t aad_len = 0;
  ant_value_t aad_val = js_get(js, algorithm, "additionalData");
  bool has_aad = buffer_source_get_bytes(js, aad_val, &aad, &aad_len);

  if (!encrypt && input_len < (size_t)tag_len) {
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM ciphertext is shorter than tagLength");
  }

  const EVP_CIPHER *cipher = crypto_aes_gcm_cipher_for_key(key->key_len);
  if (!cipher) return js_mkerr_typed(js, JS_ERR_TYPE, "Invalid AES-GCM key length");

  size_t text_len = encrypt ? input_len : input_len - (size_t)tag_len;
  size_t out_cap = encrypt ? input_len + (size_t)tag_len : text_len;
  uint8_t *out = malloc(out_cap ? out_cap : 1);
  if (!out) return Ant_Error_CreateOutOfMemory(js);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    free(out);
    return js_mkerr(js, "AES-GCM initialization failed");
  }

  int ok = encrypt
    ? EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL)
    : EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL);
  ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
  ok = ok && (encrypt
    ? EVP_EncryptInit_ex(ctx, NULL, NULL, key->key, iv)
    : EVP_DecryptInit_ex(ctx, NULL, NULL, key->key, iv));

  int out_len = 0;
  int total = 0;
  if (ok && has_aad && aad_len > 0) {
    ok = encrypt
      ? EVP_EncryptUpdate(ctx, NULL, &out_len, aad, (int)aad_len)
      : EVP_DecryptUpdate(ctx, NULL, &out_len, aad, (int)aad_len);
  }

  if (ok && text_len > 0) {
    ok = encrypt
      ? EVP_EncryptUpdate(ctx, out, &out_len, input, (int)text_len)
      : EVP_DecryptUpdate(ctx, out, &out_len, input, (int)text_len);
    total += out_len;
  }

  if (!encrypt && ok) {
    ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag_len, (void *)(input + text_len));
  }

  if (ok) {
    ok = encrypt
      ? EVP_EncryptFinal_ex(ctx, out + total, &out_len)
      : EVP_DecryptFinal_ex(ctx, out + total, &out_len);
    total += out_len;
  }

  if (ok && encrypt) {
    ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, out + total);
    total += tag_len;
  }

  EVP_CIPHER_CTX_free(ctx);
  if (!ok) {
    free(out);
    return js_mkerr_typed(js, JS_ERR_TYPE, "AES-GCM operation failed");
  }

  ant_value_t result = create_arraybuffer_copy(js, out, (size_t)total);
  free(out);
  return result;
}

static ant_value_t crypto_subtle_encrypt_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.encrypt requires algorithm, key, and data");
  if (!crypto_algorithm_is(js, args[0], "AES-GCM")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported encryption algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_AES_GCM, CRYPTO_KEY_USAGE_ENCRYPT, &key);
  if (is_err(error)) return error;
  return crypto_aes_gcm_crypt(js, true, args[0], key, args[2]);
}

static ant_value_t crypto_subtle_decrypt_impl(ant_params_t) {
  if (nargs < 3) return js_mkerr_typed(js, JS_ERR_TYPE, "subtle.decrypt requires algorithm, key, and data");
  if (!crypto_algorithm_is(js, args[0], "AES-GCM")) {
    if (Ant_Exception_Pending(js)) return Ant_Exception_Current(js);
    return crypto_dom_exception_error(js, "Unsupported decryption algorithm", "NotSupportedError");
  }
  ant_crypto_key_t *key = NULL;
  ant_value_t error = crypto_authorize_key(js, args[1], CRYPTO_KEY_AES_GCM, CRYPTO_KEY_USAGE_DECRYPT, &key);
  if (is_err(error)) return error;
  return crypto_aes_gcm_crypt(js, false, args[0], key, args[2]);
}

static ant_value_t js_crypto_subtle_encrypt(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_encrypt_impl);
}

static ant_value_t js_crypto_subtle_decrypt(ant_params_t) {
  return crypto_subtle_call(js, args, nargs, call_this, call_func, crypto_subtle_decrypt_impl);
}

static ant_value_t js_hash_update(ant_params_t) {
  ant_hash_state_t *state = NULL;
  
  const uint8_t *bytes = NULL;
  size_t len = 0;
  
  uint8_t *owned = NULL;
  ant_value_t err;

  err = crypto_require_hash_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hash digest already called");
  if (nargs < 1) return js_mkerr(js, "Hash.update requires data");

  ant_value_t encoding = (nargs >= 2) ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], encoding, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (EVP_DigestUpdate(state->ctx, bytes, len) != 1) {
    err = js_mkerr(js, "Hash update failed");
    goto cleanup;
  }

  err = call_this;

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_value_t js_hash_digest(ant_params_t) {
  ant_hash_state_t *state = NULL;
  ant_value_t err = crypto_require_hash_state(js, call_this, &state);
  
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hash digest already called");

  if (EVP_DigestFinal_ex(state->ctx, state->digest, &state->digest_len) != 1) {
    return js_mkerr(js, "Hash digest failed");
  }

  state->finalized = true;
  EVP_MD_CTX_free(state->ctx);
  state->ctx = NULL;

  return crypto_digest_result(js, state->digest, state->digest_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_crypto_create_hash(ant_params_t) {
  if (nargs < 1) return js_mkerr(js, "createHash requires an algorithm");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;

  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return js_mkerr(js, "Invalid hash algorithm");

  char *algo_name = strndup(algo, algo_len);
  if (!algo_name) return Ant_Error_CreateOutOfMemory(js);

  const EVP_MD *md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported hash algorithm");

  ant_hash_state_t *state = calloc(1, sizeof(*state));
  if (!state) return Ant_Error_CreateOutOfMemory(js);

  state->ctx = EVP_MD_CTX_new();
  state->md = md;
  
  if (!state->ctx || EVP_DigestInit_ex(state->ctx, md, NULL) != 1) {
    crypto_hash_state_free(state);
    return js_mkerr(js, "Failed to initialize hash");
  }

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_hash_update));
  js_set(js, obj, "digest", Ant_Function_Create(js_hash_digest));
  
  js_set_native(obj, state, CRYPTO_HASH_NATIVE_TAG);
  js_set_finalizer(obj, crypto_hash_finalize);
  js_set_tostringtag(js, obj, js_mkstr(js, "Hash", 4));
  
  return obj;
}

static ant_hmac_state_t *crypto_get_hmac_state(ant_value_t value) {
  ant_hmac_state_t *state = (ant_hmac_state_t *)js_get_native(value, CRYPTO_HMAC_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_hmac_state(
  ant_t *js, ant_value_t value, ant_hmac_state_t **out_state
) {
  ant_hmac_state_t *state = crypto_get_hmac_state(value);
  if (!state) return js_mkerr(js, "Invalid Hmac state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t js_hmac_update(ant_params_t) {
  ant_hmac_state_t *state = NULL;
  const uint8_t *bytes = NULL;
  size_t len = 0;
  uint8_t *owned = NULL;

  ant_value_t err = crypto_require_hmac_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hmac digest already called");
  if (nargs < 1) return js_mkerr(js, "Hmac.update requires data");

  ant_value_t encoding = (nargs >= 2) ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], encoding, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (HMAC_Update(state->ctx, bytes, len) != 1) {
    err = js_mkerr(js, "Hmac update failed");
    goto cleanup;
  }

  err = call_this;

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_value_t js_hmac_digest(ant_params_t) {
  ant_hmac_state_t *state = NULL;
  ant_value_t err = crypto_require_hmac_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Hmac digest already called");

  if (HMAC_Final(state->ctx, state->digest, &state->digest_len) != 1) {
    return js_mkerr(js, "Hmac digest failed");
  }

  state->finalized = true;
  HMAC_CTX_free(state->ctx);
  state->ctx = NULL;

  return crypto_digest_result(js, state->digest, state->digest_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_crypto_create_hmac(ant_params_t) {
  if (nargs < 2) return js_mkerr(js, "createHmac requires an algorithm and key");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;
  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  const EVP_MD *md = crypto_digest_from_name(algo, algo_len);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported HMAC algorithm");

  const uint8_t *key = NULL;
  size_t key_len = 0;
  uint8_t *owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &key, &key_len, &owned);
  if (is_err(err)) return err;

  ant_hmac_state_t *state = calloc(1, sizeof(*state));
  if (!state) {
    if (owned) free(owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  state->ctx = HMAC_CTX_new();
  state->md = md;
  if (!state->ctx || HMAC_Init_ex(state->ctx, key, (int)key_len, md, NULL) != 1) {
    if (owned) free(owned);
    crypto_hmac_state_free(state);
    return js_mkerr(js, "Failed to initialize Hmac");
  }
  if (owned) free(owned);

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_hmac_update));
  js_set(js, obj, "digest", Ant_Function_Create(js_hmac_digest));
  js_set_native(obj, state, CRYPTO_HMAC_NATIVE_TAG);
  js_set_finalizer(obj, crypto_hmac_finalize);
  js_set_tostringtag(js, obj, js_mkstr(js, "Hmac", 4));
  return obj;
}

static ant_value_t js_crypto_hash(ant_params_t) {
  ant_value_t algo_val;
  ant_value_t output_encoding;
  
  const char *algo;
  size_t algo_len = 0;
  char *algo_name = NULL;
  
  const EVP_MD *md = NULL;
  const uint8_t *bytes = NULL;
  
  size_t len = 0;
  uint8_t *owned = NULL;
  
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  ant_value_t err;

  if (nargs < 2) return js_mkerr(js, "hash requires algorithm and data");
  output_encoding = nargs >= 3 ? args[2] : Ant_Value_Undefined;

  algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;

  algo = js_getstr(js, algo_val, &algo_len);
  if (!algo || algo_len == 0) return js_mkerr(js, "Invalid hash algorithm");

  algo_name = strndup(algo, algo_len);
  if (!algo_name) return Ant_Error_CreateOutOfMemory(js);

  md = EVP_get_digestbyname(algo_name);
  free(algo_name);
  if (!md) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported hash algorithm");

  err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &bytes, &len, &owned);
  if (is_err(err)) goto cleanup;

  if (EVP_Digest(bytes, len, digest, &digest_len, md, NULL) != 1) {
    err = js_mkerr(js, "Hash failed");
    goto cleanup;
  }

  err = crypto_digest_result(js, digest, digest_len, output_encoding);

cleanup:
  if (owned) free(owned);
  return err;
}

static ant_cipher_state_t *crypto_get_cipher_state(ant_value_t value) {
  ant_cipher_state_t *state = (ant_cipher_state_t *)js_get_native(value, CRYPTO_CIPHER_NATIVE_TAG);
  return (state && state->ctx) ? state : NULL;
}

static ant_value_t crypto_require_cipher_state(
  ant_t *js, ant_value_t value, ant_cipher_state_t **out_state
) {
  ant_cipher_state_t *state = crypto_get_cipher_state(value);
  if (!state) return js_mkerr(js, "Invalid Cipher state");
  *out_state = state;
  return Ant_Value_Undefined;
}

static ant_value_t crypto_cipher_bytes_result(
  ant_t *js, uint8_t *bytes, size_t len, ant_value_t encoding
) {
  if (vtype(encoding) == kTypeString) return crypto_digest_result(js, bytes, len, encoding);
  return crypto_make_buffer(js, bytes, len);
}

static ant_value_t js_cipher_update(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Cipher already finalized");
  if (nargs < 1) return js_mkerr(js, "Cipher.update requires data");

  const uint8_t *input = NULL;
  size_t input_len = 0;
  uint8_t *owned = NULL;
  ant_value_t input_encoding = nargs >= 2 ? args[1] : Ant_Value_Undefined;
  err = crypto_get_input_bytes(js, args[0], input_encoding, &input, &input_len, &owned);
  if (is_err(err)) return err;

  int block_size = EVP_CIPHER_CTX_block_size(state->ctx);
  size_t out_cap = input_len + (block_size > 0 ? (size_t)block_size : 16u);
  uint8_t *out = malloc(out_cap ? out_cap : 1);
  if (!out) {
    if (owned) free(owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  int out_len = 0;
  int ok = state->encrypt
    ? EVP_EncryptUpdate(state->ctx, out, &out_len, input, (int)input_len)
    : EVP_DecryptUpdate(state->ctx, out, &out_len, input, (int)input_len);

  if (owned) free(owned);
  if (!ok) {
    free(out);
    return js_mkerr(js, "Cipher update failed");
  }

  ant_value_t result = crypto_cipher_bytes_result(js, out, (size_t)out_len, nargs >= 3 ? args[2] : Ant_Value_Undefined);
  free(out);
  return result;
}

static ant_value_t js_cipher_final(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (state->finalized) return js_mkerr(js, "Cipher already finalized");

  int block_size = EVP_CIPHER_CTX_block_size(state->ctx);
  size_t out_cap = block_size > 0 ? (size_t)block_size : 16u;
  uint8_t out[EVP_MAX_BLOCK_LENGTH + 16];
  int out_len = 0;

  int ok = state->encrypt
    ? EVP_EncryptFinal_ex(state->ctx, out, &out_len)
    : EVP_DecryptFinal_ex(state->ctx, out, &out_len);
  (void)out_cap;

  state->finalized = true;
  if (!ok) return js_mkerr(js, "Cipher final failed");

  if (state->encrypt && state->is_gcm) {
    state->auth_tag_len = 16;
    EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_GET_TAG, 16, state->auth_tag);
  }

  return crypto_cipher_bytes_result(js, out, (size_t)out_len, nargs >= 1 ? args[0] : Ant_Value_Undefined);
}

static ant_value_t js_cipher_set_aad(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (nargs < 1) return js_mkerr(js, "setAAD requires data");

  const uint8_t *aad = NULL;
  size_t aad_len = 0;
  if (!buffer_source_get_bytes(js, args[0], &aad, &aad_len)) {
    return js_mkerr(js, "setAAD data must be a Buffer, TypedArray, DataView, or ArrayBuffer");
  }

  int out_len = 0;
  int ok = state->encrypt
    ? EVP_EncryptUpdate(state->ctx, NULL, &out_len, aad, (int)aad_len)
    : EVP_DecryptUpdate(state->ctx, NULL, &out_len, aad, (int)aad_len);
  if (!ok) return js_mkerr(js, "setAAD failed");
  return call_this;
}

static ant_value_t js_cipher_get_auth_tag(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (!state->is_gcm || !state->encrypt || state->auth_tag_len == 0) {
    return js_mkerr(js, "Auth tag is not available");
  }
  return crypto_make_buffer(js, state->auth_tag, state->auth_tag_len);
}

static ant_value_t js_cipher_set_auth_tag(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  if (nargs < 1) return js_mkerr(js, "setAuthTag requires a tag");

  const uint8_t *tag = NULL;
  size_t tag_len = 0;
  if (!buffer_source_get_bytes(js, args[0], &tag, &tag_len) || tag_len > sizeof(state->auth_tag)) {
    return js_mkerr(js, "Invalid authentication tag");
  }

  memcpy(state->auth_tag, tag, tag_len);
  state->auth_tag_len = tag_len;
  if (!state->encrypt && state->is_gcm) {
    EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_SET_TAG, (int)tag_len, state->auth_tag);
  }
  return call_this;
}

static ant_value_t js_cipher_set_auto_padding(ant_params_t) {
  ant_cipher_state_t *state = NULL;
  ant_value_t err = crypto_require_cipher_state(js, call_this, &state);
  if (is_err(err)) return err;
  int padding = nargs < 1 || js_truthy(js, args[0]);
  EVP_CIPHER_CTX_set_padding(state->ctx, padding);
  return call_this;
}

static ant_value_t crypto_create_cipheriv(ant_native_params_t, bool encrypt) {
  if (nargs < 3) return js_mkerr(js, "createCipheriv requires algorithm, key, and iv");

  ant_value_t algo_val = js_tostring_val(js, args[0]);
  if (is_err(algo_val)) return algo_val;
  size_t algo_len = 0;
  const char *algo = js_getstr(js, algo_val, &algo_len);
  const EVP_CIPHER *cipher = crypto_cipher_from_name(algo, algo_len);
  if (!cipher) return js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported cipher algorithm");

  const uint8_t *key = NULL;
  size_t key_len = 0;
  uint8_t *key_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &key, &key_len, &key_owned);
  if (is_err(err)) return err;

  const uint8_t *iv = NULL;
  size_t iv_len = 0;
  uint8_t *iv_owned = NULL;
  if (vtype(args[2]) != kTypeNull && vtype(args[2]) != kTypeUndefined) {
    err = crypto_get_input_bytes(js, args[2], Ant_Value_Undefined, &iv, &iv_len, &iv_owned);
    if (is_err(err)) {
      if (key_owned) free(key_owned);
      return err;
    }
  }

  ant_cipher_state_t *state = calloc(1, sizeof(*state));
  if (!state) {
    if (key_owned) free(key_owned);
    if (iv_owned) free(iv_owned);
    return Ant_Error_CreateOutOfMemory(js);
  }

  state->ctx = EVP_CIPHER_CTX_new();
  state->encrypt = encrypt;
  state->is_gcm = EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE;
  if (!state->ctx) {
    crypto_cipher_state_free(state);
    if (key_owned) free(key_owned);
    if (iv_owned) free(iv_owned);
    return js_mkerr(js, "Cipher initialization failed");
  }

  int ok = encrypt
    ? EVP_EncryptInit_ex(state->ctx, cipher, NULL, NULL, NULL)
    : EVP_DecryptInit_ex(state->ctx, cipher, NULL, NULL, NULL);
  if (ok && state->is_gcm && iv) {
    ok = EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
  }
  ok = ok && (encrypt
    ? EVP_EncryptInit_ex(state->ctx, NULL, NULL, key, iv)
    : EVP_DecryptInit_ex(state->ctx, NULL, NULL, key, iv));

  if (key_owned) free(key_owned);
  if (iv_owned) free(iv_owned);
  if (!ok) {
    crypto_cipher_state_free(state);
    return js_mkerr(js, "Cipher initialization failed");
  }

  ant_value_t obj = Ant_Object_CreateBare(js);
  js_set(js, obj, "update", Ant_Function_Create(js_cipher_update));
  js_set(js, obj, "final", Ant_Function_Create(js_cipher_final));
  js_set(js, obj, "setAAD", Ant_Function_Create(js_cipher_set_aad));
  js_set(js, obj, "getAuthTag", Ant_Function_Create(js_cipher_get_auth_tag));
  js_set(js, obj, "setAuthTag", Ant_Function_Create(js_cipher_set_auth_tag));
  js_set(js, obj, "setAutoPadding", Ant_Function_Create(js_cipher_set_auto_padding));
  js_set_native(obj, state, CRYPTO_CIPHER_NATIVE_TAG);
  js_set_finalizer(obj, crypto_cipher_finalize);
  return obj;
}

static ant_value_t js_crypto_create_cipheriv(ant_params_t) {
  return crypto_create_cipheriv(js, args, nargs, true);
}

static ant_value_t js_crypto_create_decipheriv(ant_params_t) {
  return crypto_create_cipheriv(js, args, nargs, false);
}

static ant_value_t crypto_pbkdf2_result(
  ant_t *js, ant_value_t password_val, ant_value_t salt_val,
  ant_value_t iterations_val, ant_value_t keylen_val, ant_value_t digest_val
) {
  const uint8_t *password = NULL, *salt = NULL;
  size_t password_len = 0, salt_len = 0;
  uint8_t *password_owned = NULL, *salt_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, password_val, Ant_Value_Undefined, &password, &password_len, &password_owned);
  if (is_err(err)) return err;
  err = crypto_get_input_bytes(js, salt_val, Ant_Value_Undefined, &salt, &salt_len, &salt_owned);
  if (is_err(err)) goto cleanup_err;

  int iterations = (int)Ant_Value_AsNumberUnchecked(iterations_val);
  int keylen = (int)Ant_Value_AsNumberUnchecked(keylen_val);
  if (iterations <= 0 || keylen < 0) {
    err = js_mkerr(js, "Invalid PBKDF2 parameters");
    goto cleanup_err;
  }

  ant_value_t digest_str = js_tostring_val(js, digest_val);
  if (is_err(digest_str)) {
    err = digest_str;
    goto cleanup_err;
  }
  size_t digest_len = 0;
  const char *digest = js_getstr(js, digest_str, &digest_len);
  const EVP_MD *md = crypto_digest_from_name(digest, digest_len);
  if (!md) {
    err = js_mkerr_typed(js, JS_ERR_TYPE, "Unsupported PBKDF2 digest");
    goto cleanup_err;
  }

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) {
    err = Ant_Error_CreateOutOfMemory(js);
    goto cleanup_err;
  }
  if (PKCS5_PBKDF2_HMAC((const char *)password, (int)password_len, salt, (int)salt_len, iterations, md, keylen, out) != 1) {
    free(out);
    err = js_mkerr(js, "PBKDF2 failed");
    goto cleanup_err;
  }

  ant_value_t result = crypto_make_buffer(js, out, (size_t)keylen);
  free(out);
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return result;

cleanup_err:
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return err;
}

static ant_value_t js_crypto_pbkdf2_sync(ant_params_t) {
  if (nargs < 5) return js_mkerr(js, "pbkdf2Sync requires password, salt, iterations, keylen, and digest");
  return crypto_pbkdf2_result(js, args[0], args[1], args[2], args[3], args[4]);
}

static ant_value_t js_crypto_pbkdf2(ant_params_t) {
  if (nargs < 6 || (vtype(args[5]) != kTypeFunction && vtype(args[5]) != kTypeBuiltin)) {
    return js_mkerr(js, "pbkdf2 requires a callback");
  }
  ant_value_t result = crypto_pbkdf2_result(js, args[0], args[1], args[2], args[3], args[4]);
  ant_value_t cb_args[2] = { is_err(result) ? result : Ant_Value_Null, is_err(result) ? Ant_Value_Undefined : result };
  ant_value_t cb_result = Ant_Error_CallCallback(js, args[5], Ant_Value_Undefined, cb_args, 2);
  return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
}

static ant_value_t crypto_scrypt_result(ant_native_params_t, int options_index) {
  const uint8_t *password = NULL, *salt = NULL;
  size_t password_len = 0, salt_len = 0;
  uint8_t *password_owned = NULL, *salt_owned = NULL;
  ant_value_t err = crypto_get_input_bytes(js, args[0], Ant_Value_Undefined, &password, &password_len, &password_owned);
  if (is_err(err)) return err;
  err = crypto_get_input_bytes(js, args[1], Ant_Value_Undefined, &salt, &salt_len, &salt_owned);
  if (is_err(err)) goto cleanup_err;

  int keylen = (int)Ant_Value_AsNumberUnchecked(args[2]);
  if (keylen < 0) {
    err = js_mkerr(js, "Invalid scrypt key length");
    goto cleanup_err;
  }

  uint64_t N = 16384, r = 8, p = 1, maxmem = 32 * 1024 * 1024;
  if (options_index >= 0 && options_index < nargs && is_object_type(args[options_index])) {
    ant_value_t v = js_get(js, args[options_index], "N");
    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "cost");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) N = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "r");

    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "blockSize");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) r = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "p");

    if (vtype(v) == kTypeUndefined) v = js_get(js, args[options_index], "parallelization");
    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) p = (uint64_t)Ant_Value_AsNumberUnchecked(v);
    v = js_get(js, args[options_index], "maxmem");

    if (is_err(v)) { err = v; goto cleanup_err; }
    if (vtype(v) == kTypeNumber) maxmem = (uint64_t)Ant_Value_AsNumberUnchecked(v);
  }

  uint8_t *out = malloc((size_t)keylen ? (size_t)keylen : 1);
  if (!out) {
    err = Ant_Error_CreateOutOfMemory(js);
    goto cleanup_err;
  }

  if (EVP_PBE_scrypt(
    (const char *)password, password_len,
    salt, salt_len, N, r, p, maxmem, out, (size_t)keylen) != 1
  ) {
    free(out);
    err = js_mkerr(js, "scrypt failed");
    goto cleanup_err;
  }

  ant_value_t result = crypto_make_buffer(js, out, (size_t)keylen);
  free(out);
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return result;

cleanup_err:
  if (password_owned) free(password_owned);
  if (salt_owned) free(salt_owned);
  return err;
}

static ant_value_t js_crypto_scrypt_sync(ant_params_t) {
  if (nargs < 3) return js_mkerr(js, "scryptSync requires password, salt, and keylen");
  return crypto_scrypt_result(js, args, nargs, 3);
}

static ant_value_t js_crypto_scrypt(ant_params_t) {
  if (nargs < 4) return js_mkerr(js, "scrypt requires a callback");
  int callback_index = (vtype(args[3]) == kTypeFunction || vtype(args[3]) == kTypeBuiltin) ? 3 : 4;
  if (callback_index >= nargs || (vtype(args[callback_index]) != kTypeFunction && vtype(args[callback_index]) != kTypeBuiltin))
    return js_mkerr(js, "scrypt requires a callback");

  ant_value_t result = crypto_scrypt_result(js, args, nargs, callback_index == 3 ? -1 : 3);
  ant_value_t cb_args[2] = { is_err(result) ? result : Ant_Value_Null, is_err(result) ? Ant_Value_Undefined : result };
  ant_value_t cb_result = Ant_Error_CallCallback(js, args[callback_index], Ant_Value_Undefined, cb_args, 2);

  return is_err(cb_result) ? cb_result : Ant_Value_Undefined;
}

static ant_value_t create_crypto_obj(ant_t *js) {
  ant_value_t crypto_obj = Ant_Object_Create(js);
  ant_value_t subtle_obj = Ant_Object_Create(js);
  
  js_set(js, crypto_obj, "random", Ant_Function_Create(js_crypto_random));
  js_set(js, crypto_obj, "randomBytes", Ant_Function_CreateWithArity(js_crypto_random_bytes, 2));
  js_set(js, crypto_obj, "randomFillSync", Ant_Function_CreateWithArity(js_crypto_random_fill_sync, 1));
  js_set(js, crypto_obj, "randomUUID", Ant_Function_CreateWithArity(js_crypto_random_uuid, 1));
  js_set(js, crypto_obj, "randomUUIDv7", Ant_Function_Create(js_crypto_random_uuidv7));
  js_set(js, crypto_obj, "getRandomValues", Ant_Function_Create(js_crypto_get_random_values));
  // TODO: add remaining WebCrypto key formats, key derivation, wrapping, and asymmetric algorithms.
  js_set(js, subtle_obj, "digest", Ant_Function_Create(js_crypto_subtle_digest));
  js_set(js, subtle_obj, "importKey", Ant_Function_Create(js_crypto_subtle_import_key));
  js_set(js, subtle_obj, "generateKey", Ant_Function_Create(js_crypto_subtle_generate_key));
  js_set(js, subtle_obj, "exportKey", Ant_Function_Create(js_crypto_subtle_export_key));
  js_set(js, subtle_obj, "deriveBits", Ant_Function_Create(js_crypto_subtle_derive_bits));
  js_set(js, subtle_obj, "sign", Ant_Function_Create(js_crypto_subtle_sign));
  js_set(js, subtle_obj, "verify", Ant_Function_Create(js_crypto_subtle_verify));
  js_set(js, subtle_obj, "encrypt", Ant_Function_Create(js_crypto_subtle_encrypt));
  js_set(js, subtle_obj, "decrypt", Ant_Function_Create(js_crypto_subtle_decrypt));
  js_set(js, subtle_obj, "timingSafeEqual", Ant_Function_Create(js_crypto_timing_safe_equal));
  js_set_tostringtag(js, subtle_obj, js_mkstr(js, "SubtleCrypto", 12));
  js_set(js, crypto_obj, "subtle", subtle_obj);
  
  js_set_tostringtag(js, crypto_obj, js_mkstr(js, "Crypto", 6));
  return crypto_obj;
}

void init_crypto_module(ant_t *js) {
  js->mutable_roots.crypto_array_iterator_next = js_get(js, js->sym.array_iterator_proto, "next");
  for (unsigned i = 0; i < 8; i++)
    js->mutable_roots.crypto_usage_strings[i] = js_mkstr(js, crypto_key_usage_names[i], strlen(crypto_key_usage_names[i]));
  ant_value_t crypto_obj = create_crypto_obj(js);
  js_set(js, js_glob(js), "crypto", crypto_obj);
}

ant_value_t crypto_library(ant_t *js) {
  ant_value_t lib = Ant_Object_CreateBare(js);
  ant_value_t webcrypto = create_crypto_obj(js);
  
  js_set(js, lib, "webcrypto", webcrypto);
  js_set(js, lib, "subtle", js_get(js, webcrypto, "subtle"));
  js_set(js, lib, "hash", Ant_Function_CreateWithArity(js_crypto_hash, 3));
  js_set(js, lib, "createHash", Ant_Function_CreateWithArity(js_crypto_create_hash, 2));
  js_set(js, lib, "createHmac", Ant_Function_CreateWithArity(js_crypto_create_hmac, 3));
  js_set(js, lib, "createCipheriv", Ant_Function_CreateWithArity(js_crypto_create_cipheriv, 4));
  js_set(js, lib, "createDecipheriv", Ant_Function_CreateWithArity(js_crypto_create_decipheriv, 4));
  js_set(js, lib, "pbkdf2", Ant_Function_CreateWithArity(js_crypto_pbkdf2, 6));
  js_set(js, lib, "pbkdf2Sync", Ant_Function_CreateWithArity(js_crypto_pbkdf2_sync, 5));
  js_set(js, lib, "scrypt", Ant_Function_CreateWithArity(js_crypto_scrypt, 4));
  js_set(js, lib, "scryptSync", Ant_Function_CreateWithArity(js_crypto_scrypt_sync, 3));
  js_set(js, lib, "randomBytes", Ant_Function_CreateWithArity(js_crypto_random_bytes, 2));
  crypto_define_random_aliases(js, lib);
  js_set(js, lib, "randomFill", Ant_Function_CreateWithArity(js_crypto_random_fill, 4));
  js_set(js, lib, "randomFillSync", Ant_Function_CreateWithArity(js_crypto_random_fill_sync, 1));
  js_set(js, lib, "randomInt", Ant_Function_CreateWithArity(js_crypto_random_int, 3));
  js_set(js, lib, "randomUUID", Ant_Function_CreateWithArity(js_crypto_random_uuid, 1));
  js_set(js, lib, "getRandomValues", Ant_Function_Create(js_crypto_get_random_values));
  js_set(js, lib, "getCiphers", Ant_Function_Create(js_crypto_get_ciphers));
  js_set(js, lib, "getCurves", Ant_Function_Create(js_crypto_get_curves));
  js_set(js, lib, "getHashes", Ant_Function_Create(js_crypto_get_hashes));
  js_set(js, lib, "timingSafeEqual", Ant_Function_Create(js_crypto_timing_safe_equal));
  js_set_tostringtag(js, lib, js_mkstr(js, "crypto", 6));

  return lib;
}

#endif /* ANT_MBEDTLS_TLS_BACKEND */
