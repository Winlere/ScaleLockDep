CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -std=gnu11 -fPIC -pthread
LDFLAGS = -shared -ldl -pthread

TARGET = liblockdep.so
SRC = lockdep.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
