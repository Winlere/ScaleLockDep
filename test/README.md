# Test Suite — Nodeadlock Deadlock Detector

30 test cases grouped into three categories that match the `Expected:` line in
each test source file:

| Category             | Expected line                  | Tests                                 | Count |
|----------------------|--------------------------------|---------------------------------------|------:|
| NON-DEADLOCK         | `no deadlock detected`         | 01–06, 12, 17–22                      | 13    |
| DEADLOCK             | `deadlock detected`            | 07–10, 14–16, 23–26, 29–30            | 13    |
| POTENTIAL-DEADLOCK   | `potential deadlock detected`  | 11, 13, 27, 28                        | 4     |

Run the full suite from the project root:

```bash
./run_tests.sh          # all 30 tests, confusion matrix
make -C test run_all    # build + run without lockdep (for reference)
```

Exit codes: `66` = lockdep detected a cycle; `124` = timeout (actual hang
without detection); `0` = clean run.

---

## Motivating Literature

Three well-cited papers shaped the design of this test suite.

**[1] Coffman, E. G., Elphick, M., & Shoshani, A. (1971). "System Deadlocks." *ACM Computing Surveys*, 3(2), 67–78.**
Established the four necessary and sufficient conditions for deadlock —
mutual exclusion, hold-and-wait, no preemption, and circular wait — plus the
resource allocation graph formalism. Every NON-DEADLOCK test violates at
least one of these conditions structurally.

**[2] Lu, S., Park, S., Seo, E., & Zhou, Y. (2008). "Learning from Mistakes: A Comprehensive Study on Real World Concurrency Bug Characteristics." *ASPLOS '08*, pp. 329–339.**
Studied 105 real concurrency bugs across Apache, MySQL, Mozilla, and
OpenOffice. ~30 % were deadlocks; ~22 % of those were single-thread
self-deadlocks; the dominant root cause was lock-order inversion, and the
authors note that pairwise testing of acquire/release sequences suffices to
catch most deadlock bugs. This motivates why most of our DEADLOCK tests
involve exactly two resources in circular order and why POTENTIAL-DEADLOCK
tests target inter-thread orderings that only manifest under specific
interleavings.

**[3] Engler, D., & Ashcraft, K. (2003). "RacerX: Effective, Static Detection of Race Conditions and Deadlocks." *SOSP '03*, pp. 237–252.**
Demonstrated static lock-order analysis on real kernel code, finding 26
previously unknown deadlock bugs in Linux, OpenBSD, and other systems. The
dominant pattern: code path A acquires lock X while holding Y; code path B
acquires Y while holding X. Test 24 directly models this layered-inversion
class.

---

## Category 1 — Non-Deadlock (tests 01–06, 12, 17–22)

Structurally safe: each test violates at least one Coffman condition.

**Consistent global ordering or single shared resource (01–05, 17, 19, 20).**
Either only one shared mutex exists (so "another resource to wait for" doesn't),
or every thread acquires shared locks in the same fixed order. test_05 uses
entirely disjoint lock sets across threads. The scaled-up variants (test_19:
16 threads × 4 locks; test_20: 8 threads × local→global hierarchy)
verify the detector doesn't false-positive on a busy DAG.

**No hold-and-wait (06, 18, 21, 22).**
Threads never hold one lock while blocking on another. test_21 releases each
lock fully before acquiring the next; test_06 keeps a single non-nested
critical section inside an otherwise lock-free workload; test_18 uses
`pthread_cond_wait`, which releases the mutex atomically before blocking;
test_22 keeps readers on the trylock fast path while a writer blocks alone.
The dependency graph stays edgeless regardless of contention.

**Trylock-with-retry (12).** Two threads want opposite orders but use
`pthread_mutex_trylock` with a bounded retry count, so neither blocks
indefinitely. The structurally inverted ordering is *not* sufficient to
deadlock when nobody blocks — this is the regression test against false
positives.

## Category 2 — Deadlock (tests 07–10, 14–16, 23–26, 29–30)

Genuine circular waits. Under lockdep the process exits 66; without lockdep
the test hangs and is killed by the 5 s timeout.

**Clear ownership cycles (07, 08, 10, 14–16, 23, 25, 26, 29, 30).**
One thread locks A, another locks B, then each waits for the other's lock —
the two-thread inversion that Lu et al. found dominates real-world
deadlocks. Generalised to N-thread rings (3-thread `A→B→C→A`, 4-thread
square, 5-thread pentagon, 6-thread hexagon) and to parallel independent
2-thread pairs sharing no locks. The MIXED-style variants (14–16, 29, 30)
embed the cycle inside a workload of progressing "safe" threads, exercising
detection mid-run.

**Lock-order inversion within a hierarchy (09, 24).**
A single code path reverses an established acquisition order. test_24
directly models the RacerX pattern (Engler & Ashcraft): a "high-then-low"
layer convention violated by one "low-then-high" path. test_09 is the
degenerate case — one thread re-locks its own non-recursive mutex (cycle of
length 1, exercising the actual-deadlock wait-chain walker).

## Category 3 — Potential Deadlock (tests 11, 13, 27, 28)

Unsafe orderings that the dependency-graph backend flags immediately, but
where an actual hang depends on scheduling.

**test_11 — round-robin alternating order.**
Two threads alternate between `A→B` on even iterations and `B→A` on odd
iterations under the guise of "balancing hot-lock contention." Both edges
land in the global graph within a single run; the cycle is flagged on the
first conflicting iteration even though no thread sees the inversion locally.

**test_13 — role-based lock selection.**
Three threads pick their lock pair by `tid % 3`: T0 takes `A→B`, T1 takes
`B→C`, T2 takes `C→A`. The cycle `A→B→C→A` exists only across the *union*
of thread behaviours — no individual thread carries the inversion. This is
the distributed-lock-order pattern RacerX was built to surface.

**test_27 — seeded-random permutations.**
Four threads each derive a fixed TID-seeded permutation of four locks via an
LCG shuffle and run 30 iterations. Each thread's own ordering is internally
consistent, but the threads disagree pairwise; the first incompatible pair of
edges trips the detector.

**test_28 — long safe chain with single inversion edge.**
Five "safe" threads iteratively build the chain `L0→L1→L2→L3→L4`. One
"buggy" thread acquires `L4` then `L0`, contributing the single back-edge
that closes a 5-edge cycle. The most direct mapping of the Engler & Ashcraft
layered-inversion pattern onto a long lock hierarchy.

---

## Confusion Matrix Summary (expected with lockdep global mode)

|                  | Predicted DEADLOCK | Predicted NO-DEADLOCK |
|------------------|-------------------:|----------------------:|
| Actual DEADLOCK  |                 17 |                     0 |
| Actual NO-DEADLOCK |               0 |                    13 |

TP = 17 (07–10, 11, 13, 14–16, 23–30), TN = 13 (01–06, 12, 17–22),
FP = 0, FN = 0. Accuracy / Precision / Recall = 100 %.
