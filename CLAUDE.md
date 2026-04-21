# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nodeadlock** is a transparent user-space deadlock detector for multi-threaded C applications. It works by intercepting `pthread_mutex_*` calls via `LD_PRELOAD` without any modifications to the target application. It detects both **potential deadlocks** (circular lock dependency graphs) and **actual deadlocks** (real circular wait chains).

## Build Commands

```bash
# Build everything (library + benchmarks)
make build          # from project root

# Build just the shared library
make -C lockdep

# Build just benchmarks
make -C benchmarks

# Build test suite (requires liblockdep.so to exist first)
make -C lockdep && make -C test

# Clean everything
make clean
```

## Running Tests

```bash
# Full test suite with confusion matrix (16 tests)
./run_tests.sh

# Run individual benchmark under lockdep
LD_PRELOAD=./lockdep/liblockdep.so ./benchmarks/deadlock_2thread_2lock.out

# With debug output
LOCKDEP_DEBUG=1 LD_PRELOAD=./lockdep/liblockdep.so ./benchmarks/correct_40threads_3locks_10000iter.out

# With ring-buffer backend
LOCKDEP_MODE=rb LD_PRELOAD=./lockdep/liblockdep.so ./benchmarks/correct_40threads_3locks_10000iter.out
```

Exit code `66` means deadlock was detected. Exit code `0` means clean run. `run_tests.sh` uses a 5-second timeout — if a test hangs (actual deadlock without detection), it is killed and counted as a deadlock.

## Benchmarking

```bash
make run              # Run correctness/deadlock benchmarks under lockdep
make overhead         # Overhead sweep: 1..64 threads, high + low contention
make overhead-anylock # 2-lock any-acquire overhead sweep
make overhead-anylock4# 4-lock any-acquire overhead sweep
make overhead-cslen   # Vary critical-section hold time (0–100000 ns)
make latency          # Per-operation lock/unlock latency sweep
```

## Environment Variables

| Variable | Values | Description |
|----------|--------|-------------|
| `LOCKDEP_DEBUG` | `1` | Enable verbose debug logging to stderr |
| `LOCKDEP_MODE` | `global` (default), `rb` | Potential-deadlock backend selection |

## Architecture

The library lives in `lockdep/` with six C source files plus a header:

### Detection Modes

- **`global`** (default): Synchronous graph updates. On each nested acquire, adds dependency edges to a global adjacency matrix and runs DFS cycle detection under `g_meta_lock`.
- **`rb`** (ring-buffer): Asynchronous. Nested lock events are enqueued into per-thread lock-free ring buffers. A dedicated background worker thread drains the queues, reconstructs held-lock state, and runs its own graph/cycle detection. Actual deadlock detection remains synchronous in both modes.

### Execution Flow for `pthread_mutex_lock(mutex)`

1. **hook.c** intercepts the call via `dlsym(RTLD_NEXT, ...)`. Uses a `__thread` flag to prevent recursive interception.
2. Try a non-blocking `trylock` first (fast path for uncontended locks).
3. If lock is busy, call `lockdep_before_blocking_mutex_lock()` in **core.c** to check for actual deadlock by walking the wait chain: current thread → target lock → lock owner → that owner's waited-on lock → ... If the chain cycles back to the current thread, report and `_exit(66)`.
4. Call the real `pthread_mutex_lock()` to block.
5. After acquiring, call `lockdep_acquire_mutex()` in **core.c** which dispatches to the active potential-deadlock backend:
   - **global mode**: calls `lockdep_add_edge_and_check_cycle()` in **graph.c** (adds dependency edges from currently-held locks → new lock, DFS cycle check)
   - **rb mode**: calls `lockdep_potential_on_acquire()` in **potential.c** (enqueues event into per-thread ring buffer, signals worker)

### Key Subsystems

| File | Responsibility |
|------|---------------|
| `hook.c` | `LD_PRELOAD` hooking, fast-path trylock, recursive call guard |
| `core.c` | Orchestrates detection on acquire/release/before-block; actual deadlock wait-chain walking |
| `graph.c` | Global-mode backend: `uint8_t g_dependency_graph[MAX_LOCKS][MAX_LOCKS]` adjacency matrix; DFS-based cycle detection with predecessor-bit optimization |
| `potential.c` | Backend selection (`global`/`rb`); ring-buffer worker thread, per-thread SPSC queues, async graph/reachability maintenance |
| `state.c` | Global lock/thread slot registries; per-thread TLS held-lock stack; TLS lock-slot cache |
| `log.c` | Formats deadlock reports to stderr |

### Performance Optimizations

- **TLS lock-slot cache** (`LOCKDEP_TLS_LOCK_CACHE_SIZE 64`): Hashed per-thread cache avoids global lookups for mutex→slot resolution on every operation.
- **Fast paths**: Top-level acquire (no held locks) and top-level release (held count == 1) bypass general logic.
- **Predecessor summaries**: Both backends track known predecessors per lock, skipping redundant graph work for repeated dependency edges.
- **LIFO held-stack removal**: Normal nested unlock order uses O(1) pop from top of held-lock stack.
- **Compile flags**: `-flto -fno-semantic-interposition` for cross-unit inlining of hot-path helpers.

### Limits (all in `lockdep.h`)

```c
#define LOCKDEP_MAX_LOCK_SLOTS     256  // distinct mutexes tracked
#define LOCKDEP_MAX_HELD_LOCK_SLOTS 64  // locks held simultaneously per thread
#define LOCKDEP_MAX_THREAD_SLOTS   128  // threads tracked
#define LOCKDEP_RB_CAPACITY       4096  // per-thread ring buffer entries
#define LOCKDEP_TLS_LOCK_CACHE_SIZE 64  // per-thread mutex→slot cache
```

### Concurrency Model

- `g_meta_lock` (atomic spinlock) protects all global state in `global` mode (slot registries, dependency graph).
- Per-thread held-lock state uses `__thread` TLS — no locking needed.
- `rb` mode: application threads write to per-thread SPSC ring buffers (lock-free); a single background worker owns the graph.
- `_exit(66)` is used on deadlock (not `exit()`) for determinism.

## Test Suite (`run_tests.sh`)

16 test cases organized into four categories:
- **tests 01–06**: Purely non-deadlock (ordered locking, disjoint locks, lock-free)
- **tests 07–10**: Guaranteed deadlock (2-thread AB/BA, 3-thread circular, self-deadlock, 4-thread cycle)
- **tests 11–13**: Dubious/unsafe ordering (random locks, trylock-retry, dynamic selection)
- **tests 14–16**: Mixed/partial deadlock (subset of threads block)

Output includes a confusion matrix with accuracy, precision, and recall.

## Branch Structure

- **`main`**: Stable branch with the original synchronous global-mode detector.
- **`scale`**: Performance-focused branch adding the ring-buffer (`rb`) backend, per-thread SPSC queues, predecessor summaries, fast paths, and TLS caching. See `lockdep/README.md` "Optimizations" section for a detailed comparison.

## Plotting

Benchmark results can be visualized with Python scripts in `scripts/`:
- `plot_overhead.py`, `plot_overhead-any-of.py`, `plot_overhead-cslen.py`, `plot_latency.py`
- Output PDFs go to `plots/`.
