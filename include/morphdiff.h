// libmorphdiff: HTML morph-diff library.
//
// Compares two HTML buffers and returns the (selector, html) byte spans
// needed to reconcile a live DOM via a client-side morph.
//
// Sizes bounded by UINT32_MAX. Caller's new_html must outlive the result;
// op.html_off indexes into it.

#ifndef MORPHDIFF_H
#define MORPHDIFF_H

#include <stddef.h>
#include <stdint.h>

// Pair of byte spans, each (offset, length). Not NUL-terminated.
// selector_off indexes r.selectors; html_off indexes caller's new_html.
typedef struct {
    uint32_t selector_off;
    uint32_t selector_len;
    uint32_t html_off;
    uint32_t html_len;
} mdf_op;

typedef struct {
    mdf_op *ops;
    size_t count;
    char *selectors;  // Packed selector bytes, owned by r.
    size_t selectors_len;
} mdf_diff;

void mdf_diff_free(mdf_diff *r);

// One-shot compare. A buffer may be NULL only if its length is zero.
mdf_diff mdf_compare(const char *old_html, size_t old_len, const char *new_html, size_t new_len);

// Incremental view. Holds the prior snapshot; each update returns the
// diff and folds new HTML in. Steady-state updates do not allocate.
typedef struct mdf_view mdf_view;

mdf_view *mdf_view_new(const char *html, size_t len);
mdf_diff mdf_view_update(mdf_view *v, const char *new_html, size_t new_len);
void mdf_view_free(mdf_view *v);

#endif  // MORPHDIFF_H
