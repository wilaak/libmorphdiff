#ifndef MDF_INTERNAL_H
#define MDF_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *base;
    size_t len;
    size_t cap;
} mdf_arena;

void mdf_arena_init(mdf_arena *a);
void mdf_arena_free(mdf_arena *a);
// Any pointer into a->base is invalidated by a subsequent put/append
// unless reserved up front.
void mdf_arena_reserve(mdf_arena *a, size_t extra);
uint32_t mdf_arena_putn(mdf_arena *a, const char *src, size_t n);

static inline int mdf_tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}
static inline int mdf_isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static inline int mdf_isalpha(int c)
{
    c = mdf_tolower(c);
    return c >= 'a' && c <= 'z';
}
static inline int mdf_isalnum(int c)
{
    return mdf_isalpha(c) || (c >= '0' && c <= '9');
}

const char *mdf_memmem(const char *hay, size_t hlen, const char *needle, size_t nlen);

size_t mdf_tag_name_len(const char *p, size_t n);
size_t mdf_normalize_tag(const char *src, size_t src_len, char dst[16]);

int mdf_is_void_tag(const char *t, size_t n);
int mdf_is_raw_tag(const char *t, size_t n);

// NULL on miss. Valueless attribute returns non-NULL with *out_len = 0.
const char *mdf_attr_value(const char *attrs, size_t alen, const char *name, size_t *out_len);

// 24 bytes; two entries per 64B cache line.
typedef struct {
    uint32_t key_off;  // Offset into m->keys.
    uint32_t key_len;
    uint32_t start;
    uint32_t end;
    uint32_t hash;
    uint32_t occupied;  // 4B to keep the struct 4-aligned.
} mdf_entry;

typedef struct {
    mdf_entry *e;
    size_t cap;  // Power of two; index by mask, not mod.
    size_t len;
    mdf_arena keys;
} mdf_map;

void mdf_map_init(mdf_map *m, size_t hint);
void mdf_map_reset(mdf_map *m);
// Key must already be written at m->keys[off, off+len).
void mdf_map_put_off(mdf_map *m, uint32_t key_off, uint32_t key_len, uint32_t start, uint32_t end);
mdf_entry *mdf_map_get(mdf_map *m, const char *key, size_t key_len);
void mdf_map_free(mdf_map *m);

static inline const char *mdf_map_key(const mdf_map *m, const mdf_entry *e)
{
    return m->keys.base + e->key_off;
}

void mdf_scan(const char *html, size_t hlen, mdf_map *out);

#endif  // MDF_INTERNAL_H
