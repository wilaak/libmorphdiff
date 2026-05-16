#include "morphdiff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;
static const char *g_test = "?";

#define LIT(s) (s), (sizeof(s) - 1)

static void check(int cond, const char *msg)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "FAIL [%s] %s\n", g_test, msg);
    }
}

static int
has_op(const mdf_diff *r, const char *new_html, const char *selector, const char *html_substring)
{
    size_t sel_target_len = strlen(selector);
    for (size_t i = 0; i < r->count; i++) {
        const mdf_op *op = &r->ops[i];
        if (op->selector_len != sel_target_len) {
            continue;
        }
        if (memcmp(r->selectors + op->selector_off, selector, sel_target_len) != 0) {
            continue;
        }
        if (!html_substring) {
            return 1;
        }
        size_t hl = strlen(html_substring);
        const char *html = new_html + op->html_off;
        for (size_t j = 0; j + hl <= op->html_len; j++) {
            if (memcmp(html + j, html_substring, hl) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void dump(const mdf_diff *r, const char *new_html)
{
    fprintf(stderr, "  got %zu op(s):\n", r->count);
    for (size_t i = 0; i < r->count; i++) {
        const mdf_op *op = &r->ops[i];
        fprintf(stderr,
                "    [%zu] %.*s -> ",
                i,
                (int) op->selector_len,
                r->selectors + op->selector_off);
        size_t n = op->html_len < 80 ? op->html_len : 80;
        fwrite(new_html + op->html_off, 1, n, stderr);
        fputc('\n', stderr);
    }
}

#define RUN(name)      \
    do {               \
        g_test = name; \
    } while (0)

static void test_identical(void)
{
    RUN("identical");
    const char html[] = "<html><body><p>x</p></body></html>";
    mdf_diff r = mdf_compare(LIT(html), LIT(html));
    check(r.count == 0, "no ops when buffers identical");
    if (r.count != 0) {
        dump(&r, html);
    }
    mdf_diff_free(&r);
}

static void test_id_change(void)
{
    RUN("id_change");
    const char a[] = "<html><body><p id=\"x\">old</p></body></html>";
    const char b[] = "<html><body><p id=\"x\">new</p></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op");
    check(has_op(&r, b, "#x", "new"), "op selector #x with new content");
    if (r.count != 1 || !has_op(&r, b, "#x", "new")) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_path_change(void)
{
    RUN("path_change");
    const char a[] = "<html><body><p>one</p><p>two</p></body></html>";
    const char b[] = "<html><body><p>one</p><p>TWO</p></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op");
    check(has_op(&r, b, "html > body > p:nth-of-type(2)", "TWO"),
          "nth-of-type(2) selector with new content");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_ancestor_pruning(void)
{
    RUN("ancestor_pruning");
    const char a[] = "<html><body><div id=\"d\"><p>old</p></div></body></html>";
    const char b[] = "<html><body><div id=\"d\"><p>new</p></div></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op after pruning ancestors");
    check(has_op(&r, b, "#d > p", "new"), "deepest selector kept");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_multiple_changes(void)
{
    RUN("multiple_changes");
    const char a[] = "<html><body>"
                     "<p id=\"a\">A0</p>"
                     "<p id=\"b\">B0</p>"
                     "<p id=\"c\">C0</p>"
                     "</body></html>";
    const char b[] = "<html><body>"
                     "<p id=\"a\">A1</p>"
                     "<p id=\"b\">B0</p>"
                     "<p id=\"c\">C1</p>"
                     "</body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 2, "2 ops (a and c changed, b unchanged)");
    check(has_op(&r, b, "#a", "A1"), "#a updated");
    check(has_op(&r, b, "#c", "C1"), "#c updated");
    check(!has_op(&r, b, "#b", NULL), "#b not in ops");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_insertion(void)
{
    RUN("insertion");
    const char a[] = "<html><body><ul><li>x</li></ul></body></html>";
    const char b[] = "<html><body><ul><li>x</li><li>y</li></ul></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count >= 1, "at least 1 op covering the inserted child");
    check(has_op(&r, b, "html > body > ul", "<li>y</li>") ||
              has_op(&r, b, "html > body > ul > li:nth-of-type(2)", "y"),
          "ul or new li mentioned");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_self_closing(void)
{
    RUN("self_closing");
    const char a[] = "<html><body><img id=\"i\" src=\"a.jpg\"></body></html>";
    const char b[] = "<html><body><img id=\"i\" src=\"b.jpg\"></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op for void element src change");
    check(has_op(&r, b, "#i", "b.jpg"), "#i updated to new src");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_raw_script(void)
{
    RUN("raw_script");
    const char a[] = "<html><body><script>var x = '<p>'; y = 1;</script></body></html>";
    const char b[] = "<html><body><script>var x = '<p>'; y = 2;</script></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op for script body change");
    check(has_op(&r, b, "html > body > script", "y = 2"), "script content updated");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_comments(void)
{
    RUN("comments");
    const char a[] = "<html><body><!-- <p>foo</p> --><p id=\"x\">old</p></body></html>";
    const char b[] = "<html><body><!-- <p>foo</p> --><p id=\"x\">new</p></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op (only #x changed)");
    check(has_op(&r, b, "#x", "new"), "#x updated");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_nth_of_type(void)
{
    RUN("nth_of_type");
    const char a[] = "<html><body>"
                     "<p>p1</p><span>s1</span><p>p2</p><span>s2</span>"
                     "</body></html>";
    const char b[] = "<html><body>"
                     "<p>p1</p><span>s1</span><p>P2</p><span>s2</span>"
                     "</body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op");
    check(has_op(&r, b, "html > body > p:nth-of-type(2)", "P2"),
          "second p (not second child) selected");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_differ_basic(void)
{
    RUN("differ_basic");
    const char a[] = "<html><body><p id=\"x\">A</p></body></html>";
    const char b[] = "<html><body><p id=\"x\">B</p></body></html>";
    const char c[] = "<html><body><p id=\"x\">C</p></body></html>";

    mdf_view *d = mdf_view_new(LIT(a));

    mdf_diff r1 = mdf_view_update(d, LIT(b));
    check(r1.count == 1 && has_op(&r1, b, "#x", "B"), "first diff: A->B");
    if (g_fail) {
        dump(&r1, b);
    }
    mdf_diff_free(&r1);

    mdf_diff r2 = mdf_view_update(d, LIT(c));
    check(r2.count == 1 && has_op(&r2, c, "#x", "C"), "second diff: B->C");
    if (g_fail) {
        dump(&r2, c);
    }
    mdf_diff_free(&r2);

    mdf_diff r3 = mdf_view_update(d, LIT(c));
    check(r3.count == 0, "third diff: C->C produces no ops");
    if (r3.count != 0) {
        dump(&r3, c);
    }
    mdf_diff_free(&r3);

    mdf_view_free(d);
}

static void test_differ_matches_oneshot(void)
{
    RUN("differ_matches_oneshot");
    const char a[] = "<html><body>"
                     "<header id=\"h\"><h1>Title</h1></header>"
                     "<main id=\"m\">"
                     "<section id=\"s1\"><p>one</p><p>two</p></section>"
                     "<section id=\"s2\"><p>three</p></section>"
                     "</main></body></html>";
    const char b[] = "<html><body>"
                     "<header id=\"h\"><h1>Title</h1></header>"
                     "<main id=\"m\">"
                     "<section id=\"s1\"><p>one</p><p>TWO</p></section>"
                     "<section id=\"s2\"><p>three!</p></section>"
                     "</main></body></html>";

    mdf_diff r1 = mdf_compare(LIT(a), LIT(b));
    mdf_view *d = mdf_view_new(LIT(a));
    mdf_diff r2 = mdf_view_update(d, LIT(b));

    check(r1.count == r2.count, "op counts match");
    for (size_t i = 0; i < r1.count; i++) {
        const mdf_op *op = &r1.ops[i];  // Selectors not NUL-terminated.
        char sel_buf[256];
        size_t sl = op->selector_len < sizeof sel_buf - 1 ? op->selector_len : sizeof sel_buf - 1;
        memcpy(sel_buf, r1.selectors + op->selector_off, sl);
        sel_buf[sl] = '\0';
        check(has_op(&r2, b, sel_buf, NULL), "every one-shot selector present in differ result");
    }
    if (g_fail) {
        dump(&r1, b);
        dump(&r2, b);
    }

    mdf_diff_free(&r1);
    mdf_diff_free(&r2);
    mdf_view_free(d);
}

static void test_no_id_top_level(void)
{
    RUN("no_id_top_level");
    const char a[] = "<html><body><div><p>x</p></div></body></html>";
    const char b[] = "<html><body><div><p>X</p></div></body></html>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op");
    check(has_op(&r, b, "html > body > div > p", "X"), "deepest path selector");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static void test_uppercase_tags(void)
{
    RUN("uppercase_tags");
    const char a[] = "<HTML><BODY><P id=\"x\">a</P></BODY></HTML>";
    const char b[] = "<HTML><BODY><P id=\"x\">b</P></BODY></HTML>";
    mdf_diff r = mdf_compare(LIT(a), LIT(b));
    check(r.count == 1, "exactly 1 op");
    check(has_op(&r, b, "#x", "b"), "#x captured despite uppercase tags");
    if (g_fail) {
        dump(&r, b);
    }
    mdf_diff_free(&r);
}

static char *load_file_or_skip(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *) malloc((size_t) sz);
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = (size_t) sz;
    return buf;
}

static int has_op_with_prefix(const mdf_diff *r, const char *prefix)
{
    size_t pl = strlen(prefix);
    for (size_t i = 0; i < r->count; i++) {
        const mdf_op *op = &r->ops[i];
        if (op->selector_len < pl) {
            continue;
        }
        if (memcmp(r->selectors + op->selector_off, prefix, pl) == 0) {
            return 1;
        }
    }
    return 0;
}

// Needs `make data`. Pruner emits cell-level ops under #TICKER.
static void test_stocks_dashboard(void)
{
    RUN("stocks_dashboard");

    size_t a_len = 0, b_len = 0;
    char *a = load_file_or_skip("data/stocks_a.html", &a_len);
    char *b = load_file_or_skip("data/stocks_b.html", &b_len);
    if (!a || !b) {
        free(a);
        free(b);
        fprintf(stderr, "SKIP [%s] data files missing (run `make data`)\n", g_test);
        return;
    }

    mdf_diff r = mdf_compare(a, a_len, b, b_len);

    static const char *changed[] = {
        "#AAPL",
        "#AMZN",
        "#TSLA",
        "#NVDA",
        "#WMT",
        "#BAC",
        "#AMD",
        "#INTC",
        "#IBM",
        "#GS",
    };
    for (size_t i = 0; i < sizeof changed / sizeof changed[0]; i++) {
        check(has_op_with_prefix(&r, changed[i]), changed[i]);
    }
    check(!has_op_with_prefix(&r, "#MSFT"), "#MSFT unchanged");
    check(!has_op_with_prefix(&r, "#JPM"), "#JPM unchanged");
    check(!has_op_with_prefix(&r, "#SPX"), "index summary unchanged");

    mdf_view *v = mdf_view_new(a, a_len);
    mdf_diff vr = mdf_view_update(v, b, b_len);
    check(vr.count == r.count, "view update produces same op count");

    if (g_fail) {
        dump(&r, b);
        dump(&vr, b);
    }

    mdf_diff_free(&vr);
    mdf_view_free(v);
    mdf_diff_free(&r);
    free(a);
    free(b);
}

int main(void)
{
    test_identical();
    test_id_change();
    test_path_change();
    test_ancestor_pruning();
    test_multiple_changes();
    test_insertion();
    test_self_closing();
    test_raw_script();
    test_comments();
    test_nth_of_type();
    test_differ_basic();
    test_differ_matches_oneshot();
    test_no_id_top_level();
    test_uppercase_tags();
    test_stocks_dashboard();

    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
