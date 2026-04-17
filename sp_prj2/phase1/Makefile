CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = myshell
SRCS = myshell.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

myshell.o: myshell.c myshell.h
	$(CC) $(CFLAGS) -c myshell.c

clean:
	rm -f $(TARGET) $(OBJS)