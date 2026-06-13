WORK IN PROGRESS

A basic, state-machine based, "immediate access" JSON parser. Written for the JSON parser portion of the "Performance-Aware Programming" course by Casey Muratori: https://www.computerenhance.com/p/table-of-contents

This was mostly written for the class and to experiment with a different type of parser to see if it could be made more performant; not really intended to be used in production.

"Immediate access" means that it does not actually parse the JSON in its entirety to produce a dictionary-like object like how many other parsers do; there is no parse() function. Instead, the intended use is to navigate to a particular location in a JSON file using the `JsonParser_GoTo` function, and then retrieving just the needed value(s) at or after that location.

Build instructions for repo:
1. Run `make`

To use as a library:
1. Ensure the following required files exist: `utils.h utils.c json_parser.h json_parser.c`
2. Run `gcc -o utils.o -c utils.c`
3. Run `gcc -o json_parser.o -c json_parser.c`
4. Use the API functions in `json_parser.h`

Usage:

1. Please see "Typical_Usage.jpg" for a state transition diagram showing typical usage.
2. `json_parser.h` contains documentation for each API function.
3. Please see `sample.c`, `goto.c`, and `print_all.c` for usage examples. After running make:

	a. sample: `./sample "json_parser_tests/sample.json"`
	b. goto: `./goto json_parser_tests/random1.json [\"pairs\"][9][\"y..1\"][3][2][\"c..\"][2]`
	c. print_all: `./print_all "json_parser_tests/random1.json"`


