CC = gcc
CFLAGS = -Wall -Wextra -pthread

SRCS = correct_40threads_3locks_10000iter.c \
       correct_40threads_40locks_10000iter.c \
       deadlock_2thread_2lock.c \
       deadlock_3thread_circular.c \
       bench_overhead.c \
       bench_overhead_anylock.c \
       bench_overhead_anylock4.c \
       bench_overhead_cslen.c

BINS = $(SRCS:.c=.out)

all: $(BINS)

%.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)

.PHONY: all clean
