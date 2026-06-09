CC = gcc-12
CFLAGS = -ggdb -Wall -Wextra
objects = utils.o json_parser.o

json_parser.o: json_parser.h utils.h

utils.o: utils.h

clean:
	rm $(objects)

