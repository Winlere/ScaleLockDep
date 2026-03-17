CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -std=gnu11 -fPIC -pthread
LDFLAGS = -shared -ldl -pthread

TARGET = liblockdep.so
SRCS = hook.c core.c log.c state.c graph.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c lockdep.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
