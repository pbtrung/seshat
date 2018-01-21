#ifndef __PBKDF2_H__
#define __PBKDF2_H__

#include <stdint.h>
#include <stdlib.h>

void pbkdf2(const uint8_t *password, size_t password_len, const uint8_t *salt,
            size_t salt_len, uint64_t N, uint8_t *out, size_t bytes);

#endif //__PBKDF2_H__