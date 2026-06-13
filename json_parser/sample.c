/*
	Sample usage of the JSON parser.

	Execute as ./sample "json_parser_tests/sample.json"
*/

#include "json_parser.h"

const short LOG_LEVEL = 6;

int main (int argc, char** argv) {
	int mainRet = 0;

	assert(argc == 2);

	struct ParserInterface parserInterface;
	mainRet = JsonParser_Init(&parserInterface, argv[1]);
	assert(mainRet == 0);

	bool found = JsonParser_GoTo(&parserInterface, "pairs", &mainRet);
	assert(mainRet == 0);
	assert(found);

	enum ParserDataType dataType = JsonParser_GetCurrentType(&parserInterface, &mainRet);
	assert(mainRet == 0);
	assert(dataType == JP_ARR);

	mainRet = JsonParser_Expand(&parserInterface);
	assert(mainRet == 0);

	bool hasNext = JsonParser_HasNext(&parserInterface, &mainRet);
	assert(mainRet == 0);

	size_t numPairs = 0;
	while (hasNext) {
		dataType = JsonParser_GetNextType(&parserInterface, &mainRet);
		assert(dataType == JP_OBJ);

		mainRet = JsonParser_Expand(&parserInterface);
		assert(mainRet == 0);

		hasNext = JsonParser_HasNext(&parserInterface, &mainRet);
		assert(mainRet == 0);

		while (hasNext) {
			dataType = JsonParser_GetNextType(&parserInterface, &mainRet);
			assert(dataType == JP_LONG_DOUBLE);

			const char * key = JsonParser_GetKeyValue(&parserInterface, &mainRet);
			assert(mainRet == 0);

			long double val = JsonParser_GetLongDoubleValue(&parserInterface, &mainRet);
		        printf("%s: %Lf ", key, val);

			hasNext = JsonParser_HasNext(&parserInterface, &mainRet);
			assert(mainRet == 0);
		}
		printf("\n");
		++numPairs;

		mainRet = JsonParser_Collapse(&parserInterface);
		assert(mainRet == 0);

		hasNext = JsonParser_HasNext(&parserInterface, &mainRet);
		assert(mainRet == 0);
	}	
	mainRet = JsonParser_Collapse(&parserInterface);
	assert(mainRet == 0);

	printf("# of pairs: %lu\n", numPairs);
	
	JsonParser_Free(&parserInterface);

	return mainRet;
}
