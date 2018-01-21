#include "crypto/ctr.h"
#include "foundation/foundation.h"

rc_t ctr_init(ctr_ctx_t *ctr_x, key_chunk_t *key_chunk) {
    FOUNDATION_ASSERT(ctr_x != NULL);
    FOUNDATION_ASSERT(key_chunk != NULL);

    rc_t rc = SESHAT_SUCCESS;
    t3f_set_key(ctr_x->t3f_x, (uint64_t *)key_chunk->enc_key,
                (uint64_t *)key_chunk->tweak);
    hc256_set_kiv(ctr_x->hc256_x, key_chunk->hc256_kiv);
    ctr_x->is_hc256_used = false;

    return rc;
}

rc_t ctr_set_hc256_kiv(ctr_ctx_t *ctr_x, key_chunk_t *key_chunk) {
    FOUNDATION_ASSERT(ctr_x != NULL);
    FOUNDATION_ASSERT(key_chunk != NULL);

    rc_t rc = SESHAT_SUCCESS;
    hc256_set_kiv(ctr_x->hc256_x, key_chunk->hc256_kiv);
    ctr_x->is_hc256_used = false;
    return rc;
}

rc_t ctr_encrypt(ctr_ctx_t *ctr_x, unsigned char *input, size_t in_len,
                 unsigned char *output, unsigned char *ctr_iv) {
    FOUNDATION_ASSERT(ctr_x != NULL);
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(in_len > 0);
    FOUNDATION_ASSERT(output != NULL);
    FOUNDATION_ASSERT(ctr_iv != NULL);

    rc_t rc = SESHAT_SUCCESS;

    if (ctr_x->is_hc256_used == true) {
        rc = SESHAT_CTR_HC256_USED_ERR;
        log_rc_msg("ctr_encrypt", rc);
        goto exit;
    }

    // hc256_gen_bytes uses XOR so counter must be zerod
    unsigned char *counter =
        memory_allocate(0, CTR_COUNTER_LEN * sizeof(unsigned char), 0,
                        MEMORY_TEMPORARY | MEMORY_ZERO_INITIALIZED);
    if (counter == NULL) {
        rc = SESHAT_OUT_OF_MEMORY_ERR;
        log_rc_msg("ctr_encrypt::memory_allocate", rc);
        goto clean_1;
    }

    unsigned char *t3f_buf = memory_allocate(
        0, THREEFISH_BLOCK_LEN * sizeof(unsigned char), 0, MEMORY_TEMPORARY);
    if (t3f_buf == NULL) {
        rc = SESHAT_OUT_OF_MEMORY_ERR;
        log_rc_msg("ctr_encrypt::memory_allocate", rc);
        goto clean_2;
    }

    unsigned char *ctr_iv_counter = memory_allocate(
        0, THREEFISH_BLOCK_LEN * sizeof(unsigned char), 0, MEMORY_TEMPORARY);
    if (ctr_iv_counter == NULL) {
        rc = SESHAT_OUT_OF_MEMORY_ERR;
        log_rc_msg("ctr_encrypt::memory_allocate", rc);
        goto clean_3;
    }
    memcpy(ctr_iv_counter, ctr_iv, THREEFISH_BLOCK_LEN);

    uint32_t i = 0;
    for (; in_len >= THREEFISH_BLOCK_LEN; ++i, in_len -= THREEFISH_BLOCK_LEN) {
        hc256_gen_bytes(ctr_x->hc256_x, counter, CTR_COUNTER_LEN);
        for (uint32_t j = 0; j < CTR_COUNTER_LEN; ++j) {
            ctr_iv_counter[j] = ctr_iv[j] ^ counter[j];
        }

        t3f_encrypt(ctr_x->t3f_x, ctr_iv_counter, t3f_buf);
        for (uint32_t j = 0; j < THREEFISH_BLOCK_LEN; ++j) {
            output[i * THREEFISH_BLOCK_LEN + j] =
                input[i * THREEFISH_BLOCK_LEN + j] ^ t3f_buf[j];
        }
    }
    if (in_len > 0) {
        hc256_gen_bytes(ctr_x->hc256_x, counter, CTR_COUNTER_LEN);
        for (uint32_t j = 0; j < CTR_COUNTER_LEN; ++j) {
            ctr_iv_counter[j] = ctr_iv[j] ^ counter[j];
        }

        t3f_encrypt(ctr_x->t3f_x, ctr_iv_counter, t3f_buf);
        for (uint32_t j = 0; j < in_len; ++j) {
            output[i * THREEFISH_BLOCK_LEN + j] =
                input[i * THREEFISH_BLOCK_LEN + j] ^ t3f_buf[j];
        }
    }
    ctr_x->is_hc256_used = true;

clean_3:
    memory_deallocate(ctr_iv_counter);
clean_2:
    memory_deallocate(t3f_buf);
clean_1:
    memory_deallocate(counter);
exit:
    return rc;
}

rc_t ctr_decrypt(ctr_ctx_t *ctr_x, unsigned char *input, size_t in_len,
                 unsigned char *output, unsigned char *ctr_iv) {
    FOUNDATION_ASSERT(ctr_x != NULL);

    rc_t rc = SESHAT_SUCCESS;

    if (ctr_x->is_hc256_used == true) {
        rc = SESHAT_CTR_HC256_USED_ERR;
        log_rc_msg("ctr_decrypt", rc);
        goto exit;
    }
    rc = ctr_encrypt(ctr_x, input, in_len, output, ctr_iv);
    if (rc != SESHAT_SUCCESS) {
        log_rc_msg("ctr_decrypt::ctr_encrypt", rc);
    }

exit:
    return rc;
}