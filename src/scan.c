#include "internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char tag[16];
    uint8_t tag_len;
    uint8_t count;
} mdf_cc;

typedef struct {
    char tag[16];
    uint8_t tag_len;
    size_t outer_start;
    uint32_t path_off;  // Index into the map's keys arena.
    uint32_t path_len;
    mdf_cc *cc;
    size_t cc_len;
    size_t cc_cap;
} mdf_frame;

static uint8_t nth_of_type(mdf_frame *f, const char tag[16], size_t tlen)
{
    assert(f != NULL);
    assert(tlen <= 16);

    for (size_t i = 0; i < f->cc_len; i++) {
        if (f->cc[i].tag_len == tlen && memcmp(f->cc[i].tag, tag, tlen) == 0) {
            if (f->cc[i].count < 255) {  // Saturate; uint8_t.
                f->cc[i].count++;
            }
            return f->cc[i].count;
        }
    }
    if (f->cc_len == f->cc_cap) {
        size_t nc = f->cc_cap ? f->cc_cap * 2 : 4;
        f->cc = (mdf_cc *) realloc(f->cc, nc * sizeof(mdf_cc));
        assert(f->cc != NULL);
        f->cc_cap = nc;
    }
    memcpy(f->cc[f->cc_len].tag, tag, 16);
    f->cc[f->cc_len].tag_len = (uint8_t) tlen;
    f->cc[f->cc_len].count = 1;
    f->cc_len++;
    return 1;
}

static void count_in_parent(mdf_frame *stack, size_t depth, const char tag[16], size_t tlen)
{
    if (depth > 0) {
        nth_of_type(&stack[depth - 1], tag, tlen);
    }
}

// Id anchors are shorter and survive re-renders that shift positions.
static int anchor_selector(mdf_arena *keys,
                           const char *attrs,
                           size_t alen,
                           uint32_t *out_off,
                           uint32_t *out_len)
{
    assert(keys != NULL);
    assert(out_off != NULL);
    assert(out_len != NULL);

    size_t idl;
    const char *idv = mdf_attr_value(attrs, alen, "id", &idl);
    if (!idv || idl == 0) {
        return 0;
    }
    mdf_arena_reserve(keys, idl + 1);
    uint32_t off = (uint32_t) keys->len;
    char *dst = keys->base + off;
    dst[0] = '#';
    memcpy(dst + 1, idv, idl);
    keys->len += idl + 1;
    *out_off = off;
    *out_len = (uint32_t) (idl + 1);
    return 1;
}

static void build_path(mdf_arena *keys,
                       mdf_frame *stack,
                       size_t depth,
                       const char tag[16],
                       size_t tlen,
                       const char *attrs,
                       size_t alen,
                       uint32_t *out_off,
                       uint32_t *out_len)
{
    assert(keys != NULL);
    assert(tlen > 0 && tlen <= 16);
    assert(out_off != NULL);
    assert(out_len != NULL);

    if (anchor_selector(keys, attrs, alen, out_off, out_len)) {
        count_in_parent(stack, depth, tag, tlen);  // Keep sibling nth correct.
        return;
    }

    uint8_t nth = depth ? nth_of_type(&stack[depth - 1], tag, tlen) : 1;
    uint32_t parent_off = depth ? stack[depth - 1].path_off : 0;
    uint32_t parent_len = depth ? stack[depth - 1].path_len : 0;

    mdf_arena_reserve(keys, parent_len + tlen + 24);  // Stabilize base.
    uint32_t off = (uint32_t) keys->len;
    char *dst = keys->base + off;
    size_t pos = 0;
    if (parent_len) {
        memcpy(dst, keys->base + parent_off, parent_len);
        pos = parent_len;
        memcpy(dst + pos, " > ", 3);
        pos += 3;
    }
    memcpy(dst + pos, tag, tlen);
    pos += tlen;
    if (nth > 1) {
        memcpy(dst + pos, ":nth-of-type(", 13);
        pos += 13;
        char buf[4];
        int n = snprintf(buf, sizeof buf, "%u", (unsigned) nth);
        assert(n > 0 && (size_t) n < sizeof buf);
        memcpy(dst + pos, buf, (size_t) n);
        pos += (size_t) n;
        dst[pos++] = ')';
    }
    keys->len += pos;
    *out_off = off;
    *out_len = (uint32_t) pos;
}

static size_t skip_comment(const char *p, size_t n)
{
    assert(p != NULL || n == 0);
    if (n < 4) {
        return n;
    }
    const char *e = mdf_memmem(p + 4, n - 4, "-->", 3);
    return e ? (size_t) (e - p) + 3 : n;
}

// script/style/textarea: inner bytes are not parsed as HTML.
static size_t
find_raw_end(const char *bytes, size_t blen, size_t start, const char *tag, size_t tlen)
{
    assert(bytes != NULL || blen == 0);
    assert(start <= blen);
    assert(tlen > 0 && tlen <= 16);

    size_t i = start;
    while (i < blen) {
        const char *lt = memchr(bytes + i, '<', blen - i);
        if (!lt) {
            return blen;
        }
        i = (size_t) (lt - bytes);
        if (i + 1 < blen && bytes[i + 1] == '/') {
            size_t ns = i + 2;
            size_t nl = mdf_tag_name_len(bytes + ns, blen - ns);
            if (nl == tlen) {
                int eq = 1;
                for (size_t k = 0; k < nl; k++) {
                    if (mdf_tolower((unsigned char) bytes[ns + k]) != tag[k]) {
                        eq = 0;
                        break;
                    }
                }
                if (eq) {
                    const char *gt = memchr(bytes + i, '>', blen - i);
                    return gt ? (size_t) (gt - bytes) + 1 : blen;
                }
            }
        }
        i++;
    }
    return blen;
}

static size_t handle_close_tag(const char *html,
                               size_t hlen,
                               size_t i,
                               mdf_map *out,
                               mdf_frame *stack,
                               size_t *depth)
{
    assert(html != NULL);
    assert(depth != NULL);
    assert(i + 1 < hlen);
    assert(html[i] == '<' && html[i + 1] == '/');

    size_t ns = i + 2;
    size_t tlen = mdf_tag_name_len(html + ns, hlen - ns);
    const char *gtp = memchr(html + i, '>', hlen - i);
    size_t gt = gtp ? (size_t) (gtp - html) : hlen - 1;
    size_t close_end = gt + 1;

    long fidx = -1;
    for (long d = (long) *depth - 1; d >= 0; d--) {
        if (stack[d].tag_len != tlen) {
            continue;
        }
        int eq = 1;
        for (size_t k = 0; k < tlen; k++) {
            if (mdf_tolower((unsigned char) html[ns + k]) != stack[d].tag[k]) {
                eq = 0;
                break;
            }
        }
        if (eq) {
            fidx = d;
            break;
        }
    }
    if (fidx >= 0) {
        mdf_frame *f = &stack[fidx];
        mdf_map_put_off(
            out, f->path_off, f->path_len, (uint32_t) f->outer_start, (uint32_t) close_end);
        free(f->cc);
        // Drop frames opened under an unclosed ancestor.
        for (size_t d = (size_t) fidx; d + 1 < *depth; d++) {
            stack[d] = stack[d + 1];
        }
        (*depth)--;
    }
    return close_end;
}

static size_t handle_bang(const char *html, size_t hlen, size_t i)
{
    assert(html != NULL);
    assert(i + 1 < hlen);
    assert(html[i] == '<' && html[i + 1] == '!');

    if (i + 3 < hlen && html[i + 2] == '-' && html[i + 3] == '-') {
        return i + skip_comment(html + i, hlen - i);
    }
    const char *gtp = memchr(html + i, '>', hlen - i);
    return gtp ? (size_t) (gtp - html) + 1 : hlen;
}

static size_t handle_open_tag(const char *html,
                              size_t hlen,
                              size_t i,
                              mdf_map *out,
                              mdf_arena *keys,
                              mdf_frame **stack,
                              size_t *depth,
                              size_t *cap)
{
    assert(html != NULL);
    assert(i < hlen);
    assert(mdf_isalpha((unsigned char) html[i + 1]));
    assert(depth != NULL && cap != NULL);

    size_t outer = i;
    size_t ns = i + 1;
    size_t raw_tlen = mdf_tag_name_len(html + ns, hlen - ns);
    size_t ne = ns + raw_tlen;
    const char *gtp = memchr(html + i, '>', hlen - i);
    size_t gt = gtp ? (size_t) (gtp - html) : hlen - 1;
    size_t tag_end = gt + 1;
    int self_closing = (gt > 0 && html[gt - 1] == '/');

    char buf[16];
    size_t tlen = mdf_normalize_tag(html + ns, raw_tlen, buf);
    const char *attrs = html + ne;
    size_t alen = (gt > ne) ? gt - ne : 0;

    if (self_closing || mdf_is_void_tag(buf, tlen)) {
        count_in_parent(*stack, *depth, buf, tlen);
        uint32_t sel_off, sel_len;
        if (anchor_selector(keys, attrs, alen, &sel_off, &sel_len)) {
            mdf_map_put_off(out, sel_off, sel_len, (uint32_t) outer, (uint32_t) tag_end);
        }
        return tag_end;
    }

    if (mdf_is_raw_tag(buf, tlen)) {
        uint32_t path_off, path_len;
        build_path(keys, *stack, *depth, buf, tlen, attrs, alen, &path_off, &path_len);
        size_t close = find_raw_end(html, hlen, tag_end, buf, tlen);
        mdf_map_put_off(out, path_off, path_len, (uint32_t) outer, (uint32_t) close);
        return close;
    }

    uint32_t path_off, path_len;
    build_path(keys, *stack, *depth, buf, tlen, attrs, alen, &path_off, &path_len);
    if (*depth == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *stack = (mdf_frame *) realloc(*stack, *cap * sizeof(mdf_frame));
        assert(*stack != NULL);
    }
    mdf_frame *f = &(*stack)[*depth];
    memcpy(f->tag, buf, 16);
    f->tag_len = (uint8_t) tlen;
    f->outer_start = outer;
    f->path_off = path_off;
    f->path_len = path_len;
    f->cc = NULL;
    f->cc_len = 0;
    f->cc_cap = 0;
    (*depth)++;
    return tag_end;
}

void mdf_scan(const char *html, size_t hlen, mdf_map *out)
{
    assert(html != NULL || hlen == 0);
    assert(out != NULL);
    assert(hlen <= UINT32_MAX);  // uint32_t offsets.

    mdf_frame *stack = NULL;
    size_t depth = 0;
    size_t cap = 0;
    size_t i = 0;
    mdf_arena *keys = &out->keys;

    while (i < hlen) {
        const char *lt = memchr(html + i, '<', hlen - i);
        if (!lt) {
            break;
        }
        i = (size_t) (lt - html);
        if (i + 1 >= hlen) {
            break;
        }

        unsigned char nc = (unsigned char) html[i + 1];

        if (nc == '/') {
            i = handle_close_tag(html, hlen, i, out, stack, &depth);
        } else if (nc == '!') {
            i = handle_bang(html, hlen, i);
        } else if (mdf_isalpha(nc)) {
            i = handle_open_tag(html, hlen, i, out, keys, &stack, &depth, &cap);
        } else {
            i++;
        }
    }

    // Unclosed at EOF: free child-count arrays; emit no entry.
    for (size_t d = 0; d < depth; d++) {
        free(stack[d].cc);
    }
    free(stack);
}
