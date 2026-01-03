CC = gcc
CFLAGS = -Wall -g

TARGET = main

all: $(TARGET)

$(TARGET): main.o my_shell.o
	$(CC) -o $(TARGET) main.o my_shell.o

main.o: main.c my_shell.h
	$(CC) $(CFLAGS) -c main.c

my_shell.o: my_shell.c my_shell.h
	$(CC) $(CFLAGS) -c my_shell.c

clean:
	rm -f $(TARGET) *.o