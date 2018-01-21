#include "foundation/foundation.h"
#include "crypto/threefish.h"
#include "utils/common.h"

void t3f_set_key(ThreefishKey_t *t3f_x, uint64_t *t3f_key,
                 uint64_t *t3f_tweak) {
    FOUNDATION_ASSERT(t3f_key != NULL);
    FOUNDATION_ASSERT(t3f_tweak != NULL);
    threefishSetKey(t3f_x, Threefish1024, t3f_key, t3f_tweak);
}

void t3f_encrypt(ThreefishKey_t *t3f_x, unsigned char *input,
                 unsigned char *output) {
    FOUNDATION_ASSERT(t3f_x != NULL);
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(output != NULL);
    threefishEncryptBlockBytes(t3f_x, input, output);
}

void t3f_decrypt(ThreefishKey_t *t3f_x, unsigned char *input,
                 unsigned char *output) {
    FOUNDATION_ASSERT(t3f_x != NULL);
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(output != NULL);
    threefishDecryptBlockBytes(t3f_x, input, output);
}