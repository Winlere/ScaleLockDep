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
- `LOCKDEP_MODE=rb`: enqueue nested lock-order edge batches into per-thread ring
  buffers and let a background worker build the graph and detect cycles
  asynchronously.
- `LOCKDEP_RB_FULL=stall`: in `rb` mode, preserve exact potential-deadlock
  tracking by waiting when a per-thread ring is full. This is the default.
- `LOCKDEP_RB_FULL=drop`: in `rb` mode, drop edge batches when a per-thread ring
  is full. Actual-deadlock detection remains exact, but potential-deadlock
  reporting becomes approximate.

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

## Reports

Deadlock reports include lock slots, mutex addresses, participating thread
slots, Linux thread IDs, and best-effort source locations for lock operations.

For actual deadlocks, the report prints the live wait chain and annotates each
edge with:

- the thread waiting for a mutex
- the source location of the blocking lock attempt
- the thread that owns the mutex
- the source location where the owner acquired that mutex

For potential deadlocks, the report prints the new dependency edge, the existing
dependency path that closes the cycle, and the thread/source location where each
historical lock-order edge was observed.

Source locations are resolved with `addr2line` on the reporting path. They are
most useful when the target program is built with debug symbols such as `-g`.
If source information is unavailable, the report falls back to symbols and raw
addresses.

## Optimizations Compared With The Original Non-`rb` Backend

The original non-`rb` design performs potential-deadlock graph updates
synchronously in the application thread. Every nested acquire can become a
global metadata operation: walk the currently held locks, add dependency edges,
update reachability, and report a cycle if the new edge closes one.

This version keeps the same high-level semantics, but separates the two
detection problems by urgency:

- actual deadlock detection remains synchronous because it must run before a
  thread blocks on a busy mutex
- potential deadlock detection can run asynchronously because it reports
  historical lock-order cycles, not an immediately blocking wait cycle

That split is the main scalability idea in the final design. The lock path still
does the safety-critical wait-for check, while the more expensive historical
graph maintenance can be moved out of the application thread.

### Backend Selection

`LOCKDEP_MODE` selects the potential-deadlock backend:

- `global`: the original-style synchronous dependency graph backend
- `rb`: the optimized ring-buffer backend

Both modes share the same actual-deadlock detector. In other words, changing
`LOCKDEP_MODE` changes how historical lock-order cycles are detected, but it
does not weaken live deadlock detection.

### Per-Thread Ring Buffers

The optimized `rb` backend gives each application thread its own producer ring.
This avoids putting all nested lock-order events through one shared application
thread-side graph lock. Application threads append compact records locally, and
a background worker later consumes those records.

Compared with the original synchronous backend, this changes the critical path
from "update the global potential graph now" to "publish a small event to the
current thread's ring." That is the same broad scalability pattern used by many
parallel systems: keep the hot path local and move aggregation to a background
or less frequent path.

### Compact Edge-Batch Logging

Potential deadlock detection only needs lock-order dependencies. It does not
need a complete replay of every acquire and release. The `rb` backend therefore
logs edge batches instead of lock lifecycle events.

On a nested acquire, the application thread already knows which locks it holds.
It builds an event containing:

- the newly acquired lock slot
- a bitset of currently held predecessor lock slots that represent candidate
  `held_lock -> new_lock` edges

Unlocks do not enqueue potential-deadlock events, because a release does not
create a new historical lock-order dependency. This removes worker-side held
state reconstruction and keeps the async log focused on graph information.

### Front-End Summaries And Skip Paths

Stable workloads usually repeat the same lock orders many times. Reprocessing
known dependency edges is wasted work, so this implementation keeps predecessor
summaries that let the front end identify already-known edges.

In `global` mode, those summaries let the application thread skip redundant
synchronous graph updates. In `rb` mode, the front end uses atomic predecessor
snapshots plus a per-thread held-lock bitset to decide whether a nested acquire
contains any new graph information. If every `held_lock -> new_lock` edge is
already known, the thread updates only its local held-lock state and avoids
queueing an event.

This makes repeated ordered locking converge toward a cheap fast path.

### Worker-Owned Graph And Batch Deduplication

The `rb` worker owns a separate dependency graph, predecessor summaries, and
reachability matrix for potential-deadlock detection. Application threads do
not mutate this graph directly.

The worker also deduplicates work before touching the graph. It drains pending
ring events into worker-local bitsets, coalesces duplicate predecessors for each
target lock, filters known predecessors once per batch, and then updates the
graph only for genuinely new edges.
This reduces graph-update pressure when many events describe the same
lock-order information.

### Ring-Buffer Backpressure Policy

The original synchronous backend cannot overflow an async queue because it never
defers potential graph updates. Once the `rb` backend introduces bounded
per-thread rings, it needs an explicit overflow policy.

The default policy is exact:

```bash
LOCKDEP_RB_FULL=stall
```

When a ring is full, the producer waits until the worker drains space. This
preserves exact potential-deadlock reporting.

For profiling-style runs, the backend also supports:

```bash
LOCKDEP_RB_FULL=drop
```

This drops potential-deadlock edge batches under ring pressure. Actual-deadlock
detection remains synchronous and exact, but potential-deadlock reporting
becomes approximate while the queue is saturated.

Normal runs keep `stall` as the default so the detector remains exact unless
the user explicitly requests approximate profiling behavior.

### Hot-Path Metadata Reductions

The final implementation also reduces fixed per-lock overheads that existed in
the original non-`rb` path:

- thread-slot lookups use thread-local caching
- mutex-address to lock-slot resolution uses a thread-local cache
- owner and waiting metadata use atomic hot-path updates where possible
- debug logging is guarded and disabled by default
- top-level acquire and release have dedicated fast paths
- normal LIFO nested unlocks avoid the general held-stack removal path

These optimizations matter even when potential-deadlock detection has no new
graph edge to report, because they reduce the constant cost paid by every
interposed `pthread_mutex_lock` and `pthread_mutex_unlock`.

### Worker Runtime Behavior

The `rb` worker is started lazily and sleeps on a semaphore when there is no
pending work. This avoids idle polling and avoids creating the worker at all for
runs that never need the ring-buffer backend.

The worker drains all thread rings, processes events in batches, and publishes
predecessor summaries back to the front-end skip path. The design still has one
central worker and one worker-owned graph, but the amount of work sent to that
central point is reduced by edge-batch logging, front-end summaries, and
worker-side deduplication.

### Build Configuration

The shared library build uses optimization-oriented compiler settings for the
interposition hot path. These settings help the compiler inline and simplify
small helper functions that are crossed on every mutex operation.

### Prototype Scope

This is still a course-project prototype rather than a production replacement
for kernel lockdep:

- the graph is fixed-size and bitset/matrix based
- mutex addresses are treated as lock nodes
- the `rb` backend uses one worker-owned graph

Those choices keep the implementation compact and make the comparison against
the original synchronous backend clean. A production design would likely add
dynamic lock classes, call-site or allocation-site grouping, larger sparse graph
structures, and richer reports with stack traces.
