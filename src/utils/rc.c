#include "utils/rc.h"
#include "foundation/foundation.h"

const char *rc_msg(rc_t rc) {
    switch (rc) {
    case SESHAT_SUCCESS:
        return "SESHAT_SUCCESS";
    case SESHAT_OUT_OF_MEMORY:
        return "SESHAT_OUT_OF_MEMORY";
    case SESHAT_SKEIN_ERR:
        return "SESHAT_SKEIN_ERR";
    case SESHAT_SODIUM_ERR:
        return "SESHAT_SODIUM_ERR";
    case SESHAT_ARGON2_ERR:
        return "SESHAT_ARGON2_ERR";
    case SESHAT_CTR_HC256_USED_ERR:
        return "SESHAT_CTR_HC256_USED_ERR";
    default:
        return "Unknown return code";
    }
}

void log_rc_msg(const char *func, rc_t rc) {
	log_errorf(0, rc, STRING_CONST("%s::%s"), func, rc_msg(rc));
}