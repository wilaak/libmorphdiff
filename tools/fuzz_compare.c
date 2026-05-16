#include "morphdiff.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return 0;
    }

    uint32_t split;
    memcpy(&split, data, 4);
    data += 4;
    size -= 4;
    split = (uint32_t) (split % (size + 1));

    const char *old_html = (const char *) data;
    const char *new_html = (const char *) data + split;
    size_t old_len = split;
    size_t new_len = size - split;

    mdf_diff r = mdf_compare(old_html, old_len, new_html, new_len);
    mdf_diff_free(&r);
    return 0;
}
