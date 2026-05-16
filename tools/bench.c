#define _POSIX_C_SOURCE 199309L

#include "morphdiff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECTIONS   400
#define ITERATIONS 2000
#define WARMUP     200

static char *buf_append(char *dst, size_t *len, size_t *cap, const char *src, size_t n)
{
    if (*len + n + 1 > *cap) {
        while (*len + n + 1 > *cap) {
            *cap = *cap ? *cap * 2 : 4096;
        }
        dst = realloc(dst, *cap);
        if (!dst) {
            perror("realloc");
            exit(1);
        }
    }
    memcpy(dst + *len, src, n);
    *len += n;
    dst[*len] = '\0';
    return dst;
}

static char *build_page(int mutate_every, size_t *out_len)
{
    char *buf = NULL;
    size_t len = 0, cap = 0;
    char tmp[512];

    const char *head = "<!DOCTYPE html><html><body>\n"
                       "<header id=\"top\"><h1>Bench</h1></header>\n"
                       "<main id=\"content\">\n";
    buf = buf_append(buf, &len, &cap, head, strlen(head));

    for (int i = 0; i < SECTIONS; i++) {
        int mutated = mutate_every > 0 && (i % mutate_every) == 0;
        int n = snprintf(tmp,
                         sizeof tmp,
                         "  <section id=\"s%d\">\n"
                         "    <h2>Section %d%s</h2>\n"
                         "    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>\n"
                         "    <p>Sed do eiusmod tempor incididunt ut labore et dolore magna.</p>\n"
                         "    <ul>\n"
                         "      <li>alpha-%d</li>\n"
                         "      <li>beta-%d</li>\n"
                         "      <li>gamma-%d%s</li>\n"
                         "    </ul>\n"
                         "  </section>\n",
                         i,
                         i,
                         mutated ? " (edited)" : "",
                         i,
                         i,
                         i,
                         mutated ? "!" : "");
        buf = buf_append(buf, &len, &cap, tmp, (size_t) n);
    }

    const char *tail = "</main>\n<footer id=\"foot\">end</footer>\n</body></html>\n";
    buf = buf_append(buf, &len, &cap, tail, strlen(tail));

    *out_len = len;
    return buf;
}

static char *load_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        perror("ftell");
        exit(1);
    }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t) sz);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz) {
        fprintf(stderr, "short read on %s\n", path);
        exit(1);
    }
    fclose(f);
    *out_len = (size_t) sz;
    return buf;
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *) a, y = *(const double *) b;
    return (x > y) - (x < y);
}

static void report(const char *name, double *samples, int n, size_t bytes_scanned, size_t op_count)
{
    qsort(samples, n, sizeof(double), cmp_double);
    double median = samples[n / 2];
    double min = samples[0];
    double mbps = (bytes_scanned / median) / (1024.0 * 1024.0);
    printf("  %-22s  median %8.1f us   min %8.1f us   %7.1f MB/s   ops=%zu\n",
           name,
           median * 1e6,
           min * 1e6,
           mbps,
           op_count);
}

static char *build_page_one_edit(size_t *out_len, int edit_idx)
{
    char *buf = NULL;
    size_t len = 0, cap = 0;
    char tmp[512];
    const char *head = "<!DOCTYPE html><html><body>\n"
                       "<header id=\"top\"><h1>Bench</h1></header>\n"
                       "<main id=\"content\">\n";
    buf = buf_append(buf, &len, &cap, head, strlen(head));
    for (int i = 0; i < SECTIONS; i++) {
        int mut = (i == edit_idx);
        int n = snprintf(tmp,
                         sizeof tmp,
                         "  <section id=\"s%d\">\n"
                         "    <h2>Section %d%s</h2>\n"
                         "    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>\n"
                         "    <p>Sed do eiusmod tempor incididunt ut labore et dolore magna.</p>\n"
                         "    <ul>\n"
                         "      <li>alpha-%d</li>\n"
                         "      <li>beta-%d</li>\n"
                         "      <li>gamma-%d%s</li>\n"
                         "    </ul>\n"
                         "  </section>\n",
                         i,
                         i,
                         mut ? " (edited)" : "",
                         i,
                         i,
                         i,
                         mut ? "!" : "");
        buf = buf_append(buf, &len, &cap, tmp, (size_t) n);
    }
    const char *tail = "</main>\n<footer id=\"foot\">end</footer>\n</body></html>\n";
    buf = buf_append(buf, &len, &cap, tail, strlen(tail));
    *out_len = len;
    return buf;
}

int main(void)
{
    size_t old_len, new_len;
    char *old_html = build_page(0, &old_len);
    char *new_html = build_page(40, &new_len);

    printf("payload: old=%zu B, new=%zu B, sections=%d, iterations=%d\n\n",
           old_len,
           new_len,
           SECTIONS,
           ITERATIONS);

    double *samples = malloc(sizeof(double) * ITERATIONS);
    if (!samples) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < WARMUP; i++) {
        mdf_diff r = mdf_compare(old_html, old_len, new_html, new_len);
        mdf_diff_free(&r);
    }
    size_t one_shot_ops = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        double t0 = now_sec();
        mdf_diff r = mdf_compare(old_html, old_len, new_html, new_len);
        samples[i] = now_sec() - t0;
        one_shot_ops = r.count;
        mdf_diff_free(&r);
    }
    report("mdf_diff (one-shot)", samples, ITERATIONS, old_len + new_len, one_shot_ops);

    mdf_view *d = mdf_view_new(old_html, old_len);
    for (int i = 0; i < WARMUP; i++) {
        const char *h = (i & 1) ? old_html : new_html;
        size_t n = (i & 1) ? old_len : new_len;
        mdf_diff r = mdf_view_update(d, h, n);
        mdf_diff_free(&r);
    }
    size_t stateful_ops = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        const char *h = (i & 1) ? old_html : new_html;
        size_t n = (i & 1) ? old_len : new_len;
        double t0 = now_sec();
        mdf_diff r = mdf_view_update(d, h, n);
        samples[i] = now_sec() - t0;
        stateful_ops = r.count;
        mdf_diff_free(&r);
    }
    report("mdf_view_update", samples, ITERATIONS, new_len, stateful_ops);
    mdf_view_free(d);

    size_t a_len, b_len;
    char *a = build_page_one_edit(&a_len, 200);
    char *b = build_page_one_edit(&b_len, 199);

    mdf_view *d2 = mdf_view_new(a, a_len);
    for (int i = 0; i < WARMUP; i++) {
        const char *h = (i & 1) ? a : b;
        size_t n = (i & 1) ? a_len : b_len;
        mdf_diff r = mdf_view_update(d2, h, n);
        mdf_diff_free(&r);
    }
    size_t single_ops = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        const char *h = (i & 1) ? a : b;
        size_t n = (i & 1) ? a_len : b_len;
        double t0 = now_sec();
        mdf_diff r = mdf_view_update(d2, h, n);
        samples[i] = now_sec() - t0;
        single_ops = r.count;
        mdf_diff_free(&r);
    }
    report("differ (single edit)", samples, ITERATIONS, b_len, single_ops);
    mdf_view_free(d2);
    free(a);
    free(b);

    size_t sa_len = 0, sb_len = 0;
    char *stocks_a = load_file("data/stocks_a.html", &sa_len);
    char *stocks_b = load_file("data/stocks_b.html", &sb_len);

    printf("\nstocks dashboard: a=%zu B, b=%zu B\n\n", sa_len, sb_len);

    for (int i = 0; i < WARMUP; i++) {
        mdf_diff r = mdf_compare(stocks_a, sa_len, stocks_b, sb_len);
        mdf_diff_free(&r);
    }
    size_t stocks_oneshot_ops = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        double t0 = now_sec();
        mdf_diff r = mdf_compare(stocks_a, sa_len, stocks_b, sb_len);
        samples[i] = now_sec() - t0;
        stocks_oneshot_ops = r.count;
        mdf_diff_free(&r);
    }
    report("stocks compare", samples, ITERATIONS, sa_len + sb_len, stocks_oneshot_ops);

    mdf_view *sv = mdf_view_new(stocks_a, sa_len);
    for (int i = 0; i < WARMUP; i++) {
        const char *h = (i & 1) ? stocks_a : stocks_b;
        size_t n = (i & 1) ? sa_len : sb_len;
        mdf_diff r = mdf_view_update(sv, h, n);
        mdf_diff_free(&r);
    }
    size_t stocks_view_ops = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        const char *h = (i & 1) ? stocks_a : stocks_b;
        size_t n = (i & 1) ? sa_len : sb_len;
        double t0 = now_sec();
        mdf_diff r = mdf_view_update(sv, h, n);
        samples[i] = now_sec() - t0;
        stocks_view_ops = r.count;
        mdf_diff_free(&r);
    }
    report("stocks view", samples, ITERATIONS, sb_len, stocks_view_ops);
    mdf_view_free(sv);
    free(stocks_a);
    free(stocks_b);

    free(samples);
    free(old_html);
    free(new_html);
    return 0;
}
