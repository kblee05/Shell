CC = gcc
CFLAGS = -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

TARGET = myshell

SRCS = main.c \
       my_shell.c \
       parser.c \
       tokenizer.c \
       jobcontrol.c \
       sighandler.c \
       dynamicstring.c

OBJS = $(SRCS:.c=.o)

HEADERS = my_shell.h \
          parser.h \
          tokenizer.h \
          jobcontrol.h \
          sighandler.h \
          dynamicstring.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

re: clean all

.PHONY: all clean re