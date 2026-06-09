CC = gcc
CFLAGS = -Wall -Wextra
objects = utils.o sqlite.so sqlite_terminal.o

all: sqlite_terminal

sqlite_terminal: $(objects)
	$(CC) $(CFLAGS) -o sqlite_terminal $(objects) -pthread -ldl

sqlite_terminal.o: utils.h sqlite.so

sqlite.so: sqlite3.c sqlite3.h
	$(CC) -shared -c sqlite3.c -o sqlite.so 

utils.o: utils.h

clean:
	rm $(objects)
