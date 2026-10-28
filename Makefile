# Crop-Guardians build. Core is C17 and must compile cleanly with -Wall -Wextra -Werror.

CC      ?= gcc
CSTD    := -std=c17
WARN    := -Wall -Wextra -Werror
CFLAGS  ?= -O2 -g
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Isrc
ALL_CFLAGS = $(CSTD) $(WARN) $(CFLAGS)
LDLIBS_HW := -lgpiod

BUILD := build

# Core: logic, storage, clock, and the pipeline. None of it reaches a GPIO
# backend except through a gpio_backend passed in. Software-only tests link only these.
LOGIC_SRCS := src/hcsr04.c src/pir.c src/csv.c src/metrics.c src/csv_store.c \
              src/mono_clock.c src/pipeline.c src/latency_match.c
# Scripted GPIO backend for mocked-hardware tests.
MOCK_SRCS  := src/gpio_mock.c
# Real-hardware backend (needs libgpiod-dev).
HW_SRCS    := src/gpio_libgpiod.c

LOGIC_OBJS := $(LOGIC_SRCS:src/%.c=$(BUILD)/%.o)
MOCK_OBJS  := $(MOCK_SRCS:src/%.c=$(BUILD)/%.o)
HW_OBJS    := $(HW_SRCS:src/%.c=$(BUILD)/%.o)
TEST_HDRS  := $(wildcard src/*.h) $(wildcard tests/*.h tests/mock/*.h)

UNIT_TESTS := $(patsubst tests/unit/%.c,$(BUILD)/tests/unit/%,$(wildcard tests/unit/*.c))
MOCK_TESTS := $(patsubst tests/mock/%.c,$(BUILD)/tests/mock/%,$(wildcard tests/mock/*.c))

.PHONY: all tools test test-unit test-mock check-flags bench clean

all: $(BUILD)/crop_guard tools

# Latency benchmark tools (Phase 5); see tools/bench.sh.
tools: $(BUILD)/tools/stimulus $(BUILD)/tools/analyze

$(BUILD)/tools/stimulus: tools/stimulus.c $(HW_OBJS) $(wildcard src/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(HW_OBJS) $(LDLIBS_HW)

$(BUILD)/tools/analyze: tools/analyze.c $(LOGIC_OBJS) $(wildcard src/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(LOGIC_OBJS)

$(BUILD)/crop_guard: $(BUILD)/main.o $(LOGIC_OBJS) $(MOCK_OBJS) $(HW_OBJS)
	$(CC) $(ALL_CFLAGS) -o $@ $^ $(LDLIBS_HW)

$(BUILD)/%.o: src/%.c $(wildcard src/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

# Software-only tests: pure logic, no GPIO at all (not even the mock).
$(BUILD)/tests/unit/%: tests/unit/%.c $(LOGIC_OBJS) $(TEST_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(LOGIC_OBJS)

# Mocked-hardware tests: run against the scripted mock backend.
$(BUILD)/tests/mock/%: tests/mock/%.c $(LOGIC_OBJS) $(MOCK_OBJS) $(TEST_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -o $@ $< $(LOGIC_OBJS) $(MOCK_OBJS)

test: test-unit test-mock

test-unit: $(UNIT_TESTS)
	@for t in $^; do echo "[software-only] $$t"; $$t || exit 1; done

test-mock: $(MOCK_TESTS)
	@for t in $^; do echo "[mocked-hw] $$t"; $$t || exit 1; done

# Interrupt vs. polling benchmark on real hardware (needs the GPIO5->GPIO6 jumper).
bench: all
	tools/bench.sh

# Full rebuild from scratch; any warning fails because of -Werror.
check-flags: clean
	$(MAKE) all test

$(BUILD):
	@mkdir -p $@

clean:
	rm -rf $(BUILD)
