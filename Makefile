CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic
AR      ?= ar
INCS    = -Iinclude -Isrc

BUILD   = build

LIB_SRC = $(wildcard src/*.c)
LIB_OBJ = $(LIB_SRC:src/%.c=$(BUILD)/%.o)
LIB     = $(BUILD)/libmorphdiff.a

HEADERS = include/morph_diff.h src/internal.h

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

clean:
	rm -rf $(BUILD) gmon.out

.PHONY: all clean bench test data format format-check
