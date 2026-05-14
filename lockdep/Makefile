CC = gcc
CFLAGS = -O3 -g -Wall -Wextra -std=gnu11 -fPIC -pthread -flto -fno-semantic-interposition
LDFLAGS = -shared -ldl -pthread -flto

TARGET = liblockdep.so
SRCS = hook.c core.c graph.c log.c potential.c state.c
OBJS = $(SRCS:.c=.o)
HDR = lockdep.h
SMOKE_TEST = test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

smoke-test: $(SMOKE_TEST)

$(SMOKE_TEST): test.c
	$(CC) -O2 -g -Wall -Wextra -std=gnu11 -pthread -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) $(SMOKE_TEST)

.PHONY: all clean smoke-test
