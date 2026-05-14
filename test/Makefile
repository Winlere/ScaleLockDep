# Test Suite Makefile
CC = gcc
CFLAGS = -g -Wall -Wextra -pthread -std=c11
LDFLAGS = -pthread

TESTS = test_01 test_02 test_03 test_04 test_05 test_06 \
        test_07 test_08 test_09 test_10 \
        test_11 test_12 test_13 \
        test_14 test_15 test_16 \
        test_17 test_18 test_19 test_20 test_21 test_22 \
        test_23 test_24 test_25 test_26 \
        test_27 test_28 \
        test_29 test_30

all: $(TESTS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TESTS)

# Phony targets for running individual tests
.PHONY: run_all $(TESTS:%=run_%)

run_all: all
	@echo "Running all tests (with 3-second timeout for deadlock tests)..."
	@for test in $(TESTS); do \
		echo ""; \
		echo "=== Running $$test ==="; \
		timeout 3 ./$$test || echo "Test $$test: timeout or error (likely deadlock)"; \
	done

# Individual test runners
run_%: %
	timeout 3 ./$* || echo "$*: timeout or error"

.PHONY: help
help:
	@echo "Test Suite for Node Deadlock Detection"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all tests"
	@echo "  clean        - Remove compiled tests"
	@echo "  run_all      - Run all tests with timeout"
	@echo "  run_test_XX  - Run individual test (e.g., run_test_01)"
	@echo ""
	@echo "Test Categories:"
	@echo "  PURELY NON-DEADLOCK (should all complete):"
	@echo "    test_01 - Single thread with lock"
	@echo "    test_02 - Two threads, one lock"
	@echo "    test_03 - Ordered locking (2 threads, 2 locks)"
	@echo "    test_04 - Ordered locking (8 threads, 3 locks)"
	@echo "    test_05 - Disjoint locks"
	@echo "    test_06 - Lock-free work"
	@echo ""
	@echo "  DEADLOCK (should timeout/hang):"
	@echo "    test_07 - 2-thread 2-lock deadlock"
	@echo "    test_08 - 3-thread circular deadlock"
	@echo "    test_09 - Self-deadlock (recursive lock)"
	@echo "    test_10 - 4-thread cycle deadlock"
	@echo ""
	@echo "  DUBIOUS NON-DEADLOCK (risky, timing-dependent):"
	@echo "    test_11 - Random lock order"
	@echo "    test_12 - Try-lock with retry"
	@echo "    test_13 - Dynamic lock selection"
	@echo ""
	@echo "  MIXED/PARTIAL DEADLOCK (subset blocked):"
	@echo "    test_14 - Partial deadlock (some threads blocked)"
	@echo "    test_15 - Barrier with deadlock"
	@echo "    test_16 - Asymmetric deadlock"
	@echo ""
	@echo "  NON-DEADLOCK — extended (tests 17-22):"
	@echo "    test_17 - Lock convoy (10 threads, 1 lock)"
	@echo "    test_18 - Producer-consumer with condvar"
	@echo "    test_19 - 16 threads, 4 locks, ordered A->B->C->D"
	@echo "    test_20 - Per-thread local + global lock hierarchy"
	@echo "    test_21 - Sequential non-nested acquisition"
	@echo "    test_22 - Trylock readers + blocking writer"
	@echo ""
	@echo "  DEADLOCK — extended (tests 23-26):"
	@echo "    test_23 - 5-thread pentagon deadlock"
	@echo "    test_24 - Layered lock inversion (RacerX pattern)"
	@echo "    test_25 - 6-thread hexagon deadlock"
	@echo "    test_26 - Dual independent deadlock pairs"
	@echo ""
	@echo "  DUBIOUS — extended (tests 27-28):"
	@echo "    test_27 - Seeded-random lock ordering"
	@echo "    test_28 - Long safe chain with single inversion"
	@echo ""
	@echo "  MIXED — extended (tests 29-30):"
	@echo "    test_29 - Triple-pair: two deadlocks + one safe pair"
	@echo "    test_30 - Staggered deadlock under active safe workload"
