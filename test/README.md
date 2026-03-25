# Test Suite for Node Deadlock Detection

This test suite contains 16 carefully crafted test cases covering various deadlock scenarios and safe concurrent programming patterns.

## Overview

### Test Categories

#### 1. Purely Non-Deadlock Cases (6 tests)
These tests demonstrate safe concurrent patterns that provably cannot deadlock.

- **test_01** - Single thread, trivial case
- **test_02** - Multiple threads on a single lock (sequential access prevents deadlock)
- **test_03** - Two threads lock in same order (A→B)
- **test_04** - Multiple threads with 3 locks acquired in fixed order (A→B→C)
- **test_05** - Two threads with completely separate lock sets (no interference)
- **test_06** - Mostly lock-free computation with minimal critical sections

**Expected behavior:** All complete successfully within a few seconds.

#### 2. Deadlock Cases (4 tests)
These tests intentionally create deadlock situations that should not terminate.

- **test_07** - Classic 2-thread deadlock (T1: A→B, T2: B→A)
- **test_08** - Circular 3-thread deadlock (T1: A→B, T2: B→C, T3: C→A)
- **test_09** - Thread attempts to acquire same mutex twice (recursive lock on non-recursive mutex)
- **test_10** - 4-thread circular deadlock chain (A→B→C→D→A)

**Expected behavior:** These will hang indefinitely (timeout typically used to detect).

#### 3. Dubious Non-Deadlock Cases (3 tests)
These tests use patterns that are unsafe in general but may not deadlock in practice due to timing or implementation details. They illustrate the dangers of relying on luck rather than correct synchronization discipline.

- **test_11** - Threads randomly select lock order (might avoid deadlock by chance)
- **test_12** - Uses `pthread_mutex_trylock` with retry loop (avoids deadlock but risks livelock)
- **test_13** - Lock order determined by thread ID rather than explicit ordering

**Expected behavior:** May complete or deadlock depending on timing and thread interleaving.

#### 4. Mixed/Partial Deadlock Cases (3 tests)
These test scenarios where some threads deadlock while others proceed normally.

- **test_14** - Threads 1&2 deadlock (classic 2-lock deadlock), while threads 3&4 use independent locks and complete successfully
- **test_15** - Threads synchronize at barrier, then subset enters deadlock region while others continue
- **test_16** - Asymmetric pattern where threads 1&2 deadlock, thread 3 partially involved

**Expected behavior:** Some progress is made; partial output visible before deadlock is observed.

## Building

```bash
cd test
make              # Compile all tests
make clean        # Remove all compiled binaries
```

## Running Tests

### All Tests
```bash
make run_all      # Run all tests with automatic timeout (3 seconds each)
```

### Individual Tests
```bash
make run_test_01
make run_test_07
# etc
```

Or manually:
```bash
./test_01
timeout 3 ./test_07  # For expected-deadlock tests
```

## Test Output Interpretation

### Successful Tests (Non-deadlock)
```
[T1] Critical section 0
[T2] Critical section 0
...
Test N PASSED: ...
```

### Deadlock Tests
Most output will appear, then the test will hang and be killed by timeout:
```
[T1] locking A
[T1] locked A
[T2] locking B
[T2] locked B
[T1] locking B
[T2] locking A
(hangs here)
Test N: timeout or error (likely deadlock)
```

## Using with Deadlock Detection Tools

These tests are designed to work with deadlock detection tools (lockdep, ThreadSanitizer, custom analyzers, etc.). 

- **For safe tests:** Verify that the tool correctly identifies these as safe
- **For deadlock tests:** Verify that the tool successfully detects the deadlock scenario
- **For dubious tests:** Tools should ideally flag these as suspicious or provide warnings

Example with ThreadSanitizer:
```bash
gcc -fsanitize=thread -pthread test_07_deadlock_2thread_basic.c -o test_deadlock
./test_deadlock  # May detect data races if shared data is involved
```

## Complexity Progression

### Simple (Easy for Analysis)
- Tests 1-3: Single lock or ordered locks
- Test 7: Simple 2-thread, 2-lock deadlock

### Moderate (Requires Cycle Detection)
- Tests 4-6: Multiple locks or complex patterns
- Tests 8-10: Multi-thread cycles

### Complex (Edge Cases)
- Tests 11-16: Dubious patterns and partial deadlocks

## Key Patterns Demonstrated

1. **Lock Ordering** (Tests 3-4): All threads acquire locks in the same order
2. **Circular Wait** (Tests 7-10): Thread cycles waiting for resources held by each other
3. **Non-blocking Alternatives** (Test 12): Using try-lock to avoid blocking
4. **Partial Failures** (Tests 14-16): Subset of system deadlocks while rest proceeds

## Notes for Deadlock Detector Development

These tests can be used to:
- Validate deadlock detection accuracy
- Measure false positive/negative rates
- Test performance with various thread/lock counts
- Verify handling of edge cases (self-deadlock, partial deadlock)
- Check scalability (tests can be modified to increase thread/lock counts)

## Modifying Tests

To create variants:
- Increase `ITERS` or loop counts for longer-running tests
- Add more threads or locks
- Vary timing (sleeping) to affect race conditions
- Add additional output for debugging

Example modification to test_04:
```c
static const int NUM_THREADS = 40;   // Change from 8
static const int ITERS = 10000;      // Change from 100
```
