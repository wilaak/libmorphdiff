#include "internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void mdf_arena_init(mdf_arena *a)
{
    assert(a != NULL);
    a->base = NULL;
    a->len = 0;
    a->cap = 0;
}

void mdf_arena_free(mdf_arena *a)
{
    assert(a != NULL);
    free(a->base);
    a->base = NULL;
    a->len = a->cap = 0;
}

void mdf_arena_reserve(mdf_arena *a, size_t extra)
{
    assert(a != NULL);
    assert(a->len <= a->cap);
    if (a->len + extra <= a->cap) {
        return;
    }

    size_t nc = a->cap ? a->cap : 256;
    while (nc < a->len + extra) {
        nc *= 2;
    }
    a->base = (char *) realloc(a->base, nc);
    assert(a->base != NULL);
    a->cap = nc;
    assert(a->len + extra <= a->cap);
}

uint32_t mdf_arena_putn(mdf_arena *a, const char *src, size_t n)
{
    assert(a != NULL);
    assert(src != NULL || n == 0);
    assert(a->len + n <= UINT32_MAX);  // uint32_t offsets.

    mdf_arena_reserve(a, n);
    uint32_t off = (uint32_t) a->len;
    if (n) {
        memcpy(a->base + a->len, src, n);
    }
    a->len += n;
    return off;
}

const char *mdf_memmem(const char *hay, size_t hlen, const char *needle, size_t nlen)
{
    assert(hay != NULL || hlen == 0);
    assert(needle != NULL || nlen == 0);

    if (nlen == 0) {
        return hay;
    }
    if (hlen < nlen) {
        return NULL;
    }
    char first = needle[0];
    size_t end = hlen - nlen + 1;
    for (size_t i = 0; i < end;) {
        const char *p = memchr(hay + i, first, end - i);
        if (!p) {
            return NULL;
        }
        if (memcmp(p, needle, nlen) == 0) {
            return p;
        }
        i = (size_t) (p - hay) + 1;
    }
    return NULL;
}
