CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -L. -lmymalloc

all: my_shell

my_shell: my_shell.o
	$(CC) -o $@ $^ $(LDFLAGS)

my_shell.o: mys_hell.c MemoryAllocator.h
	$(CC) $(CFLAGS) -c myshell.c

clean:
	rm -f my_shell *.o