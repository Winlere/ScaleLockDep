# Test Suite Makefile
CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c11
LDFLAGS = -pthread

TESTS = test_01 test_02 test_03 test_04 test_05 test_06 test_07 test_08 \
        test_09 test_10 test_11 test_12 test_13 test_14 test_15 test_16

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
