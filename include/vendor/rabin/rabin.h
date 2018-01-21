#ifndef __RABIN_H__
#define __RABIN_H__

#include "foundation/foundation.h"

#define POLYNOMIAL 0x3DA3358B4DC173LL
#define POLYNOMIAL_DEGREE 53
#define WINSIZE 64
#define AVERAGE_BITS 20

struct rabin_t {
    uint8_t window[WINSIZE];
    unsigned int wpos;
    unsigned int count;
    unsigned int pos;
    unsigned int start;
    uint64_t digest;
};
typedef struct rabin_t rabin_t;

struct chunk_t {
    unsigned int start;
    unsigned int length;
    uint64_t cut_fingerprint;
};
typedef struct chunk_t rabin_chunk_t;

extern rabin_chunk_t last_chunk;

rabin_t *rabin_init(void);
void rabin_reset(rabin_t *h);
void rabin_slide(rabin_t *h, uint8_t b);
void rabin_append(rabin_t *h, uint8_t b);
int rabin_next_chunk(rabin_t *h, uint8_t *buf, uint32_t len, uint32_t min_size,
                     uint32_t max_size);
rabin_chunk_t *rabin_finalize(rabin_t *h);
void rabin_deallocate(rabin_t *h);

#endif //__RABIN_H__