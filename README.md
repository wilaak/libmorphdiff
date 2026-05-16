# libmorphdiff

Just some testing for now; not meant to be used in its current state.

Diffs two HTML buffers and emits the minimal patches needed to morph a live DOM into the new state. Made for Datastar workloads with very high re-render rates to reduce client side load, pairs well with Brotli/Zstd or can even replace it in some cases.

## Usage

```c
#include "morphdiff.h"

//
// One-shot comparison
//

mdf_diff r = mdf_compare(old_html, old_len, new_html, new_len);
for (size_t i = 0; i < r.count; i++) {
    const mdf_op *op = &r.ops[i];
    // Neither span is NUL-terminated.
    // r.selectors + op->selector_off, length op->selector_len
    // new_html    + op->html_off,     length op->html_len
}
mdf_diff_free(&r);
```


```c
//
// Incremental patches
//

mdf_view *v = mdf_view_new(initial_html, initial_len);

for (;;) {
    size_t next_len;
    // const char *next = your_page_renderer(&next_len);
    const char next[] = "<html><body><p id=\"price\">42.10</p></body></html>";
    next_len = sizeof(next) - 1;

    mdf_diff r = mdf_view_update(v, next, next_len);

    // send_patches_to_client(r.ops, r.count);
    //
    // ^ do this or alternatively:
    //
    //for (size_t i = 0; i < r.count; i++) {
    //    const mdf_op *op = &r.ops[i];
    //    printf("patch %.*s -> %.*s\n",
    //           (int) op->selector_len, r.selectors + op->selector_off,
    //           (int) op->html_len,     next        + op->html_off);
    //}

    mdf_diff_free(&r);
}

mdf_view_free(v);
```

## Build

C99 no external dependencies. 

```
make       # builds the demo
make test  # builds and runs tests
make bench # builds the bench harness

include/ public header
src/     library sources + internal.h
tools/   demo, bench, tests
```

## Fuzz

```
make fuzz-compare  # libFuzzer + ASan + UBSan over mdf_compare
make fuzz-view     # same, over the streaming mdf_view API
```

Requires clang.