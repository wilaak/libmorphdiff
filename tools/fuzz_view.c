#include "morphdiff.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) {
        return 0;
    }

    uint8_t updates = (data[0] & 0x0F) + 1;
    data += 1;
    size -= 1;

    size_t chunk = size / (updates + 1u);
    if (chunk == 0) {
        chunk = 1;
    }

    mdf_view *v = mdf_view_new((const char *) data, chunk < size ? chunk : size);
    size_t off = chunk;
    for (uint8_t i = 0; i < updates && off < size; i++) {
        size_t take = chunk;
        if (off + take > size) {
            take = size - off;
        }
        mdf_diff r = mdf_view_update(v, (const char *) data + off, take);
        mdf_diff_free(&r);
        off += take;
    }
    mdf_view_free(v);
    return 0;
}
