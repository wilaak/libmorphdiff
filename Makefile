CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic
AR      ?= ar
INCS    = -Iinclude -Isrc

BUILD   = build

LIB_SRC = $(wildcard src/*.c)
LIB_OBJ = $(LIB_SRC:src/%.c=$(BUILD)/%.o)
LIB     = $(BUILD)/libmorphdiff.a

HEADERS = include/morphdiff.h src/internal.h

all: $(BUILD)/demo

bench: $(BUILD)/bench

test: $(BUILD)/tests
	$(BUILD)/tests

data: $(BUILD)/gen_data
	$(BUILD)/gen_data

$(BUILD)/gen_data: $(BUILD)/gen_data.o | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/gen_data.o

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): $(LIB_OBJ) | $(BUILD)
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

$(BUILD)/%.o: tools/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

$(BUILD)/demo: $(BUILD)/demo.o $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/demo.o $(LIB)

$(BUILD)/bench: $(BUILD)/bench.o $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/bench.o $(LIB)

$(BUILD)/tests: $(BUILD)/tests.o $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/tests.o $(LIB)

FORMAT_SRC = $(wildcard include/*.h src/*.c src/*.h tools/*.c)

format:
	clang-format -i $(FORMAT_SRC)

format-check:
	clang-format --dry-run --Werror $(FORMAT_SRC)

# Sanitizer builds. Rebuild from scratch since CFLAGS change.
SAN_CFLAGS = -std=c99 -g -O1 -fno-omit-frame-pointer \
             -Wall -Wextra -Wpedantic \
             -fsanitize=address,undefined \
             -fno-sanitize-recover=all

test-asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(SAN_CFLAGS)"

test-ubsan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c99 -g -O1 -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all"

# Fuzzing (requires clang for libFuzzer support).
FUZZ_CFLAGS = -std=c99 -g -O1 -fno-omit-frame-pointer \
              -Wall -Wextra -Wpedantic \
              -fsanitize=fuzzer,address,undefined \
              -fno-sanitize-recover=all

$(BUILD)/fuzz_compare: $(BUILD)/fuzz_compare.o $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/fuzz_compare.o $(LIB)

$(BUILD)/fuzz_view: $(BUILD)/fuzz_view.o $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BUILD)/fuzz_view.o $(LIB)

fuzz-compare:
	@command -v clang >/dev/null 2>&1 || { echo "fuzzing requires clang (sudo apt install clang)"; exit 1; }
	$(MAKE) clean
	$(MAKE) $(BUILD)/fuzz_compare CC=clang CFLAGS="$(FUZZ_CFLAGS)"
	mkdir -p fuzz-corpus fuzz-findings
	[ -f fuzz-corpus/stocks_a ] && true || cp data/stocks_a.html fuzz-corpus/stocks_a 2>/dev/null || true
	[ -f fuzz-corpus/stocks_b ] && true || cp data/stocks_b.html fuzz-corpus/stocks_b 2>/dev/null || true
	$(BUILD)/fuzz_compare -artifact_prefix=fuzz-findings/ fuzz-corpus

fuzz-view:
	@command -v clang >/dev/null 2>&1 || { echo "fuzzing requires clang (sudo apt install clang)"; exit 1; }
	$(MAKE) clean
	$(MAKE) $(BUILD)/fuzz_view CC=clang CFLAGS="$(FUZZ_CFLAGS)"
	mkdir -p fuzz-corpus fuzz-findings
	$(BUILD)/fuzz_view -artifact_prefix=fuzz-findings/ fuzz-corpus

clean:
	rm -rf $(BUILD) gmon.out

.PHONY: all clean bench test data format format-check test-asan test-ubsan fuzz-compare fuzz-view
