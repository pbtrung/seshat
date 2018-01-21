#include "crypto/key.h"
#include "foundation/foundation.h"

rc_t key_main_derive(const unsigned char *input, uint32_t in_len,
                     const unsigned char *salt, unsigned char *main_enc_key) {
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(main_enc_key != NULL);
    FOUNDATION_ASSERT(in_len >= KEY_MAIN_MIN_LEN);
    FOUNDATION_ASSERT(salt != NULL);

    rc_t rc = SESHAT_SUCCESS;
    uint32_t t = 6;
    uint32_t m = 1 << 18;
    uint32_t p = 2;

    rc = argon2i_hash_raw(t, m, p, input, in_len, salt, KEY_MAIN_SALT_LEN,
                          main_enc_key, KEY_MAIN_ENC_KEY_LEN);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_ARGON2_ERR;
        log_rc_msg("key_main_derive::argon2i_hash_raw", rc);
    }

    return rc;
}

rc_t key_chunk_derive(key_chunk_t *key_chunk, const unsigned char *main_enc_key,
                      const unsigned char *chunk_salt) {
    FOUNDATION_ASSERT(key_chunk != NULL);
    FOUNDATION_ASSERT(main_enc_key != NULL);
    FOUNDATION_ASSERT(chunk_salt != NULL);

    rc_t rc = SESHAT_SUCCESS;
    uint32_t t = 3;
    uint32_t m = 1 << 15;
    uint32_t p = 1;

    unsigned char total_key_chunk[KEY_CHUNK_TOTAL_LEN];
    rc = argon2i_hash_raw(t, m, p, main_enc_key, KEY_MAIN_ENC_KEY_LEN,
                          chunk_salt, KEY_CHUNK_SALT_LEN, total_key_chunk,
                          KEY_CHUNK_TOTAL_LEN);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_ARGON2_ERR;
        log_rc_msg("key_chunk_derive::argon2i_hash_raw", rc);
        goto exit;
    }

    memcpy(key_chunk->hash_key, total_key_chunk, KEY_CHUNK_HASH_KEY_LEN);
    memcpy(key_chunk->id_key, &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN],
           KEY_CHUNK_ID_KEY_LEN);
    memcpy(key_chunk->enc_key,
           &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN],
           KEY_CHUNK_ENC_KEY_LEN);
    memcpy(key_chunk->hc256_kiv,
           &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN +
                            KEY_CHUNK_ENC_KEY_LEN],
           KEY_CHUNK_HC256_KIV_LEN);
    memcpy(key_chunk->ctr_iv,
           &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN +
                            KEY_CHUNK_ENC_KEY_LEN + KEY_CHUNK_HC256_KIV_LEN],
           KEY_CHUNK_CTR_IV_LEN);
    memcpy(key_chunk->tweak,
           &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN +
                            KEY_CHUNK_ENC_KEY_LEN + KEY_CHUNK_HC256_KIV_LEN +
                            KEY_CHUNK_CTR_IV_LEN],
           KEY_CHUNK_TWEAK_LEN);
    memcpy(key_chunk->mac_key,
           &total_key_chunk[KEY_CHUNK_HASH_KEY_LEN + KEY_CHUNK_ID_KEY_LEN +
                            KEY_CHUNK_ENC_KEY_LEN + KEY_CHUNK_HC256_KIV_LEN +
                            KEY_CHUNK_CTR_IV_LEN + KEY_CHUNK_TWEAK_LEN],
           KEY_CHUNK_MAC_KEY_LEN);

exit:
    return rc;
}