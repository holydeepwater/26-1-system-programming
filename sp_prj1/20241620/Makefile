CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g
TARGET = testlib

SRCS = main.c list.c hash.c bitmap.c debug.c hex_dump.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
