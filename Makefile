LOCKDEP_LIB = lockdep/liblockdep.so

BENCHMARKS = \
	benchmarks/correct_40threads_3locks_10000iter.out \
	benchmarks/correct_40threads_40locks_10000iter.out \
	benchmarks/deadlock_2thread_2lock.out \
	benchmarks/deadlock_3thread_circular.out

.PHONY: all build run clean

all: build

build: $(LOCKDEP_LIB) $(BENCHMARKS)

$(LOCKDEP_LIB):
	$(MAKE) -C lockdep

$(BENCHMARKS):
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

clean:
	$(MAKE) -C lockdep clean
	$(MAKE) -C benchmarks clean
