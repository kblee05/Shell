CC = gcc
CFLAGS = -Wall -g

TARGET = main

all: $(TARGET)

$(TARGET): main.o my_shell.o parser.o tokenizer.o dynamicstring.o
	$(CC) -o $(TARGET) main.o my_shell.o parser.o tokenizer.o dynamicstring.o

main.o: main.c my_shell.h parser.h tokenizer.h dynamicstring.h
	$(CC) $(CFLAGS) -c main.c

my_shell.o: my_shell.c my_shell.h parser.h tokenizer.h dynamicstring.h
	$(CC) $(CFLAGS) -c my_shell.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

tokenizer.o: tokenizer.c tokenizer.h
	$(CC) $(CFLAGS) -c tokenizer.c

dynamicstring.o: dynamicstring.c dynamicstring.h
	$(CC) $(CFLAGS) -c dynamicstring.c

clean:
	rm -f $(TARGET) *.o