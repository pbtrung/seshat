#include "crypto/ctr.h"
#include "minunit.h"

MU_TEST(test_ctr_enc_dec) {
    rc_t rc = SESHAT_SUCCESS;

    ctr_ctx_t *ctr_x = memory_allocate(0, sizeof(ctr_ctx_t), 0, 0);
    ctr_x->t3f_x = memory_allocate(0, sizeof(ThreefishKey_t), 0, 0);
    ctr_x->hc256_x = memory_allocate(0, sizeof(hc256_ctx_t), 0, 0);

    key_chunk_t *key_chunk =
        memory_allocate(0, sizeof(key_chunk_t), 0, MEMORY_ZERO_INITIALIZED);
    rc = ctr_init(ctr_x, key_chunk);
    mu_check(rc == SESHAT_SUCCESS);

    size_t in_len = THREEFISH_BLOCK_LEN * 2 + 64;
    unsigned char *input = memory_allocate(0, in_len * sizeof(unsigned char), 0,
                                           MEMORY_ZERO_INITIALIZED);
    unsigned char *ctr_output = memory_allocate(
        0, in_len * sizeof(unsigned char), 0, MEMORY_ZERO_INITIALIZED);
    unsigned char *ctr_dec_output = memory_allocate(
        0, in_len * sizeof(unsigned char), 0, MEMORY_ZERO_INITIALIZED);

    rc = ctr_encrypt(ctr_x, input, in_len, ctr_output, key_chunk->ctr_iv);
    mu_check(rc == SESHAT_SUCCESS);
    rc = ctr_set_hc256_kiv(ctr_x, key_chunk);
    mu_check(rc == SESHAT_SUCCESS);
    rc = ctr_decrypt(ctr_x, ctr_output, in_len, ctr_dec_output,
                     key_chunk->ctr_iv);
    mu_check(rc == SESHAT_SUCCESS);
    rc = memcmp(input, ctr_dec_output, in_len);
    mu_check(rc == SESHAT_SUCCESS);

    memory_deallocate(ctr_x->t3f_x);
    memory_deallocate(ctr_x->hc256_x);
    memory_deallocate(ctr_x);
    memory_deallocate(key_chunk);
    memory_deallocate(input);
    memory_deallocate(ctr_output);
    memory_deallocate(ctr_dec_output);
}

MU_TEST_SUITE(test_ctr) { MU_RUN_TEST(test_ctr_enc_dec); }