LOCKDEP_LIB = lockdep/liblockdep.so

BENCHMARKS = \
	benchmarks/correct_40threads_3locks_10000iter.out \
	benchmarks/correct_40threads_40locks_10000iter.out \
	benchmarks/deadlock_2thread_2lock.out \
	benchmarks/deadlock_3thread_circular.out

OVERHEAD_BIN     = benchmarks/bench_overhead.out
ANYLOCK_BIN      = benchmarks/bench_overhead_anylock.out
ANYLOCK4_BIN     = benchmarks/bench_overhead_anylock4.out

# Thread counts to sweep
THREAD_COUNTS = 1 2 4 8 16 32 64

# Iterations per thread (tune down if runtime is too long)
ITERS = 100000

.PHONY: all build run overhead overhead-anylock overhead-anylock4 clean

all: build

build: $(LOCKDEP_LIB) $(BENCHMARKS)

$(LOCKDEP_LIB):
	$(MAKE) -C lockdep

$(BENCHMARKS) $(OVERHEAD_BIN) $(ANYLOCK_BIN) $(ANYLOCK4_BIN):
	$(MAKE) -C benchmarks

# Run all benchmarks under lockdep; deadlock ones exit 66, correct ones exit 0.
# Use "|| true" so make doesn't abort on expected deadlock exits.
run: build
	@for bin in $(BENCHMARKS); do \
		echo "========================================"; \
		echo "Running: $$bin"; \
		echo "========================================"; \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$$bin; \
		ret=$$?; \
		if [ $$ret -eq 66 ]; then \
			echo "[RESULT] Deadlock detected (exit 66)"; \
		elif [ $$ret -eq 0 ]; then \
			echo "[RESULT] No deadlock (exit 0)"; \
		else \
			echo "[RESULT] Unexpected exit code: $$ret"; \
		fi; \
		echo ""; \
	done

# ---------------------------------------------------------------------------
# Overhead experiment
#
# Two scenarios:
#   high-contention : all threads share a single lock
#   low-contention  : each thread gets its own lock (num_locks == num_threads)
#
# For each scenario, runs WITHOUT lockdep (baseline) then WITH lockdep.
# Columns: threads  locks  iters  wall_ns  total_lock_ops  lock_ops_per_sec
# ---------------------------------------------------------------------------
overhead: build $(OVERHEAD_BIN)
	@echo "=== High-contention (1 shared lock) ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\tlocks\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		./$(OVERHEAD_BIN) $$t 1 $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\tlocks\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(OVERHEAD_BIN) $$t 1 $(ITERS); \
	done
	@echo ""
	@echo "=== Low-contention (each thread owns its own lock) ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\tlocks\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		./$(OVERHEAD_BIN) $$t $$t $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\tlocks\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(OVERHEAD_BIN) $$t $$t $(ITERS); \
	done

# ---------------------------------------------------------------------------
# Overhead experiment — 2-lock any-acquire
#
# 2 shared locks; a thread acquires whichever is free (trylock both, then
# block on preferred). Allows up to 2-way concurrency vs 1 for single-lock.
# Columns: threads  iters  wall_ns  total_ops  ops_per_sec
# ---------------------------------------------------------------------------
overhead-anylock: build $(ANYLOCK_BIN)
	@echo "=== 2-lock any-acquire ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		./$(ANYLOCK_BIN) $$t $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(ANYLOCK_BIN) $$t $(ITERS); \
	done

overhead-anylock4: build $(ANYLOCK4_BIN)
	@echo "=== 4-lock any-acquire ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		./$(ANYLOCK4_BIN) $$t $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for t in $(THREAD_COUNTS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(ANYLOCK4_BIN) $$t $(ITERS); \
	done

clean:
	$(MAKE) -C lockdep clean
	$(MAKE) -C benchmarks clean
