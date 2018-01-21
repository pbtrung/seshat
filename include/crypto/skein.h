#ifndef __SKEIN_H__
#define __SKEIN_H__

#include "skein3fish/skeinApi.h"
#include "utils/common.h"

#define SKEIN_HASH_LEN 64U
#define SKEIN_MAC_KEY_LEN 64U
#define SKEIN_MAC_LEN 64U

rc_t skein_hash(const unsigned char *input, size_t in_len,
                unsigned char *output);
rc_t skein_mac(const unsigned char *skein_mac_key, const unsigned char *input,
               size_t in_len, unsigned char *output);
rc_t skein_mac_init(SkeinCtx_t *skein_x, const unsigned char *skein_mac_key);
rc_t skein_mac_update(SkeinCtx_t *skein_x, const unsigned char *input,
                      size_t in_len);
rc_t skein_mac_final(SkeinCtx_t *skein_x, unsigned char *output);

#endif //__SKEIN_H__
