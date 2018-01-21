#ifndef __CTR_H__
#define __CTR_H__

#include "foundation/foundation.h"

#include "crypto/key.h"
#include "utils/common.h"

#define CTR_COUNTER_LEN 32U

struct ctr_ctx_t {
    ThreefishKey_t *t3f_x;
    hc256_ctx_t *hc256_x;
    // SkeinCtx_t *skein_hash_x; // Hash of plain text
    // SkeinCtx_t *skein_id_x;   // Chunk ID/name in storage
    // SkeinCtx_t *skein_mac_x;  // MAC of salt and cipher text
    bool is_hc256_used;
};
typedef struct ctr_ctx_t ctr_ctx_t;

rc_t ctr_init(ctr_ctx_t *ctr_x, key_chunk_t *key_chunk);
rc_t ctr_set_hc256_kiv(ctr_ctx_t *ctr_x, key_chunk_t *key_chunk);
rc_t ctr_encrypt(ctr_ctx_t *ctr_x, unsigned char *input, size_t in_len,
                 unsigned char *output, unsigned char *ctr_iv);
rc_t ctr_decrypt(ctr_ctx_t *ctr_x, unsigned char *input, size_t in_len,
                 unsigned char *output, unsigned char *ctr_iv);

#endif //__CTR_H__