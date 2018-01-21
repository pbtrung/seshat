#ifndef __RC_H__
#define __RC_H__

enum rc_t {
    SESHAT_SUCCESS = 0,
    SESHAT_OUT_OF_MEMORY = -1,
    SESHAT_SKEIN_ERR = -2,
    SESHAT_SODIUM_ERR = -3,
    SESHAT_ARGON2_ERR = -4,
    SESHAT_CTR_HC256_USED_ERR = -5,
};
typedef enum rc_t rc_t;

const char *rc_msg(rc_t rc);
void log_rc_msg(const char *func, rc_t rc);

#endif //__RC_H__
