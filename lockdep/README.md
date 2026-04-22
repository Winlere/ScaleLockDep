# lockdep

This directory contains the user-space deadlock detector library. It interposes on
`pthread_mutex_*` with `LD_PRELOAD`, so existing applications can be checked
without source changes.

The library implements two detectors:

- `Potential deadlock`: records historical lock-order dependencies and reports
  cycles in the lock graph.
- `Actual deadlock`: tracks the current owner/wait relation and reports a live
  wait cycle before a thread blocks forever.

## Components

- `core.c`: coordinates runtime ownership tracking, actual deadlock checks, and
  potential-deadlock dispatch.
- `graph.c`: synchronous global dependency graph backend.
- `hook.c`: `pthread_mutex_lock`, `unlock`, and `trylock` interposition logic.
- `lockdep.h`: shared types, limits, and hot-path helpers.
- `log.c`: debug logs and deadlock reports.
- `potential.c`: potential-deadlock backend selection plus the ring-buffer
  backend.
- `state.c`: thread/lock registries, thread-local held-lock state, and cache
  helpers.

## Configuration

All limits are defined in `lockdep.h`:

- `LOCKDEP_MAX_LOCK_SLOTS`: maximum number of tracked mutexes. Default: `256`.
- `LOCKDEP_MAX_HELD_LOCK_SLOTS`: maximum number of simultaneously held locks per
  thread. Default: `64`.
- `LOCKDEP_MAX_THREAD_SLOTS`: maximum number of tracked threads. Default: `128`.
- `LOCKDEP_RB_CAPACITY`: per-thread ring-buffer capacity in `rb` mode.

Runtime environment variables:

- `LOCKDEP_DEBUG=1`: enable verbose debug logging. By default the library stays
  quiet except for deadlock reports.
- `LOCKDEP_MODE=global`: run synchronous graph updates in the lock path.
- `LOCKDEP_MODE=rb`: enqueue nested lock events into per-thread ring buffers and
  let a background worker rebuild held state, build the graph, and detect cycles
  asynchronously.

The actual-deadlock detector remains synchronous in both modes.

## Build

Run:

```bash
make
```

This builds `liblockdep.so`.

## Usage

Example:

```bash
LD_PRELOAD=$PWD/liblockdep.so ./test

LOCKDEP_DEBUG=1 LD_PRELOAD=$PWD/liblockdep.so ./test

LOCKDEP_MODE=rb LD_PRELOAD=$PWD/liblockdep.so ./test
```

## Optimizations Compared With The Original `lockdep` Branch

This branch keeps the original detection semantics, but restructures the
metadata path to reduce lock-path overhead and to support a more scalable
potential-deadlock backend.

### 1. Backend Split For Potential Deadlocks

The original branch used one centralized potential-deadlock path: successful
acquires updated the global graph synchronously under shared metadata
coordination.

This branch introduces explicit backend selection through `LOCKDEP_MODE`:

- `global`: preserves the original synchronous graph-update model.
- `rb`: moves potential-deadlock graph construction off the application lock
  path by logging nested lock events into per-thread ring buffers and consuming
  them in a background worker.

This keeps actual deadlock detection unchanged while making the potential path
swappable.

### 2. Asynchronous Ring-Buffer Backend

The `rb` backend adds the scalable metadata organization proposed for the final
stage:

- each application thread writes to its own single-producer ring buffer
- a dedicated internal worker thread consumes those events
- the worker reconstructs per-thread held-lock state
- the worker owns its own dependency graph and cycle-detection state

Compared with the original branch, potential-deadlock processing no longer has
to be completed synchronously by the thread that just acquired a nested lock.

### 3. Synchronous Actual Deadlock Detection Preserved

The original branch relied on a `trylock`-first path so that actual deadlock
could be checked before a thread blocks on a busy mutex.

That invariant is preserved here:

- the actual detector still runs before blocking
- the current wait chain is still checked synchronously
- actual deadlock reporting and exit behavior remain independent of
  `LOCKDEP_MODE`

Only the potential-deadlock backend changes between modes.

### 4. Reduced Front-End Work For Repeated Dependencies

The original branch always walked the held-lock set and updated the graph on
each nested acquire, even when all corresponding dependency edges had already
been seen before.

This branch adds predecessor summaries for both backends:

- in `global` mode, known predecessors let the lock path skip redundant graph
  work
- in `rb` mode, front-end threads can avoid entering tracking or queueing when
  the dependency is already known

This reduces repeated metadata work in stable lock-order workloads.

### 5. Smaller Hot-Path Metadata Cost

Several library-internal overheads from the original branch were reduced:

- thread-slot lookups use thread-local caching
- mutex-address to lock-slot resolution uses a thread-local cache
- debug logging is compiled into cheap guarded helpers and stays off by default
- owner and waiting state use atomic hot-path updates instead of routing common
  operations through the global metadata lock

These changes target the fixed cost paid on every successful lock/unlock pair.

### 6. Fast Paths For Common Locking Shapes

The original branch treated most acquires and releases through one general path.
This branch adds fast paths for common cases:

- top-level acquire fast path when the thread currently holds no locks
- top-level release fast path for the common `count == 1` case
- LIFO held-stack removal fast path for normal nested unlock order

These paths are especially relevant to shallow nesting and high-frequency mutex
microbenchmarks.

### 7. Worker Runtime Improvements In `rb` Mode

The ring-buffer backend also includes runtime improvements over a naive async
implementation:

- lazy worker startup, so the background thread is not created unless `rb` work
  is actually needed
- semaphore-based worker wakeup instead of idle polling
- worker-owned graph state, so `rb` mode does not reuse the synchronous global
  graph update path

This makes `rb` mode a distinct backend rather than a deferred wrapper around
the original one.

### 8. Build Configuration For Shared-Library Optimization

The library build now uses stronger optimization settings for the interposition
hot path, including LTO-oriented shared-library optimization flags. The goal is
to help the compiler inline and simplify small helper paths that are otherwise
expensive when crossed on every lock/unlock operation.
