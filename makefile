CC = gcc
CFLAGS = -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

TARGET = myshell

SRCS = main.c my_shell.c sighandler.c jobcontrol.c parser.c dynamicstring.c tokenizer.c
OBJS = $(SRCS:.c=.o)

HEADERS = my_shell.h sighandler.h jobcontrol.h parser.h dynamicstring.h tokenizer.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean