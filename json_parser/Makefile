CC = gcc-12
objects = utils.o json_parser.o print_all.o

ifeq ($(DEBUG), 1)
	CFLAGS = -ggdb -DDEBUG -Wall -Wextra
else
	CFLAGS = -Wall -Wextra
endif

all: $(objects) print_all

print_all: print_all.o utils.o json_parser.o

print_all.o: json_parser.h

json_parser.o: json_parser.h utils.h

utils.o: utils.h

clean:
	rm $(objects)

