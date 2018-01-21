#include "foundation/foundation.h"
#include "crypto/skein.h"
#include "skein3fish/skeinApi.h"

rc_t skein_hash(const unsigned char *input, size_t in_len,
                unsigned char *output) {
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(output != NULL);
    FOUNDATION_ASSERT(in_len > 0);

    rc_t rc = SESHAT_SUCCESS;

    SkeinCtx_t *skein_x = memory_allocate(0, sizeof(SkeinCtx_t), 0, 0);
    if (skein_x == NULL) {
        rc = SESHAT_OUT_OF_MEMORY;
        log_rc_msg("skein_hash::memory_allocate", rc);
        goto exit;
    }
    
    rc = skeinCtxPrepare(skein_x, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_hash::skeinCtxPrepare", rc);
        goto exit;
    }
    
    rc = skeinInit(skein_x, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_hash::skeinInit", rc);
        goto exit;
    }
    
    rc = skeinUpdate(skein_x, input, in_len);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_hash::skeinUpdate", rc);
        goto exit;
    }
    
    rc = skeinFinal(skein_x, output);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_hash::skeinFinal", rc);
        goto exit;
    }

exit:
    memory_deallocate(skein_x);
    return rc;
}

rc_t skein_mac(const unsigned char *skein_mac_key, const unsigned char *input,
               size_t in_len, unsigned char *output) {
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(output != NULL);
    FOUNDATION_ASSERT(skein_mac_key != NULL);
    FOUNDATION_ASSERT(in_len > 0);

    rc_t rc = SESHAT_SUCCESS;

    SkeinCtx_t *skein_x = memory_allocate(0, sizeof(SkeinCtx_t), 0, 0);
    if (skein_x == NULL) {
        rc = SESHAT_OUT_OF_MEMORY;
        log_rc_msg("skein_mac::memory_allocate", rc);
        goto exit;
    }
    
    rc = skeinCtxPrepare(skein_x, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac::skeinCtxPrepare", rc);
        goto exit;
    }
   
    rc = skeinMacInit(skein_x, skein_mac_key, SKEIN_MAC_KEY_LEN, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac::skeinMacInit", rc);
        goto exit;
    }

    rc = skeinUpdate(skein_x, input, in_len);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac::skeinUpdate", rc);
        goto exit;
    }
   
    rc = skeinFinal(skein_x, output);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac::skeinFinal", rc);
        goto exit;
    }

exit:
    memory_deallocate(skein_x);
    return rc;
}

rc_t skein_mac_init(SkeinCtx_t *skein_x, const unsigned char *skein_mac_key) {
    FOUNDATION_ASSERT(skein_mac_key != NULL);
    FOUNDATION_ASSERT(skein_x != NULL);

    rc_t rc = SESHAT_SUCCESS;

    rc = skeinCtxPrepare(skein_x, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac_init::skeinCtxPrepare", rc);
        goto exit;
    }
    
    rc = skeinMacInit(skein_x, skein_mac_key, SKEIN_MAC_KEY_LEN, Skein512);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac_init::skeinMacInit", rc);
        goto exit;
    }

exit:
    return rc;
}

rc_t skein_mac_update(SkeinCtx_t *skein_x, const unsigned char *input,
                      size_t in_len) {
    FOUNDATION_ASSERT(input != NULL);
    FOUNDATION_ASSERT(skein_x != NULL);
    FOUNDATION_ASSERT(in_len > 0);

    rc_t rc = SESHAT_SUCCESS;
    rc = skeinUpdate(skein_x, input, in_len);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac_update::skeinUpdate", rc);
    }
    
    return rc;
}

rc_t skein_mac_final(SkeinCtx_t *skein_x, unsigned char *output) {
    FOUNDATION_ASSERT(skein_x != NULL);
    FOUNDATION_ASSERT(output != NULL);

    rc_t rc = SESHAT_SUCCESS;
    rc = skeinFinal(skein_x, output);
    if (rc != SESHAT_SUCCESS) {
        rc = SESHAT_SKEIN_ERR;
        log_rc_msg("skein_mac_final::skeinFinal", rc);
    }
    
    skeinReset(skein_x);
    return rc;
}
