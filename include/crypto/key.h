#ifndef __KEY_H__
#define __KEY_H__

#include "argon2/argon2.h"
#include "crypto/skein.h"
#include "crypto/threefish.h"
#include "hc256/hc256.h"
#include "utils/common.h"

#define KEY_MAIN_MIN_LEN 32U
#define KEY_MAIN_SALT_LEN 32U
#define KEY_MAIN_ENC_KEY_LEN THREEFISH_KEY_LEN

#define KEY_CHUNK_HASH_KEY_LEN SKEIN_MAC_KEY_LEN
#define KEY_CHUNK_ID_KEY_LEN SKEIN_MAC_KEY_LEN
#define KEY_CHUNK_ENC_KEY_LEN THREEFISH_KEY_LEN
#define KEY_CHUNK_HC256_KIV_LEN HC256_KIV_LEN
#define KEY_CHUNK_CTR_IV_LEN THREEFISH_BLOCK_LEN
#define KEY_CHUNK_TWEAK_LEN THREEFISH_TWEAK_LEN
#define KEY_CHUNK_MAC_KEY_LEN SKEIN_MAC_KEY_LEN
#define KEY_CHUNK_SALT_LEN 32U

#define KEY_CHUNK_TOTAL_LEN                                                    \
    (KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN + KEY_CHUNK_ENC_KEY_LEN +   \
     KEY_CHUNK_HC256_KIV_LEN + KEY_CHUNK_CTR_IV_LEN + THREEFISH_TWEAK_LEN +    \
     KEY_CHUNK_MAC_KEY_LEN)

struct key_chunk_t {
    unsigned char hash_key[KEY_CHUNK_HASH_KEY_LEN];
    unsigned char id_key[KEY_CHUNK_ID_KEY_LEN];
    unsigned char enc_key[KEY_CHUNK_ENC_KEY_LEN];
    unsigned char hc256_kiv[KEY_CHUNK_HC256_KIV_LEN];
    unsigned char ctr_iv[KEY_CHUNK_CTR_IV_LEN];
    unsigned char tweak[KEY_CHUNK_TWEAK_LEN];
    unsigned char mac_key[KEY_CHUNK_MAC_KEY_LEN];
};
typedef struct key_chunk_t key_chunk_t;

rc_t key_main_derive(const unsigned char *input, uint32_t in_len,
                     const unsigned char *salt, unsigned char *main_enc_key);
rc_t key_chunk_derive(key_chunk_t *key_chunk, const unsigned char *main_enc_key,
                      const unsigned char *chunk_salt);

#endif //__KEY_H__