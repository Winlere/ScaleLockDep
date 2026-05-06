# Test Suite — Nodeadlock Deadlock Detector

30 test cases organized into four categories. Run the full suite from the project root:

```bash
./run_tests.sh          # all 30 tests, confusion matrix
make -C test run_all    # build + run without lockdep (for reference)
```

Exit codes: `66` = lockdep detected potential cycle; `124` = timeout (actual hang); `0` = clean.

---

## Motivating Literature

Three well-cited papers shaped the design of this test suite:

**[1] Coffman, E. G., Elphick, M., & Shoshani, A. (1971). "System Deadlocks." *ACM Computing Surveys*, 3(2), 67–78. doi:10.1145/356586.356588 (~2,800 citations)**
The foundational paper that established the four *necessary and sufficient* conditions for deadlock — mutual exclusion, hold-and-wait, no preemption, and circular wait — along with the resource allocation graph formalism. Tests 01–22 are designed so that at least one Coffman condition is structurally violated (e.g., single lock → no hold-and-wait; sequential non-nested → no hold-and-wait; consistent ordering → no circular wait).

**[2] Lu, S., Park, S., Seo, E., & Zhou, Y. (2008). "Learning from Mistakes: A Comprehensive Study on Real World Concurrency Bug Characteristics." *ASPLOS 2008*, pp. 329–339. doi:10.1145/1346281.1346323 (~1,300 citations)**
Studied 105 real concurrency bugs across Apache, MySQL, Mozilla, and OpenOffice. Found that ~31% were deadlocks, 97% involved two or fewer threads, and the dominant root cause was lock-order inversion. This finding motivates why most of our deadlock tests involve exactly two resources in circular order, and why the dubious category (tests 11–13, 27–28) captures hazards that only manifest under specific interleavings.

**[3] Engler, D., & Ashcraft, K. (2003). "RacerX: Effective, Static Detection of Race Conditions and Deadlocks." *SOSP 2003*, pp. 237–252. doi:10.1145/945445.945468 (~700 citations)**
Demonstrated static lock-order analysis on real kernel code, finding 26 previously unknown deadlock bugs in Linux, OpenBSD, and other systems. The dominant pattern: code path A acquires lock X while holding Y; code path B acquires Y while holding X. Test 24 (layered lock inversion) directly models this class of bug.

---

## Category 1 — Purely Non-Deadlock (tests 01–06, 17–22)

These tests are structurally safe: they violate at least one Coffman condition by design.

### test_01 — Single Thread, Single Lock
A single thread acquires and releases the same mutex 100 times sequentially. With only one thread there is no concurrent access, so none of the Coffman conditions for deadlock can be satisfied. Serves as the simplest sanity check for the detector.

### test_02 — Two Threads, One Lock
Two threads compete for a single shared mutex. Because only one resource exists, a circular wait is impossible — you cannot hold one lock and wait for another when there is no "another." Checks that the detector does not generate false positives under high single-lock contention.

### test_03 — Ordered Locking (2 threads, 2 locks)
Two threads each acquire locks A and B in the same fixed order A→B. Consistent global ordering is the textbook prevention strategy for circular wait (Coffman et al. 1971). No dependency cycle can form in the lock graph, so this is the canonical safe two-lock pattern.

### test_04 — Ordered Locking (8 threads, 3 locks)
Scales test_03 to 8 threads and 3 locks (A→B→C for every thread, 100 iterations). Tests that the detector handles a busier, larger dependency graph without false positives while all threads follow the same lock hierarchy.

### test_05 — Disjoint Lock Sets
Thread 1 uses only locks {A, B}; thread 2 uses only locks {C, D}. The two threads never share a resource, so the "mutual exclusion" Coffman condition is vacuous across thread boundaries. No matter what ordering is used within each thread, inter-thread circular wait is impossible.

### test_06 — Lock-Free Majority Workload
Three threads perform mostly computation (50 iterations each) with only a brief critical section protected by a single lock. The lock is used for exclusive access but never nested with another lock, so no dependency edge is ever added to the graph.

### test_17 — Lock Convoy (High-Contention Single Lock)
Ten threads each acquire a single shared mutex 200 times, creating a high-contention "lock convoy" where threads queue behind each other. A single lock cannot form a cycle — no hold-and-wait between multiple resources is possible — so this is a non-deadlock stress test for the detector's performance under load.

### test_18 — Producer-Consumer with Condition Variable
Two producer threads enqueue into a bounded buffer and two consumer threads dequeue, coordinating via a single `pthread_mutex_t` and `pthread_cond_wait`. Because `pthread_cond_wait` releases the mutex atomically before blocking, no thread ever holds a mutex while waiting for another — the hold-and-wait condition is never satisfied.

### test_19 — Scaled Ordered Locking (16 threads, 4 locks)
Sixteen threads each acquire four locks in identical fixed order A→B→C→D. This is a direct scale-up of test_04, verifying that the detector's graph structures (adjacency matrix, predecessor summaries, TLS caches) remain correct and free of false positives at higher thread counts.

### test_20 — Per-Thread Local Lock + Shared Global Lock
Eight threads each hold their own private local mutex and then acquire a single shared global mutex (local[i]→global for every thread). Because all threads follow the same local→global hierarchy, the dependency graph is a DAG (star topology pointing into the global lock) with no back-edge and therefore no cycle.

### test_21 — Sequential Non-Nested Acquisition
Four threads cycle through three locks A, B, C but release each fully before acquiring the next. No thread ever holds more than one lock simultaneously, so the hold-and-wait Coffman condition is structurally impossible. Even though threads share all three locks, no dependency edge is ever added to the graph.

### test_22 — Trylock Readers + Blocking Writer
Four reader threads spin with `pthread_mutex_trylock` and a 0.1 ms backoff; one writer uses blocking `pthread_mutex_lock`. No thread nests multiple locks, so the lock-dependency graph never acquires any edge regardless of contention level. Tests that the detector handles the `trylock` fast path without incorrectly building dependency edges.

---

## Category 2 — Deadlock (tests 07–10, 23–26)

These tests create genuine circular wait chains. With lockdep, the process exits with code 66 before the actual block; without lockdep (e.g., `make run_all`), it hangs and is killed by the 5-second timeout.

### test_07 — Classic 2-Thread, 2-Lock Deadlock
Thread 1 locks A then waits for B; thread 2 locks B then waits for A. A 200 ms sleep between acquisitions guarantees both threads hold their first lock before attempting their second, creating the minimal circular wait: T1→B→T2→A→T1. The canonical textbook example (Lu et al. found this pattern in 97% of real deadlocks).

### test_08 — 3-Thread Circular Deadlock
Three threads form a triangle: T1 holds A and waits for B; T2 holds B and waits for C; T3 holds C and waits for A. The dependency cycle A→B→C→A has length 3, requiring the DFS cycle detector to traverse two edges before finding the back-edge.

### test_09 — Self-Deadlock (Double Acquisition)
A single thread tries to lock the same non-recursive mutex twice. On the second lock attempt, it is already the owner, so it blocks on itself — an immediate, single-thread deadlock. Tests the actual-deadlock wait-chain walker (current thread → lock owner = current thread → cycle of length 1).

### test_10 — 4-Thread Circular Deadlock
Four threads form a square: T1:A→B, T2:B→C, T3:C→D, T4:D→A. Extends test_08 to a 4-node cycle, requiring the DFS to traverse three edges before closing the cycle. Tests that cycle detection scales to larger dependency subgraphs.

### test_23 — 5-Thread Pentagon Deadlock
Five threads form a length-5 circular wait: T1:A→B, T2:B→C, T3:C→D, T4:D→E, T5:E→A. All five sleep 200 ms after grabbing their first lock so all are simultaneously blocked when the cycle closes. Tests cycle detection on a 5-node subgraph and a 5-thread actual-deadlock wait-chain traversal.

### test_24 — Layered Lock Inversion (RacerX Pattern)
Models the lock-layer inversion bug class documented by Engler & Ashcraft (SOSP 2003). A "correct" thread follows the layer hierarchy by acquiring the high-level lock H before the low-level lock L; a "buggy" thread inverts the order (L then H), creating cycle H→L→H. This is a common real-world anti-pattern in kernel and database code where modules have assumed lock ordering conventions that one code path violates.

### test_25 — 6-Thread Hexagon Deadlock
Six threads form a length-6 circular wait: T1:A→B, T2:B→C, T3:C→D, T4:D→E, T5:E→F, T6:F→A. Extends test_23 by one node, requiring the DFS to traverse five edges before closing the cycle. Tests that both the potential-deadlock graph backend and the actual-deadlock wait-chain walker correctly handle longer cycles.

### test_26 — Dual Independent Deadlock Pairs
Four threads form two completely independent 2-thread deadlocks: T1 and T2 deadlock over {A, B}, while T3 and T4 simultaneously deadlock over {C, D}. The two cycles share no locks and arise concurrently, testing whether the detector can report multiple simultaneous deadlock cycles and whether the first detected cycle causes a clean exit before the second is reached.

---

## Category 3 — Dubious (tests 11–13, 27–28)

These tests have unsafe lock orderings that the dependency-graph detector flags immediately (exit 66), but an actual deadlock depends on thread scheduling. Ground truth is 1 because the detector correctly identifies the potential hazard.

### test_11 — Round-Robin Alternating Lock Order
Two threads alternate between A→B (even iterations) and B→A (odd iterations), ostensibly to "balance hot-lock contention." Both orderings are exercised within a single run, so the lock graph accumulates both A→B and B→A edges, forming a cycle. The detector exits on the first iteration that introduces the conflicting edge; actual deadlock depends on whether both threads hit an opposite-order iteration simultaneously.

### test_12 — Trylock with Retry (Avoids Blocking Deadlock)
Two threads want to acquire A and B in opposite orders but use `pthread_mutex_trylock` and retry up to 100 times rather than blocking indefinitely. Because they never block permanently, actual deadlock cannot occur. Ground truth is 0 (no deadlock), making this a true-negative test for the detector.

### test_13 — Role-Based Dynamic Lock Selection
Three threads select their lock pair based on their thread ID modulo 3: T0 acquires A→B, T1 acquires B→C, T2 acquires C→A. Collectively they form the same closed cycle as test_08, but no single thread exhibits the inversion — the cycle only appears when all three roles run concurrently. The detector builds the global dependency graph across all threads and detects the cycle.

### test_27 — Seeded-Random Lock Ordering
Four threads each compute a fixed (TID-seeded) random permutation of 4 locks using an LCG shuffle and acquire locks in that order for 30 iterations. Different permutations create conflicting dependency edges in the global lock graph; the detector exits when the first conflicting pair is registered. Actual deadlock requires two threads with incompatible permutations to each hold their respective first lock simultaneously, which is scheduling-dependent.

### test_28 — Long Safe Chain with Single Inversion Edge
Five "safe" threads iteratively build the dependency chain L0→L1→L2→L3→L4. One "buggy" thread acquires L4 then L0, adding the single back-edge L4→L0 that completes the cycle L0→…→L4→L0. The detector immediately flags the cycle (exit 66) when the buggy thread runs; an actual deadlock requires the buggy thread to race with a safe thread that holds L0 while the buggy thread holds L4.

---

## Category 4 — Mixed / Partial Deadlock (tests 14–16, 29–30)

A subset of threads deadlocks while others continue or complete. The detector fires on the first deadlock it encounters (exit 66), so the "safe" threads may not finish in the lockdep run — but the expected outcome (deadlock = 1) is correct.

### test_14 — Partial Deadlock (2+2 Threads)
Threads 1 and 2 deadlock on {A, B} (classic inversion); threads 3 and 4 use independent locks {C, D} and complete freely. Demonstrates that a deadlock in one thread pair does not prevent other threads from making progress, and that the detector correctly identifies the partial failure.

### test_15 — Barrier then Deadlock
Four threads first synchronize at a `pthread_barrier_t`. After the barrier, threads 0 and 1 proceed safely while threads 2 and 3 enter a classic A↔B deadlock. Tests that the detector handles barriers correctly (they are not mutex operations) and still identifies the post-barrier lock cycle.

### test_16 — Asymmetric Deadlock
Three threads with asymmetric roles: T1 acquires A then B; T2 acquires B then A (deadlocks with T1); T3 acquires only B then C. T1 and T2 deadlock while T3 may or may not block on B depending on scheduling, illustrating that a deadlock can form in a subset of a thread group even when not all threads contribute to the cycle.

### test_29 — Triple-Pair Mixed Scenario
Six threads operate in three independent pairs. Pair 1 (T1, T2) deadlocks on {A, B}; pair 2 (T3, T4) deadlocks on {C, D}; pair 3 (T5, T6) safely acquires E then F in consistent order and completes freely. Two simultaneous deadlock cycles coexist with actively completing threads, testing the detector under a compound workload with multiple independent hazard zones.

### test_30 — Staggered Deadlock Under Active Safe Workload
Six "safe" threads each perform 20 iterations of ordered A→B acquisition and complete quickly. Two "dangerous" threads start with a 50 ms delay so that safe threads are already in-flight: danger1 acquires A→B while danger2 acquires B→A (inversion), forming a deadlock pair while the safe workload is ongoing. Tests that the detector fires correctly mid-workload when the inversion edge is added to a graph that already contains the safe A→B edge.

---

## Confusion Matrix Summary (expected with lockdep global mode)

| Category | Tests | Count | Expected |
|---|---|---|---|
| NON-DEADLOCK | 01–06, 17–22 | 12 | 12 TN |
| DEADLOCK | 07–10, 23–26 | 8 | 8 TP |
| DUBIOUS | 11–13, 27–28 | 5 | 4 TP + 1 TN (test_12) |
| MIXED | 14–16, 29–30 | 5 | 5 TP |
| **Total** | | **30** | **17 TP, 13 TN** |

Ideal: Accuracy = 100%, Precision = 100%, Recall = 100%.
