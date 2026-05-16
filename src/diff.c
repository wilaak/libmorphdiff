#include "internal.h"
#include "morphdiff.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t key_off;
    uint32_t key_len;
    uint32_t html_off;  // Index into new_html.
    uint32_t html_len;
} mdf_iop;

typedef struct {
    mdf_iop *data;
    size_t len;
    size_t cap;
} mdf_iop_vec;

static void iop_push(mdf_iop_vec *v, mdf_iop op)
{
    assert(v != NULL);
    assert(v->len <= v->cap);

    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->data = (mdf_iop *) realloc(v->data, v->cap * sizeof(mdf_iop));
        assert(v->data != NULL);
    }
    v->data[v->len++] = op;
}

static int iop_doc_order_cmp(const void *a, const void *b)
{
    const mdf_iop *x = (const mdf_iop *) a;
    const mdf_iop *y = (const mdf_iop *) b;
    if (x->html_off != y->html_off) {
        return (x->html_off > y->html_off) - (x->html_off < y->html_off);
    }
    return (x->html_len < y->html_len) - (x->html_len > y->html_len);
}

// Containment, not selector strings: `#x` shares no prefix with its
// path-only ancestor `html > body`.
static void prune_ancestors(mdf_iop_vec *v)
{
    assert(v != NULL);
    if (v->len < 2) {
        return;
    }
    qsort(v->data, v->len, sizeof(mdf_iop), iop_doc_order_cmp);

    char *keep = (char *) malloc(v->len);
    assert(keep != NULL);
    memset(keep, 1, v->len);
    for (size_t i = 0; i < v->len; i++) {
        uint32_t a_lo = v->data[i].html_off;
        uint32_t a_hi = a_lo + v->data[i].html_len;
        for (size_t j = 0; j < v->len; j++) {
            if (j == i) {
                continue;
            }
            uint32_t b_lo = v->data[j].html_off;
            uint32_t b_hi = b_lo + v->data[j].html_len;
            if (a_lo <= b_lo && a_hi >= b_hi && (a_lo < b_lo || a_hi > b_hi)) {
                keep[i] = 0;
                break;
            }
        }
    }
    size_t w = 0;
    for (size_t r = 0; r < v->len; r++) {
        if (keep[r]) {
            v->data[w++] = v->data[r];
        }
    }
    assert(w <= v->len);
    v->len = w;
    free(keep);
}

static void collect_ops(mdf_iop_vec *ops,
                        const char *old_html,
                        mdf_map *old_map,
                        const char *new_html,
                        mdf_map *new_map)
{
    assert(ops != NULL && ops->len == 0);
    assert(old_map != NULL && new_map != NULL);

    const char *new_keys = new_map->keys.base;
    for (size_t k = 0; k < new_map->cap; k++) {
        if (!new_map->e[k].occupied) {
            continue;
        }
        mdf_entry *n = &new_map->e[k];
        const char *nk = new_keys + n->key_off;
        mdf_entry *o = mdf_map_get(old_map, nk, n->key_len);
        if (!o) {
            continue;
        }

        uint32_t olen = o->end - o->start;
        uint32_t nlen = n->end - n->start;
        if (olen == nlen && memcmp(old_html + o->start, new_html + n->start, olen) == 0) {
            continue;
        }

        mdf_iop op;
        op.key_off = n->key_off;
        op.key_len = n->key_len;
        op.html_off = n->start;
        op.html_len = nlen;
        iop_push(ops, op);
    }
}

static mdf_diff materialize(const mdf_iop_vec *ops, const char *new_keys)
{
    assert(ops != NULL);

    size_t sel_total = 0;
    for (size_t i = 0; i < ops->len; i++) {
        sel_total += ops->data[i].key_len;
    }

    mdf_diff r;
    r.count = ops->len;
    r.ops = ops->len ? (mdf_op *) malloc(ops->len * sizeof(mdf_op)) : NULL;
    r.selectors = sel_total ? (char *) malloc(sel_total) : NULL;
    r.selectors_len = sel_total;
    assert(ops->len == 0 || r.ops != NULL);
    assert(sel_total == 0 || r.selectors != NULL);

    uint32_t sel_pos = 0;
    for (size_t i = 0; i < ops->len; i++) {
        uint32_t kl = ops->data[i].key_len;
        memcpy(r.selectors + sel_pos, new_keys + ops->data[i].key_off, kl);
        r.ops[i].selector_off = sel_pos;
        r.ops[i].selector_len = kl;
        r.ops[i].html_off = ops->data[i].html_off;
        r.ops[i].html_len = ops->data[i].html_len;
        sel_pos += kl;
    }
    assert(sel_pos == sel_total);
    return r;
}

static mdf_diff
diff_maps(const char *old_html, mdf_map *old_map, const char *new_html, mdf_map *new_map)
{
    assert(old_map != NULL);
    assert(new_map != NULL);

    mdf_iop_vec ops = {NULL, 0, 0};
    collect_ops(&ops, old_html, old_map, new_html, new_map);
    prune_ancestors(&ops);
    mdf_diff r = materialize(&ops, new_map->keys.base);
    free(ops.data);
    return r;
}

mdf_diff mdf_compare(const char *old_html, size_t old_len, const char *new_html, size_t new_len)
{
    assert(old_html != NULL || old_len == 0);
    assert(new_html != NULL || new_len == 0);
    assert(old_len <= UINT32_MAX);
    assert(new_len <= UINT32_MAX);

    mdf_map old_map, new_map;
    mdf_map_init(&old_map, old_len / 40);
    mdf_scan(old_html, old_len, &old_map);
    mdf_map_init(&new_map, new_len / 40);
    mdf_scan(new_html, new_len, &new_map);

    mdf_diff r = diff_maps(old_html, &old_map, new_html, &new_map);

    mdf_map_free(&old_map);
    mdf_map_free(&new_map);
    return r;
}

void mdf_diff_free(mdf_diff *r)
{
    if (!r) {
        return;
    }
    free(r->ops);
    free(r->selectors);
    r->ops = NULL;
    r->selectors = NULL;
    r->count = 0;
    r->selectors_len = 0;
}

struct mdf_view {
    char *html[2];
    size_t html_cap[2];
    size_t html_len[2];
    mdf_map map[2];
    int cur;  // Current snapshot index. Scratch is 1 - cur.
};

static void slot_init(mdf_view *v, int s, size_t hint_len)
{
    assert(v != NULL);
    assert(s == 0 || s == 1);
    v->html_cap[s] = hint_len ? hint_len : 1;
    v->html[s] = (char *) malloc(v->html_cap[s]);
    assert(v->html[s] != NULL);
    v->html_len[s] = 0;
    mdf_map_init(&v->map[s], hint_len / 40);
}

mdf_view *mdf_view_new(const char *html, size_t len)
{
    assert(html != NULL || len == 0);
    assert(len <= UINT32_MAX);

    mdf_view *v = (mdf_view *) malloc(sizeof *v);
    assert(v != NULL);
    v->cur = 0;

    slot_init(v, 0, len);
    if (len) {
        memcpy(v->html[0], html, len);
    }
    v->html_len[0] = len;
    mdf_scan(v->html[0], v->html_len[0], &v->map[0]);

    slot_init(v, 1, len);
    if (v->map[1].cap < v->map[0].cap) {
        free(v->map[1].e);
        v->map[1].e = (mdf_entry *) calloc(v->map[0].cap, sizeof(mdf_entry));
        assert(v->map[1].e != NULL);
        v->map[1].cap = v->map[0].cap;
    }
    return v;
}

mdf_diff mdf_view_update(mdf_view *v, const char *new_html, size_t new_len)
{
    assert(v != NULL);
    assert(new_html != NULL || new_len == 0);
    assert(new_len <= UINT32_MAX);
    assert(v->cur == 0 || v->cur == 1);

    int next = v->cur ^ 1;

    if (v->html_cap[next] < new_len) {
        v->html[next] = (char *) realloc(v->html[next], new_len);
        assert(v->html[next] != NULL);
        v->html_cap[next] = new_len;
    }
    if (new_len) {
        memcpy(v->html[next], new_html, new_len);
    }
    v->html_len[next] = new_len;

    mdf_map_reset(&v->map[next]);
    mdf_scan(v->html[next], v->html_len[next], &v->map[next]);

    mdf_diff r = diff_maps(v->html[v->cur], &v->map[v->cur], new_html, &v->map[next]);

    v->cur = next;
    return r;
}

void mdf_view_free(mdf_view *v)
{
    if (!v) {
        return;
    }
    free(v->html[0]);
    free(v->html[1]);
    mdf_map_free(&v->map[0]);
    mdf_map_free(&v->map[1]);
    free(v);
}
