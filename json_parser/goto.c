#include "utils.h"
#include "json_parser.h"

const short LOG_LEVEL = 6;

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("Usage: goto [filePath] [jsonPath]\n");
		return 0;
	}

	struct ParserInterface interface;
	int ret = JsonParser_Init(&interface, argv[1]);
	CHECK(catch_main);

	bool gotoSuccessful = JsonParser_GoTo(&interface, argv[2], &ret);
	CHECK(catch_main);

	if (!gotoSuccessful) {
		UtilsError("goto - Unable to reach specified path\n");
		goto catch_main;
	} else {
		ret = JsonParser_Debug_PrintCurrent(&interface);
		CHECK(catch_main);
	}
	
	goto finally_main;	
catch_main:
	ret = EXIT_FAILURE;
finally_main:
	JsonParser_Free(&interface);

	return ret;
}
