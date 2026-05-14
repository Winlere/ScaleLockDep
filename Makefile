LOCKDEP_LIB = lockdep/liblockdep.so

BENCHMARKS = \
	benchmarks/correct_40threads_3locks_10000iter.out \
	benchmarks/correct_40threads_40locks_10000iter.out \
	benchmarks/deadlock_2thread_2lock.out \
	benchmarks/deadlock_3thread_circular.out

OVERHEAD_BIN     = benchmarks/bench_overhead.out
ANYLOCK_BIN      = benchmarks/bench_overhead_anylock.out
ANYLOCK4_BIN     = benchmarks/bench_overhead_anylock4.out
CSLEN_BIN        = benchmarks/bench_overhead_cslen.out
LATENCY_BIN      = benchmarks/bench_latency.out
PEDGES_BIN       = benchmarks/bench_potential_edges.out

# Thread counts to sweep
THREAD_COUNTS = 1 2 4 8 16 32 64

# Iterations per thread (tune down if runtime is too long)
ITERS = 100000

# Critical section hold times to sweep (nanoseconds)
CS_LENGTHS = 0 10 20 30 50 75 100 150 200 300 500 750 1000 10000 100000

# Threads for cslen experiment
CSLEN_THREADS = 8

.PHONY: all build run overhead overhead-anylock overhead-anylock4 overhead-cslen latency potential-edges clean

all: build

build: $(LOCKDEP_LIB) $(BENCHMARKS)

$(LOCKDEP_LIB):
	$(MAKE) -C lockdep

$(BENCHMARKS) $(OVERHEAD_BIN) $(ANYLOCK_BIN) $(ANYLOCK4_BIN) $(CSLEN_BIN) $(LATENCY_BIN) $(PEDGES_BIN):
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

# ---------------------------------------------------------------------------
# Overhead experiment — critical section length
#
# 1 shared lock, fixed thread count. Vary busy-spin hold time inside the
# critical section: 0, 100ns, 1us, 10us, 100us.
# Columns: threads  iters  cs_ns  wall_ns  total_ops  ops_per_sec
# ---------------------------------------------------------------------------
overhead-cslen: build $(CSLEN_BIN)
	@echo "=== Critical section length ($(CSLEN_THREADS) threads, 1 shared lock) ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\titers\tcs_ns\twall_ns\ttotal_ops\tops_per_sec\n"
	@for cs in $(CS_LENGTHS); do \
		./$(CSLEN_BIN) $(CSLEN_THREADS) $(ITERS) $$cs; \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\titers\tcs_ns\twall_ns\ttotal_ops\tops_per_sec\n"
	@for cs in $(CS_LENGTHS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(CSLEN_BIN) $(CSLEN_THREADS) $(ITERS) $$cs; \
	done

# ---------------------------------------------------------------------------
# Latency experiment — per-operation lock/unlock delay
#
# Measures average nanoseconds per lock and per unlock call individually.
# Sweep thread counts to show how contention affects per-op latency.
# Columns: threads  iters  avg_lock_ns  avg_unlock_ns  avg_pair_ns
# ---------------------------------------------------------------------------
latency: build $(LATENCY_BIN)
	@echo "=== Per-operation latency (1 shared lock) ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\titers\tavg_lock_ns\tavg_unlock_ns\tavg_pair_ns\n"
	@for t in $(THREAD_COUNTS); do \
		./$(LATENCY_BIN) $$t $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep ---"
	@printf "threads\titers\tavg_lock_ns\tavg_unlock_ns\tavg_pair_ns\n"
	@for t in $(THREAD_COUNTS); do \
		LD_PRELOAD=./$(LOCKDEP_LIB) ./$(LATENCY_BIN) $$t $(ITERS); \
	done

# ---------------------------------------------------------------------------
# Potential-graph construction experiment
#
# Each thread owns a private set of locks_per_thread mutexes (disjoint across
# threads) and acquires them in fixed order, releases reverse. App-level mutex
# contention is zero, so the lockdep delta is the cost of potential-deadlock
# graph construction (held-stack + dependency-edge + cycle-check / enqueue).
#
# Sweeps the trade-off between thread count and per-thread nesting depth at
# (mostly) fixed total lock budget. PEDGES_PAIRS is "threads:depth" tuples.
# Constraints (lockdep.h defaults):
#   threads * depth <= 256 (LOCKDEP_MAX_LOCK_SLOTS)
#   depth           <= 64  (LOCKDEP_MAX_HELD_LOCK_SLOTS)
#   threads         <= 128 (LOCKDEP_MAX_THREAD_SLOTS)
# Columns: threads  locks_per_thread  iters  wall_ns  total_ops  ops_per_sec
# ---------------------------------------------------------------------------
PEDGES_PAIRS = 1:64 4:64 8:32 16:16 32:8 64:4

potential-edges: build $(PEDGES_BIN)
	@echo "=== Potential-graph construction (disjoint locks, threads:depth pairs) ==="
	@echo "--- baseline (no lockdep) ---"
	@printf "threads\tlocks_per_thread\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for p in $(PEDGES_PAIRS); do \
		t=$${p%:*}; d=$${p#*:}; \
		./$(PEDGES_BIN) $$t $$d $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep (global) ---"
	@printf "threads\tlocks_per_thread\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for p in $(PEDGES_PAIRS); do \
		t=$${p%:*}; d=$${p#*:}; \
		LOCKDEP_MODE=global LD_PRELOAD=./$(LOCKDEP_LIB) ./$(PEDGES_BIN) $$t $$d $(ITERS); \
	done
	@echo ""
	@echo "--- with lockdep (rb) ---"
	@printf "threads\tlocks_per_thread\titers\twall_ns\ttotal_ops\tops_per_sec\n"
	@for p in $(PEDGES_PAIRS); do \
		t=$${p%:*}; d=$${p#*:}; \
		LOCKDEP_MODE=rb LD_PRELOAD=./$(LOCKDEP_LIB) ./$(PEDGES_BIN) $$t $$d $(ITERS); \
	done

clean:
	$(MAKE) -C lockdep clean
	$(MAKE) -C benchmarks clean
