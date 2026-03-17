CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -std=gnu11 -fPIC -pthread
LDFLAGS = -shared -ldl -pthread

TARGET = liblockdep.so
SRCS = hook.c core.c graph.c log.c state.c
OBJS = $(SRCS:.c=.o)
HDR = lockdep.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
