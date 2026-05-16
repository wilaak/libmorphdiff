#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    const char *sym;
    const char *name;
    double price;
    double change;
    long volume;
    double day_low;
    double day_high;
} ticker_t;

static const ticker_t TICKERS[] = {
    {"AAPL", "Apple Inc.", 214.56, 1.23, 52341200, 213.10, 215.45},
    {"MSFT", "Microsoft Corp.", 431.22, -2.18, 28129400, 429.50, 434.10},
    {"GOOGL", "Alphabet Inc. Class A", 178.45, 0.92, 21048300, 177.20, 179.10},
    {"AMZN", "Amazon.com Inc.", 186.31, 3.04, 41209800, 183.40, 187.05},
    {"META", "Meta Platforms Inc.", 506.78, -1.45, 14820100, 504.20, 510.30},
    {"TSLA", "Tesla Inc.", 247.92, 5.67, 89321400, 241.10, 249.80},
    {"NVDA", "NVIDIA Corp.", 923.14, 8.55, 39220500, 911.80, 930.20},
    {"BRK.B", "Berkshire Hathaway Inc. Cl B", 419.07, 0.21, 3128400, 418.00, 420.55},
    {"JPM", "JPMorgan Chase & Co.", 198.74, -0.84, 8245100, 197.20, 200.10},
    {"V", "Visa Inc.", 279.55, 1.12, 5621300, 278.00, 280.40},
    {"UNH", "UnitedHealth Group Inc.", 512.18, -3.42, 2914200, 510.00, 517.30},
    {"WMT", "Walmart Inc.", 66.40, 0.55, 18412900, 65.80, 66.95},
    {"XOM", "Exxon Mobil Corp.", 108.92, 0.43, 12041800, 107.80, 109.55},
    {"MA", "Mastercard Inc.", 459.30, 2.17, 2912500, 456.20, 460.80},
    {"PG", "Procter & Gamble Co.", 165.21, -0.31, 6128400, 164.80, 166.20},
    {"HD", "Home Depot Inc.", 335.42, 1.84, 3014700, 333.00, 336.50},
    {"COST", "Costco Wholesale Corp.", 812.55, 4.20, 1842100, 806.40, 815.10},
    {"JNJ", "Johnson & Johnson", 152.18, -0.62, 6520300, 151.50, 153.40},
    {"ABBV", "AbbVie Inc.", 168.40, 0.78, 4912800, 167.20, 169.00},
    {"BAC", "Bank of America Corp.", 39.82, 0.14, 32014800, 39.55, 40.05},
    {"CRM", "Salesforce Inc.", 275.18, -1.92, 4128700, 273.80, 278.30},
    {"AVGO", "Broadcom Inc.", 1342.55, 12.04, 1842300, 1325.00, 1350.20},
    {"NFLX", "Netflix Inc.", 652.84, 3.21, 2841200, 648.10, 656.40},
    {"AMD", "Advanced Micro Devices", 167.92, 2.18, 41204800, 165.20, 169.40},
    {"PEP", "PepsiCo Inc.", 169.31, -0.45, 4128900, 168.80, 170.20},
    {"ADBE", "Adobe Inc.", 482.10, 1.62, 2812400, 478.50, 484.30},
    {"KO", "Coca-Cola Co.", 62.14, 0.18, 12914800, 61.80, 62.45},
    {"TMO", "Thermo Fisher Scientific", 564.20, -2.41, 1142800, 562.00, 568.50},
    {"CSCO", "Cisco Systems Inc.", 47.92, 0.21, 18412300, 47.55, 48.10},
    {"ACN", "Accenture plc", 298.55, -1.18, 1812400, 297.30, 301.00},
    {"DIS", "Walt Disney Co.", 102.40, 0.94, 9128700, 101.20, 103.15},
    {"MRK", "Merck & Co.", 128.72, -0.38, 7140200, 128.00, 129.40},
    {"ABT", "Abbott Laboratories", 108.30, 0.27, 4912800, 107.80, 108.95},
    {"VZ", "Verizon Communications", 41.18, 0.08, 14014800, 40.95, 41.40},
    {"NKE", "Nike Inc. Class B", 89.42, -1.05, 6412900, 88.80, 91.20},
    {"INTC", "Intel Corp.", 29.84, 0.34, 41208700, 29.30, 30.15},
    {"PFE", "Pfizer Inc.", 27.92, -0.18, 28412800, 27.70, 28.20},
    {"ORCL", "Oracle Corp.", 127.55, 1.42, 8120400, 125.80, 128.30},
    {"T", "AT&T Inc.", 17.20, 0.05, 32048700, 17.05, 17.35},
    {"WFC", "Wells Fargo & Co.", 60.18, 0.42, 14820100, 59.70, 60.55},
    {"LIN", "Linde plc", 434.20, -1.85, 1042400, 432.00, 437.50},
    {"MCD", "McDonald's Corp.", 268.42, -0.62, 2412900, 267.50, 269.80},
    {"PM", "Philip Morris International", 102.84, 0.55, 4128400, 102.00, 103.20},
    {"DHR", "Danaher Corp.", 248.31, -1.04, 1812400, 247.00, 250.10},
    {"IBM", "International Business Machines", 167.42, 0.28, 3412800, 166.80, 168.30},
    {"TXN", "Texas Instruments Inc.", 198.55, 1.18, 4128700, 196.40, 199.20},
    {"GE", "GE Aerospace", 162.74, 2.42, 3120400, 159.80, 163.50},
    {"BA", "Boeing Co.", 175.20, -2.18, 6412800, 174.50, 178.40},
    {"CAT", "Caterpillar Inc.", 348.92, 3.41, 2012400, 344.20, 351.10},
    {"GS", "Goldman Sachs Group Inc.", 492.18, 1.85, 1742900, 489.30, 494.50},
};
static const size_t TICKER_COUNT = sizeof(TICKERS) / sizeof(TICKERS[0]);

typedef struct {
    int index;           // Index into TICKERS to mutate.
    double price_delta;  // Signed dollar change applied to price.
    long volume_add;     // Extra volume added since last tick.
} mutation_t;

static const mutation_t MUTATIONS[] = {
    {0, 0.34, 184200},    // AAPL
    {3, -0.82, 421100},   // AMZN
    {5, 1.91, 1209400},   // TSLA
    {6, 2.45, 920400},    // NVDA
    {11, 0.18, 281000},   // WMT
    {19, -0.07, 612400},  // BAC
    {23, 0.92, 1841100},  // AMD
    {35, -0.12, 921000},  // INTC
    {44, 0.31, 421000},   // IBM
    {49, 0.95, 184100},   // GS
};
static const size_t MUTATION_COUNT = sizeof(MUTATIONS) / sizeof(MUTATIONS[0]);

static const char *TRADES[] = {
    "AAPL  214.56  +1.23  500   14:32:18",
    "MSFT  431.22  -2.18  120   14:32:17",
    "TSLA  247.92  +5.67  2400  14:32:17",
    "NVDA  923.14  +8.55  180   14:32:16",
    "AMZN  186.31  +3.04  900   14:32:15",
    "META  506.78  -1.45  340   14:32:14",
    "GOOGL 178.45  +0.92  1100  14:32:14",
    "JPM   198.74  -0.84  220   14:32:13",
    "V     279.55  +1.12  480   14:32:12",
    "BAC    39.82  +0.14  6200  14:32:11",
    "AMD   167.92  +2.18  3100  14:32:10",
    "INTC   29.84  +0.34  8400  14:32:09",
    "T      17.20  +0.05  12000 14:32:08",
    "PFE    27.92  -0.18  4200  14:32:07",
    "WFC    60.18  +0.42  1900  14:32:06",
};
static const size_t TRADE_COUNT = sizeof(TRADES) / sizeof(TRADES[0]);

static void apply_mutation(ticker_t *t, const mutation_t *m)
{
    t->price += m->price_delta;
    t->change += m->price_delta;
    t->volume += m->volume_add;
    if (t->price > t->day_high) {
        t->day_high = t->price;
    }
    if (t->price < t->day_low) {
        t->day_low = t->price;
    }
}

static void write_row(FILE *f, const ticker_t *t)
{
    double pct = (t->change / (t->price - t->change)) * 100.0;
    fprintf(f,
            "      <tr id=\"%s\">"
            "<td>%s</td>"
            "<td>%s</td>"
            "<td>%.2f</td>"
            "<td>%+.2f</td>"
            "<td>%+.2f%%</td>"
            "<td>%ld</td>"
            "<td>%.2f</td>"
            "<td>%.2f</td>"
            "</tr>\n",
            t->sym,
            t->sym,
            t->name,
            t->price,
            t->change,
            pct,
            t->volume,
            t->day_low,
            t->day_high);
}

static void write_page(FILE *f, int with_mutations)
{
    assert(f != NULL);

    ticker_t rows[sizeof(TICKERS) / sizeof(TICKERS[0])];
    memcpy(rows, TICKERS, sizeof(TICKERS));
    if (with_mutations) {
        for (size_t i = 0; i < MUTATION_COUNT; i++) {
            apply_mutation(&rows[MUTATIONS[i].index], &MUTATIONS[i]);
        }
    }

    fputs("<!DOCTYPE html>\n"
          "<html>\n"
          "<head><title>Market Watch</title></head>\n"
          "<body>\n"
          "  <header id=\"page-header\">\n"
          "    <h1>Market Watch</h1>\n"
          "    <p id=\"timestamp\">Snapshot at ",
          f);
    fputs(with_mutations ? "14:32:18 ET" : "14:32:13 ET", f);
    fputs("</p>\n"
          "    <ul id=\"indices\">\n"
          "      <li id=\"SPX\">S&amp;P 500: 5314.21 <span>+12.84 (+0.24%)</span></li>\n"
          "      <li id=\"DJI\">Dow: 41209.18 <span>-48.21 (-0.12%)</span></li>\n"
          "      <li id=\"NDX\">Nasdaq 100: 18742.92 <span>+92.41 (+0.50%)</span></li>\n"
          "      <li id=\"RUT\">Russell 2000: 2148.55 <span>+5.18 (+0.24%)</span></li>\n"
          "      <li id=\"VIX\">VIX: 14.21 <span>-0.42 (-2.87%)</span></li>\n"
          "    </ul>\n"
          "  </header>\n"
          "  <main id=\"quotes\">\n"
          "    <table id=\"quote-table\">\n"
          "      <thead>\n"
          "        <tr>\n"
          "          <th>Ticker</th><th>Name</th><th>Price</th><th>Change</th>"
          "<th>%</th><th>Volume</th><th>Day Low</th><th>Day High</th>\n"
          "        </tr>\n"
          "      </thead>\n"
          "      <tbody id=\"quote-rows\">\n",
          f);

    for (size_t i = 0; i < TICKER_COUNT; i++) {
        write_row(f, &rows[i]);
    }

    fputs("      </tbody>\n"
          "    </table>\n"
          "  </main>\n"
          "  <aside id=\"recent-trades\">\n"
          "    <h2>Recent Trades</h2>\n"
          "    <ol id=\"trade-log\">\n",
          f);

    for (size_t i = 0; i < TRADE_COUNT; i++) {
        fprintf(f, "      <li>%s</li>\n", TRADES[i]);
    }

    fputs("    </ol>\n"
          "  </aside>\n"
          "  <footer id=\"status\">\n"
          "    <p>Quotes refresh every 5 seconds. Source: simulated feed.</p>\n"
          "  </footer>\n"
          "</body>\n"
          "</html>\n",
          f);
}

int main(void)
{
    mkdir("data", 0755);

    FILE *fa = fopen("data/stocks_a.html", "w");
    FILE *fb = fopen("data/stocks_b.html", "w");
    if (!fa || !fb) {
        perror("fopen");
        return 1;
    }
    write_page(fa, 0);
    write_page(fb, 1);
    fclose(fa);
    fclose(fb);

    printf("wrote data/stocks_a.html, data/stocks_b.html (%zu tickers, %zu mutated)\n",
           TICKER_COUNT,
           MUTATION_COUNT);
    return 0;
}
