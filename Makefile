# Crop-Guardians build. Core is C17 and must compile cleanly with -Wall -Wextra -Werror.

CC      ?= gcc
CSTD    := -std=c17
WARN    := -Wall -Wextra -Werror
CFLAGS  ?= -O2 -g
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Isrc
ALL_CFLAGS = $(CSTD) $(WARN) $(CFLAGS)
LDLIBS_HW := -lgpiod

BUILD := build

# Modules shared by the program and the tests (no direct hardware dependency).
CORE_SRCS := src/gpio_mock.c
# Real-hardware backend (needs libgpiod-dev).
HW_SRCS   := src/gpio_libgpiod.c

CORE_OBJS := $(CORE_SRCS:src/%.c=$(BUILD)/%.o)
HW_OBJS   := $(HW_SRCS:src/%.c=$(BUILD)/%.o)

UNIT_TESTS := $(patsubst tests/unit/%.c,$(BUILD)/tests/unit/%,$(wildcard tests/unit/*.c))
MOCK_TESTS := $(patsubst tests/mock/%.c,$(BUILD)/tests/mock/%,$(wildcard tests/mock/*.c))

.PHONY: all test test-unit test-mock check-flags clean

all: $(BUILD)/crop_guard

$(BUILD)/crop_guard: $(BUILD)/main.o $(CORE_OBJS) $(HW_OBJS)
	$(CC) $(ALL_CFLAGS) -o $@ $^ $(LDLIBS_HW)

$(BUILD)/%.o: src/%.c $(wildcard src/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

# Software-only tests: pure logic, no GPIO at all.
$(BUILD)/tests/unit/%: tests/unit/%.c $(CORE_OBJS) $(wildcard src/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(CORE_OBJS)

# Mocked-hardware tests: run the pipeline against the scripted mock backend.
$(BUILD)/tests/mock/%: tests/mock/%.c $(CORE_OBJS) $(wildcard src/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(CORE_OBJS)

test: test-unit test-mock

test-unit: $(UNIT_TESTS)
	@for t in $^; do echo "[software-only] $$t"; $$t || exit 1; done

test-mock: $(MOCK_TESTS)
	@for t in $^; do echo "[mocked-hw] $$t"; $$t || exit 1; done

# Full rebuild from scratch; any warning fails because of -Werror.
check-flags: clean
	$(MAKE) all test

$(BUILD):
	@mkdir -p $@

clean:
	rm -rf $(BUILD)
