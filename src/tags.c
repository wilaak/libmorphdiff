#include "internal.h"

#include <assert.h>
#include <string.h>

size_t mdf_tag_name_len(const char *p, size_t n)
{
    assert(p != NULL || n == 0);
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char) p[i];
        if (!(mdf_isalnum(c) || c == '-')) {
            break;
        }
        i++;
    }
    assert(i <= n);
    return i;
}

size_t mdf_normalize_tag(const char *src, size_t src_len, char dst[16])
{
    assert(src != NULL || src_len == 0);
    assert(dst != NULL);
    size_t n = src_len < 16 ? src_len : 16;  // Frame stores tag inline at 16.
    for (size_t i = 0; i < n; i++) {
        dst[i] = (char) mdf_tolower((unsigned char) src[i]);
    }
    return n;
}

// `t` must already be lowercased.
static int tag_eq_lit(const char *t, size_t n, const char *lit, size_t l)
{
    return n == l && memcmp(t, lit, n) == 0;
}

int mdf_is_void_tag(const char *t, size_t n)
{
    assert(t != NULL || n == 0);
    switch (n) {
        case 2:
            return tag_eq_lit(t, n, "br", 2) || tag_eq_lit(t, n, "hr", 2);
        case 3:
            return tag_eq_lit(t, n, "col", 3) || tag_eq_lit(t, n, "img", 3) ||
                   tag_eq_lit(t, n, "wbr", 3);
        case 4:
            return tag_eq_lit(t, n, "area", 4) || tag_eq_lit(t, n, "base", 4) ||
                   tag_eq_lit(t, n, "link", 4) || tag_eq_lit(t, n, "meta", 4);
        case 5:
            return tag_eq_lit(t, n, "embed", 5) || tag_eq_lit(t, n, "input", 5) ||
                   tag_eq_lit(t, n, "param", 5) || tag_eq_lit(t, n, "track", 5);
        case 6:
            return tag_eq_lit(t, n, "source", 6);
        default:
            return 0;
    }
}

int mdf_is_raw_tag(const char *t, size_t n)
{
    assert(t != NULL || n == 0);
    switch (n) {
        case 5:
            return tag_eq_lit(t, n, "style", 5);
        case 6:
            return tag_eq_lit(t, n, "script", 6);
        case 8:
            return tag_eq_lit(t, n, "textarea", 8);
        default:
            return 0;
    }
}

const char *mdf_attr_value(const char *attrs, size_t alen, const char *name, size_t *out_len)
{
    assert(attrs != NULL || alen == 0);
    assert(name != NULL);
    assert(out_len != NULL);

    size_t name_len = strlen(name);
    size_t i = 0;
    while (i < alen) {
        while (i < alen && mdf_isspace((unsigned char) attrs[i])) {
            i++;
        }
        size_t ns = i;
        while (i < alen && !mdf_isspace((unsigned char) attrs[i]) && attrs[i] != '=') {
            i++;
        }
        size_t an = i - ns;
        while (i < alen && mdf_isspace((unsigned char) attrs[i])) {
            i++;
        }

        int has_value = (i < alen && attrs[i] == '=');
        const char *vp = NULL;
        size_t vl = 0;
        if (has_value) {
            i++;
            while (i < alen && mdf_isspace((unsigned char) attrs[i])) {
                i++;
            }
            if (i < alen && (attrs[i] == '"' || attrs[i] == '\'')) {
                char q = attrs[i++];
                size_t s = i;
                while (i < alen && attrs[i] != q) {
                    i++;
                }
                vp = attrs + s;
                vl = i - s;
                if (i < alen) {
                    i++;
                }
            } else {
                size_t s = i;
                while (i < alen && !mdf_isspace((unsigned char) attrs[i])) {
                    i++;
                }
                vp = attrs + s;
                vl = i - s;
            }
        }

        if (an == name_len) {
            int eq = 1;
            for (size_t k = 0; k < an; k++) {
                if (mdf_tolower((unsigned char) attrs[ns + k]) != name[k]) {
                    eq = 0;
                    break;
                }
            }
            if (eq) {
                if (has_value) {
                    *out_len = vl;
                    return vp;
                }
                *out_len = 0;
                return attrs + ns;
            }
        }
    }
    return NULL;
}
