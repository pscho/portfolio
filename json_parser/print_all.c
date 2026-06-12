
#include "json_parser.h"

const short LOG_LEVEL = 6;

int main(int argc, char *argv[]) {
	int mainRet;
	if ((mainRet = CheckAssumptions())) return EXIT_FAILURE;
	
	int ret = 0;
	
	if (argc != 2) {
		printf("json_parser options: [filePath]\n");
		return EXIT_FAILURE;
	}
	
	if (errno != 0) {
		return EXIT_FAILURE;
	}
	
	// goto cleanup beyond this point
		
	struct ParserInterface parserInterface;
	JsonParser_Init(&parserInterface, argv[1]);
	
	JsonParser_Debug_PrintAll(&parserInterface, &ret);
	assert(ret == 0 || ret == PARSE_END_OF_FILE);
	
	/*
	bool found = JsonParser_GoTo(&parserInterface, argv[2], &ret);
	assert(found);
	assert(ret == 0);
	
	ret = JsonParser_Debug_PrintCurrent(&parserInterface);
	assert(ret == 0);
	*/

	cleanup:
	JsonParser_Free(&parserInterface);
	
	return mainRet;
}


