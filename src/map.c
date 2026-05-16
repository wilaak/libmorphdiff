#include "internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a(const char *s, size_t n)
{
    assert(s != NULL || n == 0);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint8_t) s[i];
        h *= 16777619u;
    }
    return h;
}

void mdf_map_init(mdf_map *m, size_t hint)
{
    assert(m != NULL);
    size_t cap = 64;
    while (cap < hint * 2) {
        cap <<= 1;
    }
    m->e = (mdf_entry *) calloc(cap, sizeof(mdf_entry));
    assert(m->e != NULL);
    m->cap = cap;
    m->len = 0;
    mdf_arena_init(&m->keys);
    assert((m->cap & (m->cap - 1)) == 0);  // Power of two for masking.
}

void mdf_map_reset(mdf_map *m)
{
    assert(m != NULL);
    if (m->cap) {
        memset(m->e, 0, m->cap * sizeof(mdf_entry));
    }
    m->len = 0;
    m->keys.len = 0;  // Rewind bump pointer; arena buffer kept.
    assert(m->len == 0);
}

static void map_grow(mdf_map *m)
{
    assert(m != NULL);
    assert(m->cap > 0);
    size_t new_cap = m->cap * 2;
    mdf_entry *ne = (mdf_entry *) calloc(new_cap, sizeof(mdf_entry));
    assert(ne != NULL);
    size_t mask = new_cap - 1;
    for (size_t k = 0; k < m->cap; k++) {
        if (!m->e[k].occupied) {
            continue;
        }
        size_t i = m->e[k].hash & mask;
        while (ne[i].occupied) {
            i = (i + 1) & mask;
        }
        ne[i] = m->e[k];
    }
    free(m->e);
    m->e = ne;
    m->cap = new_cap;
    assert((m->cap & (m->cap - 1)) == 0);
}

void mdf_map_put_off(mdf_map *m, uint32_t key_off, uint32_t key_len, uint32_t start, uint32_t end)
{
    assert(m != NULL);
    assert(key_off + key_len <= m->keys.len);
    assert(start <= end);

    if ((m->len + 1) * 2 > m->cap) {
        map_grow(m);
    }

    const char *key = m->keys.base + key_off;
    uint32_t h = fnv1a(key, key_len);
    size_t mask = m->cap - 1;
    size_t i = h & mask;
    while (m->e[i].occupied) {
        mdf_entry *e = &m->e[i];
        if (e->hash == h && e->key_len == key_len &&
            memcmp(m->keys.base + e->key_off, key, key_len) == 0) {
            // Duplicate key: overwrite span; new arena bytes are slack.
            e->start = start;
            e->end = end;
            return;
        }
        i = (i + 1) & mask;
    }
    m->e[i].key_off = key_off;
    m->e[i].key_len = key_len;
    m->e[i].start = start;
    m->e[i].end = end;
    m->e[i].hash = h;
    m->e[i].occupied = 1;
    m->len++;
    assert(m->len <= m->cap / 2 + 1);
}

mdf_entry *mdf_map_get(mdf_map *m, const char *key, size_t key_len)
{
    assert(m != NULL);
    assert(key != NULL || key_len == 0);

    if (m->cap == 0) {
        return NULL;
    }
    uint32_t h = fnv1a(key, key_len);
    size_t mask = m->cap - 1;
    size_t i = h & mask;
    while (m->e[i].occupied) {
        mdf_entry *e = &m->e[i];
        if (e->hash == h && e->key_len == key_len &&
            memcmp(m->keys.base + e->key_off, key, key_len) == 0) {
            return e;
        }
        i = (i + 1) & mask;
    }
    return NULL;
}

void mdf_map_free(mdf_map *m)
{
    assert(m != NULL);
    free(m->e);
    mdf_arena_free(&m->keys);
    m->e = NULL;
    m->cap = m->len = 0;
}
