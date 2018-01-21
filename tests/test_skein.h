#include "crypto/skein.h"
#include "minunit.h"

MU_TEST(test_skein_hash) {
    rc_t rc = SESHAT_SUCCESS;

    int in_len = 32;
    unsigned char *input = calloc(in_len, sizeof(unsigned char));
    unsigned char *hash_output = calloc(SKEIN_HASH_LEN, sizeof(unsigned char));

    rc = skein_hash(input, in_len, hash_output);
    mu_check(rc == SESHAT_SUCCESS);

    free(input);
    free(hash_output);
}

MU_TEST(test_skein_mac) {
    rc_t rc = SESHAT_SUCCESS;

    int in_len = 64;
    unsigned char *input = calloc(in_len, sizeof(unsigned char));
    unsigned char *mac_output_1 = malloc(SKEIN_MAC_LEN * sizeof(unsigned char));
    unsigned char *skein_mac_key =
        calloc(SKEIN_MAC_KEY_LEN, sizeof(unsigned char));

    rc = skein_mac(skein_mac_key, input, in_len, mac_output_1);
    mu_check(rc == SESHAT_SUCCESS);

    SkeinCtx_t *skein_x = malloc(sizeof(SkeinCtx_t));
    unsigned char *mac_output_2 = malloc(SKEIN_MAC_LEN * sizeof(unsigned char));
    rc = skein_mac_init(skein_x, skein_mac_key);
    mu_check(rc == SESHAT_SUCCESS);
    rc = skein_mac_update(skein_x, input, in_len);
    mu_check(rc == SESHAT_SUCCESS);
    rc = skein_mac_final(skein_x, mac_output_2);
    mu_check(rc == SESHAT_SUCCESS);

    rc = memcmp(mac_output_1, mac_output_2, in_len);
    mu_check(rc == SESHAT_SUCCESS);

    free(input);
    free(skein_x);
    free(mac_output_1);
    free(mac_output_2);
    free(skein_mac_key);
}

MU_TEST_SUITE(test_skein) {
    MU_RUN_TEST(test_skein_hash);
    MU_RUN_TEST(test_skein_mac);
}