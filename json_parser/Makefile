CC = gcc-12
objects = utils.o json_parser.o print_all.o goto.o print_all goto

ifeq ($(DEBUG), 1)
	CFLAGS = -ggdb -DDEBUG -Wall -Wextra -rdynamic
else
	CFLAGS = -DNDEBUG -Wall -Wextra
endif

all: $(objects)

goto: goto.o json_parser.o utils.o

goto.o: json_parser.h utils.h

print_all: print_all.o json_parser.o utils.o

print_all.o: json_parser.h

json_parser.o: json_parser.h utils.h

utils.o: utils.h

clean:
	rm $(objects)

