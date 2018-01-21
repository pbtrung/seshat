#ifndef __THREEFISH_H__
#define __THREEFISH_H__

#include "skein3fish/threefishApi.h"

#define THREEFISH_KEY_LEN 128U
#define THREEFISH_TWEAK_LEN 16U
#define THREEFISH_BLOCK_LEN 128U

void t3f_set_key(ThreefishKey_t *t3f_x, uint64_t *t3f_key, uint64_t *t3f_tweak);
void t3f_encrypt(ThreefishKey_t *t3f_x, unsigned char *input,
                 unsigned char *output);
void t3f_decrypt(ThreefishKey_t *t3f_x, unsigned char *input,
                 unsigned char *output);

#endif //__THREEFISH_H__