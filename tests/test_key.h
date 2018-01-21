// #include <time.h>

// #include "crypto/key.h"
#include "minunit.h"

MU_TEST(test_key_derive) {
    rc_t rc = SESHAT_SUCCESS;

    // int in_len = 64 + 32;
    // unsigned char *input = calloc(in_len, sizeof(unsigned char));
    // unsigned char *salt = calloc(32, sizeof(unsigned char));
    // unsigned char *main_enc_key = calloc(128, sizeof(unsigned char));

    // time_t begin, end;
    // begin = clock();
    // rc = key_main_derive(input, in_len, salt, main_enc_key);
    // end = clock();
    // printf("\n\nkey_main_derive: %fs", (double)(end - begin) / CLOCKS_PER_SEC);
    // mu_check(rc == SESHAT_SUCCESS);

    // key_chunk_t *key_chunk = malloc(sizeof(key_chunk_t));
    // unsigned char *chunk_salt = calloc(32, sizeof(unsigned char));

    // begin = clock();
    // rc = key_chunk_derive(key_chunk, main_enc_key, chunk_salt);
    // end = clock();
    // printf("\nkey_chunk_derive: %fs", (double)(end - begin) / CLOCKS_PER_SEC);
    mu_check(rc == SESHAT_SUCCESS);

    // free(input);
    // free(main_enc_key);
    // free(key_chunk);
    // free(chunk_salt);
}

MU_TEST_SUITE(test_key) { MU_RUN_TEST(test_key_derive); }