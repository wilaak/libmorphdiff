#include "morph_diff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *slurp(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t) sz);
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz) {
        fprintf(stderr, "short read on %s\n", path);
        exit(1);
    }
    fclose(f);
    *out_len = (size_t) sz;
    return buf;
}

static void print_trimmed(const char *p, size_t n)
{
    size_t s = 0, e = n;
    while (s < e && (p[s] == ' ' || p[s] == '\t' || p[s] == '\n' || p[s] == '\r')) {
        s++;
    }
    while (e > s && (p[e - 1] == ' ' || p[e - 1] == '\t' || p[e - 1] == '\n' || p[e - 1] == '\r')) {
        e--;
    }
    fwrite(p + s, 1, e - s, stdout);
}

int main(void)
{
    size_t old_len, new_len;
    char *old_html = slurp("data/demo_old.html", &old_len);
    char *new_html = slurp("data/demo_new.html", &new_len);

    mdf_diff r = mdf_compare(old_html, old_len, new_html, new_len);
    printf("%zu op(s)\n\n", r.count);
    for (size_t i = 0; i < r.count; i++) {
        const mdf_op *op = &r.ops[i];
        printf("  selector : %.*s\n", (int) op->selector_len, r.selectors + op->selector_off);
        printf("  html     : ");
        print_trimmed(new_html + op->html_off, op->html_len);
        printf("\n\n");
    }
    mdf_diff_free(&r);
    free(old_html);
    free(new_html);
    return 0;
}
