// State machine based JSON parser
// See journal 1/27/26 for notes

// - Maximum supported precision for floats is long double precision (16 bytes)
// - Maximum supported JSON path length is (INT_MAX - 1) chars

// TODO: Error code parameter for functions (multiple return values pattern)

// 5/22/26: Commented out usages of addrStack, memberCountStack, FinishParsingObjectOrArray
// 5/22/26: TODO Clear byte array with each usage
// TODO: 5/22/26 Better memory chunk size allocation for byteArr
// TODO: 5/22/26 Fractional values for very large (long double) values printing 0 
// TODO: 6/12/26 Input validation for JsonParser_GoTo

#include "json_parser.h"

const size_t PARSER_ARRAY_BLOCK_SIZE = 512;

const size_t PARSER_KEY_ID_ARRAY_BLOCK_SIZE = 64;

/*
// Assumption #3; modeStack has elems of size 8, holding addresses and these const values of size 8
const signed long PARSER_MODE_VAR = -1, PARSER_MODE_ARR = -2, PARSER_MODE_OBJ = -3;
*/

const unsigned char PARSER_OBJ = 1, PARSER_ARR = 2, PARSER_PRIMITIVE = 3;

const char UNICODE_C0_CONTROL_CHARS[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, \
	24, 25, 26, 27, 28, 29, 30, 31};


enum ParserTransition {
	TO_SAME_STATE_DEFAULT, // There may be multiple transitions to the same state w/ different side effects
	PARSE_INIT_TO_PARSE_OBJECT_OPEN_CURLY_BRACE,
	PARSE_INIT_TO_PARSE_ARRAY_OPEN_BRACKET,
	PARSE_OBJECT_OPEN_CURLY_BRACE_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_OBJECT_OPEN_CURLY_BRACE,
	ARRAY_OPEN_BRACKET_TO_ARRAY_OPEN_BRACKET_AGAIN,
	ARRAY_OPEN_BRACKET_AGAIN_TO_ARRAY_OPEN_BRACKET,
	PARSE_KEY_ID_ESCAPE_CHAR_TO_PARSE_KEY_ID_UNICODE_0,
	PARSE_KEY_ID_TO_PARSE_KEY_ID_FINISH,
	BEGIN_VALUE_PARSE_TO_PARSE_OBJECT_OPEN_CURLY_BRACE,
	BEGIN_VALUE_PARSE_TO_PARSE_ARRAY_OPEN_BRACKET,
	BEGIN_VALUE_PARSE_TO_PARSE_STRING,
	BEGIN_VALUE_PARSE_TO_PARSE_BOOL_T,
	BEGIN_VALUE_PARSE_TO_PARSE_BOOL_F,
	BEGIN_VALUE_PARSE_TO_PARSE_NULL_N,
	PARSE_NUMBER_INTEGRAL_TO_PARSE_VALUE_FINISH,
	PARSE_NUMBER_INTEGRAL_TO_PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_COMMA,
	PARSE_NUMBER_INTEGRAL_TO_BEGIN_VALUE_PARSE,
	PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_VALUE_FINISH,
	PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_COMMA,
	PARSE_NUMBER_INTEGRAL_ZERO_TO_BEGIN_VALUE_PARSE,
	PARSE_NUMBER_FRACTIONAL_TO_PARSE_VALUE_FINISH,
	PARSE_NUMBER_FRACTIONAL_TO_PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_COMMA,
	PARSE_NUMBER_FRACTIONAL_TO_BEGIN_VALUE_PARSE,
	PARSE_STRING_TO_PARSE_VALUE_FINISH,
	PARSE_BOOL_TRU_TO_PARSE_VALUE_FINISH,
	PARSE_BOOL_FALS_TO_PARSE_VALUE_FINISH,
	PARSE_NULL_NUL_TO_PARSE_VALUE_FINISH,
	OBJECT_CLOSE_CURLY_BRACE_TO_OBJECT_CLOSE_CURLY_BRACE_AGAIN,
	OBJECT_CLOSE_CURLY_BRACE_AGAIN_TO_OBJECT_CLOSE_CURLY_BRACE,
	ARRAY_CLOSE_BRACKET_TO_ARRAY_CLOSE_BRACKET_AGAIN,
	ARRAY_CLOSE_BRACKET_AGAIN_TO_ARRAY_CLOSE_BRACKET,
	PARSE_OBJECT_CLOSE_CURLY_BRACE_TO_PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_ARRAY_CLOSE_BRACKET_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	TO_SAME_STATE_PARSE_OBJECT_CLOSE_CURLY_BRACE_CLOSE_ANOTHER_OBJECT,
	PARSE_VALUE_FINISH_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE,
	PARSE_VALUE_FINISH_TO_PARSE_ARRAY_CLOSE_BRACKET
};


bool CharInString(const char *s, unsigned char c) {
	for (size_t i = 0; i < strlen(s); i++) {
		if (s[i] == c) {
			return true;
		}
	}
	
	return false;
}


int ValidateAndAdvanceStrIdx(const char * const validationStr, const char * const s, size_t * const idx) {
	int ret = 0;

	for (size_t i = 0; i < strlen(validationStr); ++i) {
		if (s[*idx] != validationStr[i]) {
			UtilsError("ValidateAndAdvanceStrIdx - char mismatch; %s %s %lu %lu\n", validationStr, s, *idx, i);
			ret = EXIT_FAILURE;
			break;
		} else {
			*idx += 1;
		}
	}

	return ret; 
}


int CheckArraySize(void **array, size_t idx, size_t size, size_t *nmembPtr, size_t blockSize) {
	int ret = 0;
	
	if (idx == *nmembPtr - 1) {
		*array = realloc(*array, (size * *nmembPtr) + blockSize);
		if (CheckNotNull(*array, "json_parser CheckArraySize realloc")) {
			ret = EXIT_FAILURE;
		} else {
			//printf("CheckArraySize realloc\n");
			*nmembPtr = *nmembPtr + (blockSize / size);
		}
	}
	
	return ret;
}


int VerifyState_ParseKeyId(struct Parser *parser) {
	int ret = 0;
	
	if (parser->state != PARSE_KEY_ID) {
		ret = EXIT_FAILURE;
		printf("Invalid value for state: %d\n", parser->state);
	}
	
	return ret;
}


/* Parsing functions start */


int UpdateByteArray(struct Parser *parser, unsigned char byteVal) {
	parser->byteArr[parser->byteArrIdx] = byteVal;
	++parser->byteArrIdx;
	if (CheckArraySize((void **) &parser->byteArr, parser->byteArrIdx, sizeof parser->byteArr, \
		&parser->byteArrMaxItems, PARSER_ARRAY_BLOCK_SIZE)) {
		return EXIT_FAILURE;
	}

	/*
	for (size_t i = 0; i < parser->byteArrMaxItems; ++i) {
		printf("%d ", parser->byteArr[i]);
	}
	printf("\n");*/

	return 0;
}


void ResetByteArray(struct Parser *parser) {
	parser->byteArr[0] = '\0';
	parser->byteArrIdx = 0;

	return;
}


int UpdateAddrArray(struct Parser *parser, void *addr) {
	parser->addrArr[parser->addrArrIdx] = addr;
	++parser->addrArrIdx;
	if (CheckArraySize((void **) &parser->addrArr, parser->addrArrIdx, sizeof parser->addrArr, \
		&parser->addrArrMaxItems, PARSER_ARRAY_BLOCK_SIZE)) {
		return EXIT_FAILURE;
	}
	
	return 0;
}


int InitObjectMetadata(struct Parser *parser) {
	size_t initMemberCount = 0;
	//if (ArrayStack_Push(&parser->memberCountStack, &initMemberCount)) return EXIT_FAILURE;
	//if (ArrayStack_Push(&parser->modeStack, &PARSER_OBJ)) return EXIT_FAILURE;
	//parser->isParsingArray = false;
	
	return 0;
}


int InitArrayMetadata(struct Parser *parser) {
	size_t initMemberCount = 0;
	//if (ArrayStack_Push(&parser->memberCountStack, &initMemberCount)) return EXIT_FAILURE;
	//if (ArrayStack_Push(&parser->modeStack, &PARSER_ARR)) return EXIT_FAILURE;
	//parser->isParsingArray = true;
	
	return 0;
}


int ParseKeyIdToParseKeyIdFinish(struct Parser *parser) {
	if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
	
	// Loop until idStartPtr points to the first character of the key ID string in byteArr
	unsigned char *idStartPtr = parser->byteArr + (parser->byteArrIdx - 2);
	do {
		--idStartPtr;
	} while (*idStartPtr != '\0');
	++idStartPtr;
	
	//if (ArrayStack_Push(&parser->modeStack, &idStartPtr)) return EXIT_FAILURE;
	//if (ArrayStack_Push(&parser->modeStack, &PARSER_MODE_VAR)) return EXIT_FAILURE;

	return 0;
}


int ParseNumber(struct Parser *parser) {
	assert(parser->state == PARSE_NUMBER_INTEGRAL ||
		parser->state == PARSE_NUMBER_INTEGRAL_ZERO ||
		parser->state == PARSE_NUMBER_FRACTIONAL);
	// Temporary add \0 so that strtold parsing will end at that position.
	// The index where this temporary \0 is added will be replaced by a value of
	// enum CDataType indicating the data type of the value, followed by a
	// permanent \0 indicating end of the key:value field.
	if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
	
	// Revert byteArrIdx to the first character of the value in byteArr both to
	// parse the value and as the existing value will be replaced with the parsed numeric value below
	parser->byteArrIdx = parser->byteArrIdx - 2;
	do {
		--parser->byteArrIdx;
	} while (parser->byteArr[parser->byteArrIdx] != '\0');
	++parser->byteArrIdx;
	
	// Parse number from string value
	if (CheckNotZero(errno, "json_parser ParseNumber errno check")) return EXIT_FAILURE;
	char *temp = NULL;
	unsigned char *convertedParsedValPtr = NULL;
	unsigned char numBytes;
	enum ParserDataType parsedValueType;
	if (parser->state == PARSE_NUMBER_INTEGRAL || parser->state == PARSE_NUMBER_INTEGRAL_ZERO) {
		long long int initialParsedVal = strtoll((char*) parser->byteArr + parser->byteArrIdx, &temp, 10);
		if (errno || *temp != '\0') {
			UtilsError("json_parser ParseNumber strtold");
			return EXIT_FAILURE;
		}	
		convertedParsedValPtr = (unsigned char*) &initialParsedVal;
		numBytes = sizeof(long long int);
		parsedValueType = JP_S_LONG;
	} else {
		long double initialParsedVal = strtold((char *) parser->byteArr + parser->byteArrIdx, &temp);
		if (errno || *temp != '\0') {
			UtilsError("json_parser ParseNumber strtold");
			return EXIT_FAILURE;
		}
		convertedParsedValPtr = (unsigned char*) &initialParsedVal;
		numBytes = sizeof(long double);
		parsedValueType = JP_LONG_DOUBLE;

		/*
		// Check value and decrease allocated memory size from long double to a smaller data type if possible
		long double fractional, integral;
		fractional = modfl(initialParsedVal, &integral); // No infinity or NaN numeric values supported!
		enum ParserDataType parsedValueType = JP_LONG_DOUBLE;
		if (fractional == 0.0) { // Floating point so very small fractional values can still equal zero!
			if (integral <= CHAR_MAX && integral >= CHAR_MIN) {
				parsedValueType = JP_S_CHAR;
			} else if (integral <= INT_MAX && integral >= INT_MIN) {
				parsedValueType = JP_S_INT;
			} else if (integral <= LONG_MAX && integral >= LONG_MIN) {
				parsedValueType = JP_S_LONG;
			}
		} else if (initialParsedVal <= FLT_MAX && initialParsedVal >= -FLT_MAX) {
			parsedValueType = JP_FLOAT;
		}
		
		// Replace string value with numeric value
		char numBytes;
		switch (parsedValueType) {
			case JP_S_CHAR:
				numBytes = 1;
				char charParsedVal = (char) initialParsedVal;
				convertedParsedValPtr = (unsigned char*) &charParsedVal;
				break;
			case JP_S_INT:
				numBytes = 4;
				int intParsedVal = (int) initialParsedVal;
				convertedParsedValPtr = (unsigned char*) &intParsedVal;
				break;
			case JP_S_LONG:
				numBytes = 8;
				long longParsedVal = (long) initialParsedVal;
				convertedParsedValPtr = (unsigned char*) &longParsedVal;
				break;
			case JP_FLOAT:
				numBytes = 4;
				float floatParsedVal = (float) initialParsedVal;
				convertedParsedValPtr = (unsigned char*) &floatParsedVal;
				break;
			case JP_LONG_DOUBLE:
				numBytes = 16;
				convertedParsedValPtr = (unsigned char*) &initialParsedVal;
				break;
			default:
				UtilsError("json_parser ParseNumber switch parsed value");
				return EXIT_FAILURE;
		}*/
	}	
		
	for (int i = 0; i < numBytes; ++i) {
		if (UpdateByteArray(parser, *convertedParsedValPtr)) return EXIT_FAILURE;
		++convertedParsedValPtr;
	}
	
	unsigned char *valTypePtr = parser->byteArr + parser->byteArrIdx;
	//if (ArrayStack_Push(&parser->addrStack, &valTypePtr)) return EXIT_FAILURE;
	if (UpdateByteArray(parser, (unsigned char) parsedValueType)) return EXIT_FAILURE;
	if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
	
	/*size_t memberCount = 0;
	if (ArrayStack_Pop(&parser->memberCountStack, &memberCount)) return EXIT_FAILURE;	
	++memberCount;
	if (ArrayStack_Push(&parser->memberCountStack, &memberCount)) return EXIT_FAILURE;*/
	//if (ArrayStack_Push(&parser->modeStack, &valTypePtr)) return EXIT_FAILURE;

	return 0;
}


int ParseBoolToParseValueFinish(struct Parser *parser, bool boolVal) {
	if (UpdateByteArray(parser, boolVal)) return EXIT_FAILURE;
	unsigned char *valTypePtr = parser->byteArr + parser->byteArrIdx;
	// Assumption #4: Bool length is 1 byte
	if (UpdateByteArray(parser, JP_BOOL)) return EXIT_FAILURE;
	if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
	
	/*size_t memberCount = 0;
	if (ArrayStack_Pop(&parser->memberCountStack, &memberCount)) return EXIT_FAILURE;
	++memberCount;
	if (ArrayStack_Push(&parser->memberCountStack, &memberCount)) return EXIT_FAILURE;*/
	
	//if (ArrayStack_Push(&parser->modeStack, &valTypePtr)) return EXIT_FAILURE;
	//if (ArrayStack_Push(&parser->addrStack, &valTypePtr)) return EXIT_FAILURE;
	//parser->state = PARSE_VALUE_FINISH;
	
	return 0;
}


int RestoreIsParsingArray(struct Parser *parser) {
	unsigned char mode = 0;
	// Pop current mode from modeStack
	if (ArrayStack_Pop(&parser->modeStack, &mode)) return EXIT_FAILURE;
	
	// If modeStack is not empty set isParsingOnly back to previous mode
	// Else parsing is FINISHED
	if (!ArrayStack_IsEmpty(&parser->modeStack)) {
		if (ArrayStack_Peek(&parser->modeStack, &mode)) return EXIT_FAILURE;
		parser->isParsingArray = mode == PARSER_ARR;
	}
	
	return 0;
}


int FinishParsingObjectOrArray(struct Parser *parser) {
	/*// Pop object element addresses from addrStack and add to addrArr
	void *valTypePtr = NULL;
	for (int i = 0; i < memberCount; ++i) {
		if (ArrayStack_Pop(&parser.addrStack, &valTypePtr)) return EXIT_FAILURE;
		if (UpdateAddrArray(parser, valTypePtr)) return EXIT_FAILURE;
	}*/
	
	// Add object/array metadata to byteArr:
	// 1. Get member count
	size_t memberCount = 0;
	if (ArrayStack_Pop(&parser->memberCountStack, &memberCount)) return EXIT_FAILURE;
	
	// 2. Pop object/array element addresses from addrStack add to byteArr
	void *valTypePtr = NULL;
	unsigned char *tempPtr = (unsigned char*) &valTypePtr;
	for (size_t i = 0; i < memberCount; ++i) {
		if (ArrayStack_Pop(&parser->addrStack, &valTypePtr)) return EXIT_FAILURE;
		for (size_t j = 0; j < sizeof valTypePtr; ++j) {
			if (UpdateByteArray(parser, *tempPtr)) return EXIT_FAILURE;
			++tempPtr;
		}
		tempPtr = (unsigned char*) &valTypePtr;
	}
	
	// 3. To byteArr, add the memberCount
	tempPtr = (unsigned char*) &memberCount;
	for (size_t i = 0; i < sizeof memberCount; ++i) {
		if (UpdateByteArray(parser, *tempPtr)) return EXIT_FAILURE;
		++tempPtr;
	}
	
	// 4. Add object/array enum to byteArr as the type pointer and add the type pointer's address
	// as the address of the object into addrStack
	valTypePtr = &parser->byteArr + parser->byteArrIdx;
	
	enum ParserDataType objArrEnum;
	if (parser->isParsingArray) {
		objArrEnum = JP_ARR;
	} else {
		objArrEnum = JP_OBJ;
	}
	if (UpdateByteArray(parser, objArrEnum)) return EXIT_FAILURE;
	if (ArrayStack_Push(&parser->addrStack, &valTypePtr)) return EXIT_FAILURE;
	
	// 5. Add separator
	if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;

	/*// Pop current mode from modeStack
	if (ArrayStack_Pop(&parser->modeStack, &tempPtr)) return EXIT_FAILURE;
	
	// If modeStack is not empty set isParsingOnly back to previous mode
	// Else parsing is FINISHED
	if (!ArrayStack_IsEmpty(&parser->modeStack)) {
		if (ArrayStack_Peek(&parser->modeStack, &tempPtr)) return EXIT_FAILURE;
		parser->isParsingArray = *tempPtr == PARSER_ARR;
	}*/

	return 0;
}

/*

Return values:
	- 0: OK
	- 1: Error
	- PARSE_END_OF_FILE: EOF	

*/
int _IterateParser(struct Parser *parser) {
	int ret = 0;	
	
	if (parser->state == PARSE_END_OF_FILE) return 0;
	
	unsigned char nextChar;
	ret = fread(&nextChar, 1, 1, parser->jsonFile);
	if (ret != 1) {
		if (ferror(parser->jsonFile)) {
			UtilsError("json_parser _IterateParser ferror");
			return EXIT_FAILURE;
		} else {
			assert(feof(parser->jsonFile));
			parser->state = PARSE_END_OF_FILE;
			return PARSE_END_OF_FILE;
		}
	}
	
#ifdef DEBUG
	printf("char: %c lineNum: %ld colNum: %ld\n", nextChar, parser->lineNum, parser->colNum);
#endif

	if (nextChar == 10) { // Line feed
		++parser->lineNum;
		parser->colNum = 0;
	} else {
		++parser->colNum;
	}
	
	char const *charFilter;
	bool inclusiveFilter = true;
	
	switch (parser->state) {
		case PARSE_INIT:
			charFilter = " \t\r\n{[";
			break;
		case PARSE_OBJECT_OPEN_CURLY_BRACE:
			charFilter = " \t\r\n\"}";
			break;
		case PARSE_ARRAY_OPEN_BRACKET:
		case PARSE_ARRAY_OPEN_BRACKET_AGAIN:
			charFilter = " \t\r\n0123456789-\"tf[{n]";
			break;
		case PARSE_OBJECT_COMMA:
			charFilter = " \t\r\n\"";
			break;
		case PARSE_KEY_ID:
		case PARSE_STRING:
			charFilter = UNICODE_C0_CONTROL_CHARS;
			inclusiveFilter = false;
			break;
		case PARSE_KEY_ID_ESCAPE_CHAR:
		case PARSE_STRING_ESCAPE_CHAR:
			charFilter = "\"\\nrtbfu";
			break;
		case PARSE_KEY_ID_UNICODE_0:
		case PARSE_KEY_ID_UNICODE_1:
		case PARSE_KEY_ID_UNICODE_2:
		case PARSE_KEY_ID_UNICODE_3:
		case PARSE_STRING_UNICODE_0:
		case PARSE_STRING_UNICODE_1:
		case PARSE_STRING_UNICODE_2:
		case PARSE_STRING_UNICODE_3:
			charFilter = "0123456789abcdefABCDEF";
			break;
		case PARSE_KEY_ID_FINISH:
			charFilter = " \t\r\n:";
			break;
		case BEGIN_VALUE_PARSE:
			charFilter = " \t\r\n0123456789-\"tf[{n";
			break;
		case PARSE_NUMBER_INTEGRAL:
			charFilter = " \t\r\n0123456789.,}]";
			break;
		case PARSE_NUMBER_INTEGRAL_ZERO:
			charFilter = " \t\r\n.,}]";
			break;
		case PARSE_NUMBER_NEG:
		case PARSE_NUMBER_FRACTIONAL_TENTHS_PLACE:
			charFilter = "0123456789";
			break;
		case PARSE_NUMBER_FRACTIONAL:
			charFilter = " \t\r\n0123456789,}]";
			break;
		case PARSE_BOOL_T:
			charFilter = "r";
			break;
		case PARSE_BOOL_TR:
			charFilter = "u";
			break;
		case PARSE_BOOL_TRU:
			charFilter = "e";
			break;
		case PARSE_BOOL_F:
			charFilter = "a";
			break;
		case PARSE_BOOL_FA:
			charFilter = "l";
			break;
		case PARSE_BOOL_FAL:
			charFilter = "s";
			break;
		case PARSE_BOOL_FALS:
			charFilter = "e";
			break;
		case PARSE_OBJECT_CLOSE_CURLY_BRACE:
		case PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN:
		case PARSE_ARRAY_CLOSE_BRACKET:
		case PARSE_ARRAY_CLOSE_BRACKET_AGAIN:
			charFilter = " \t\r\n,}]";
			break;
		case PARSE_NULL_N:
			charFilter = "u";
			break;
		case PARSE_NULL_NU:
		case PARSE_NULL_NUL:
			charFilter = "l";
			break;
		case PARSE_VALUE_FINISH:
			charFilter = " \t\r\n,}]";
			break;
		default:
			UtilsError("json_parser _IterateParser switch charFilter");
			return EXIT_FAILURE;
			break;
	}
	
	if (charFilter) {
		bool charInString = CharInString(charFilter, nextChar);
		if ((inclusiveFilter && !charInString) || (!inclusiveFilter && charInString)) {
			printf("Char \"%c\" at line %ld, column %ld is not an expected character\n", nextChar, \
				 parser->lineNum, parser->colNum);
			parser->state = PARSE_ERROR_UNEXPECTED_CHAR;
			return PARSE_ERROR_UNEXPECTED_CHAR;
		}
	}
	
	enum ParserState updateStateOnly = PARSE_NULL_STATE;
	enum ParserState updateStateAndByteArray = PARSE_NULL_STATE;
	enum ParserTransition transition = TO_SAME_STATE_DEFAULT;
	
	/*
		Variables used to determine state transition (keep small):
			parser->state
			nextChar
			parser->isParsingArray

	*/
	switch(parser->state) {
		case PARSE_INIT:
			if (nextChar == '{') {
				transition = PARSE_INIT_TO_PARSE_OBJECT_OPEN_CURLY_BRACE;
			} else if (nextChar == '[') {
				transition = PARSE_INIT_TO_PARSE_ARRAY_OPEN_BRACKET;
			}
			break;
		case PARSE_OBJECT_OPEN_CURLY_BRACE:
			if (nextChar == '"') {
				updateStateOnly = PARSE_KEY_ID;
			} else if (nextChar == '}') {
				transition = PARSE_OBJECT_OPEN_CURLY_BRACE_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
			}
			break;
		case PARSE_ARRAY_OPEN_BRACKET:
		case PARSE_ARRAY_OPEN_BRACKET_AGAIN:
			if (CharInString(" \t\r\n", nextChar)) {
				updateStateOnly = BEGIN_VALUE_PARSE;
			} else if (nextChar == ']') {
				transition = PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_ARRAY_CLOSE_BRACKET;
			} else if (nextChar == '0') {
				updateStateAndByteArray = PARSE_NUMBER_INTEGRAL_ZERO;
			} else if (CharInString("123456789", nextChar)) {
				updateStateAndByteArray = PARSE_NUMBER_INTEGRAL;
			} else if (nextChar == '-') {
				updateStateAndByteArray = PARSE_NUMBER_NEG;
			} else if (nextChar == '"') {
				updateStateOnly = PARSE_STRING;
			} else if (nextChar == 't') {
				updateStateOnly = PARSE_BOOL_T;
			} else if (nextChar == 'f') {
				updateStateOnly = PARSE_BOOL_F;
			} else if (nextChar == '{') {
				transition = PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_OBJECT_OPEN_CURLY_BRACE;
			} else if (nextChar == 'n') {
				updateStateOnly = PARSE_NULL_N;
			} else {
				if (parser->state == PARSE_ARRAY_OPEN_BRACKET) {
					transition = ARRAY_OPEN_BRACKET_TO_ARRAY_OPEN_BRACKET_AGAIN;
				} else {
					transition = ARRAY_OPEN_BRACKET_AGAIN_TO_ARRAY_OPEN_BRACKET;
				}
			}
			break;
		case PARSE_OBJECT_COMMA:
			if (nextChar == '\"') {
				updateStateOnly = PARSE_KEY_ID;
			}
			break;
		case PARSE_KEY_ID:
			if (nextChar == '\\') {
				updateStateAndByteArray = PARSE_KEY_ID_ESCAPE_CHAR;
			} else if (nextChar == '"') {
				transition = PARSE_KEY_ID_TO_PARSE_KEY_ID_FINISH;
			}
			break;
		case PARSE_KEY_ID_ESCAPE_CHAR:
			if (nextChar == 'u') {
				updateStateAndByteArray = PARSE_KEY_ID_UNICODE_0;
			} else {
				updateStateAndByteArray = PARSE_KEY_ID;
			}
			break;
		case PARSE_KEY_ID_UNICODE_0:
			updateStateAndByteArray = PARSE_KEY_ID_UNICODE_1;
			break;
		case PARSE_KEY_ID_UNICODE_1:
			updateStateAndByteArray = PARSE_KEY_ID_UNICODE_2;
			break;
		case PARSE_KEY_ID_UNICODE_2:
			updateStateAndByteArray = PARSE_KEY_ID_UNICODE_3;
			break;
		case PARSE_KEY_ID_UNICODE_3:
			updateStateAndByteArray = PARSE_KEY_ID;
			break;
		case PARSE_KEY_ID_FINISH:
			if (nextChar == ':') {
				updateStateOnly = BEGIN_VALUE_PARSE;
			}
			break;
		case PARSE_STRING:
			if (nextChar == '\\') {
				updateStateAndByteArray = PARSE_STRING_ESCAPE_CHAR;
			} else if (nextChar == '"') {
				transition = PARSE_STRING_TO_PARSE_VALUE_FINISH;
			}
			break;
		case PARSE_STRING_ESCAPE_CHAR:
			if (nextChar == 'u') {
				updateStateAndByteArray = PARSE_STRING_UNICODE_0;
			} else {
				updateStateAndByteArray = PARSE_STRING;
			}
			break;
		case PARSE_STRING_UNICODE_0:
			updateStateAndByteArray = PARSE_STRING_UNICODE_1;
			break;
		case PARSE_STRING_UNICODE_1:
			updateStateAndByteArray = PARSE_STRING_UNICODE_2;
			break;
		case PARSE_STRING_UNICODE_2:
			updateStateAndByteArray = PARSE_STRING_UNICODE_3;
			break;
		case PARSE_STRING_UNICODE_3:
			updateStateAndByteArray = PARSE_STRING;
			break;
		case BEGIN_VALUE_PARSE:
			if (!CharInString(" \t\r\n", nextChar)) {
				if (nextChar == '0') {
					updateStateAndByteArray = PARSE_NUMBER_INTEGRAL_ZERO;
				} else if (CharInString("123456789", nextChar)) {
					updateStateAndByteArray = PARSE_NUMBER_INTEGRAL;
				} else if (nextChar == '-') {
					updateStateAndByteArray = PARSE_NUMBER_NEG;
				} else if (nextChar == '"') {
					updateStateOnly = PARSE_STRING;
				} else if (nextChar == 't') {
					updateStateOnly = PARSE_BOOL_T;
				} else if (nextChar == 'f') {
					updateStateOnly = PARSE_BOOL_F;
				} else if (nextChar == '[') {
					transition = BEGIN_VALUE_PARSE_TO_PARSE_ARRAY_OPEN_BRACKET;
				} else if (nextChar == '{') {
					transition = BEGIN_VALUE_PARSE_TO_PARSE_OBJECT_OPEN_CURLY_BRACE;
				} else {
					updateStateOnly = PARSE_NULL_N;
				}
			}
			break;
		case PARSE_NUMBER_INTEGRAL:
			if (CharInString(" \t\r\n", nextChar)) {
				transition = PARSE_NUMBER_INTEGRAL_TO_PARSE_VALUE_FINISH;
			} else if (nextChar == '.') {
				updateStateAndByteArray = PARSE_NUMBER_FRACTIONAL_TENTHS_PLACE;
			} else if (nextChar == ',') {
				if (parser->isParsingArray) {
					transition = PARSE_NUMBER_INTEGRAL_TO_BEGIN_VALUE_PARSE;
				} else {
					transition = PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_COMMA;
				}
			} else if (nextChar == ']') {
				transition = PARSE_NUMBER_INTEGRAL_TO_PARSE_ARRAY_CLOSE_BRACKET;
			} else if (nextChar == '}') {
				transition = PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
			}
			break;
		case PARSE_NUMBER_INTEGRAL_ZERO:
			if (nextChar == '.') {
				updateStateAndByteArray = PARSE_NUMBER_FRACTIONAL_TENTHS_PLACE;
			} else if (nextChar == ',') {
				if (parser->isParsingArray) {
					transition = PARSE_NUMBER_INTEGRAL_ZERO_TO_BEGIN_VALUE_PARSE;
				} else {
					transition = PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_COMMA;
				}
			} else if (nextChar == ']') {
				transition = PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_ARRAY_CLOSE_BRACKET;
			} else if (nextChar == '}') {
				transition = PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
			}
			break;
		case PARSE_NUMBER_NEG:
			if (nextChar == 0) {
				updateStateAndByteArray = PARSE_NUMBER_INTEGRAL_ZERO;
			} else {
				updateStateAndByteArray = PARSE_NUMBER_INTEGRAL;
			}
			break;
		case PARSE_NUMBER_FRACTIONAL_TENTHS_PLACE:
			updateStateAndByteArray = PARSE_NUMBER_FRACTIONAL;
			break;
		case PARSE_NUMBER_FRACTIONAL:
			if (CharInString(" \t\r\n", nextChar)) {
				transition = PARSE_NUMBER_FRACTIONAL_TO_PARSE_VALUE_FINISH;
			} else if (nextChar == ',') {
				if (parser->isParsingArray) {
					transition = PARSE_NUMBER_FRACTIONAL_TO_BEGIN_VALUE_PARSE;
				} else {
					transition = PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_COMMA;
				}
			} else if (nextChar == ']') {
				transition = PARSE_NUMBER_FRACTIONAL_TO_PARSE_ARRAY_CLOSE_BRACKET;
			} else if (nextChar == '}') {
				transition = PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
			}
			break;
		case PARSE_BOOL_T:
			updateStateOnly = PARSE_BOOL_TR;
			break;
		case PARSE_BOOL_TR:
			updateStateOnly = PARSE_BOOL_TRU;
			break;
		case PARSE_BOOL_TRU:
			transition = PARSE_BOOL_TRU_TO_PARSE_VALUE_FINISH;
			break;
		case PARSE_BOOL_F:
			updateStateOnly = PARSE_BOOL_FA;
			break;
		case PARSE_BOOL_FA:
			updateStateOnly = PARSE_BOOL_FAL;
			break;
		case PARSE_BOOL_FAL:
			updateStateOnly = PARSE_BOOL_FALS;
			break;
		case PARSE_BOOL_FALS:
			transition = PARSE_BOOL_FALS_TO_PARSE_VALUE_FINISH;
			break;
		case PARSE_NULL_N:
			updateStateOnly = PARSE_NULL_NU;
			break;
		case PARSE_NULL_NU:
			updateStateOnly = PARSE_NULL_NUL;
			break;
		case PARSE_NULL_NUL:
			transition = PARSE_NULL_NUL_TO_PARSE_VALUE_FINISH;
			break;
		case PARSE_OBJECT_CLOSE_CURLY_BRACE:
		case PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN:
			if (!CharInString(" \t\r\n", nextChar)) {
				if (nextChar == ',') {
					if (parser->isParsingArray) {
						updateStateOnly = BEGIN_VALUE_PARSE;
					} else {
						updateStateOnly = PARSE_OBJECT_COMMA;
					}
				} else if (nextChar == ']') {
					transition = PARSE_OBJECT_CLOSE_CURLY_BRACE_TO_PARSE_ARRAY_CLOSE_BRACKET;
				} else {
					if (parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE) {
						transition = OBJECT_CLOSE_CURLY_BRACE_TO_OBJECT_CLOSE_CURLY_BRACE_AGAIN;
					} else {
						transition = OBJECT_CLOSE_CURLY_BRACE_AGAIN_TO_OBJECT_CLOSE_CURLY_BRACE;
					}
				}
			}
			break;
		case PARSE_ARRAY_CLOSE_BRACKET:
		case PARSE_ARRAY_CLOSE_BRACKET_AGAIN:
			if (!CharInString(" \t\r\n", nextChar)) {
				if (nextChar == ',') {
					if (parser->isParsingArray) {
						updateStateOnly = BEGIN_VALUE_PARSE;
					} else {
						updateStateOnly = PARSE_OBJECT_COMMA;
					}
				} else if (nextChar == '}') {
					transition = PARSE_ARRAY_CLOSE_BRACKET_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
				} else {
					if (parser->state == PARSE_ARRAY_CLOSE_BRACKET) {
						transition = ARRAY_CLOSE_BRACKET_TO_ARRAY_CLOSE_BRACKET_AGAIN;
					} else {
						transition = ARRAY_CLOSE_BRACKET_AGAIN_TO_ARRAY_CLOSE_BRACKET;
					}
				}
			}
			break;
		case PARSE_VALUE_FINISH:
			if (nextChar == ',') {
				if (parser->isParsingArray) {
					updateStateOnly = BEGIN_VALUE_PARSE;
				} else {
					updateStateOnly = PARSE_OBJECT_COMMA;
				}
			} else if (nextChar == '}') {
				transition = PARSE_VALUE_FINISH_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE;
			} else if (nextChar == ']') {
				transition = PARSE_VALUE_FINISH_TO_PARSE_ARRAY_CLOSE_BRACKET;
			}
			break;
		default:
			UtilsError("json_parser _IterateParser switch state");
			return EXIT_FAILURE;
			break;
	}

	ret = 0;
	if (updateStateOnly != PARSE_NULL_STATE) {
		assert(updateStateAndByteArray == PARSE_NULL_STATE &&
			transition == TO_SAME_STATE_DEFAULT);
		parser->state = updateStateOnly;
	} else if (updateStateAndByteArray != PARSE_NULL_STATE) {
		assert(updateStateOnly == PARSE_NULL_STATE &&
			transition == TO_SAME_STATE_DEFAULT);
		if (!parser->iterateOnly)
			ret = UpdateByteArray(parser, nextChar);
		parser->state = updateStateAndByteArray;
	} else {
		assert(updateStateOnly == PARSE_NULL_STATE &&
			updateStateAndByteArray == PARSE_NULL_STATE);
		switch (transition) {
			case TO_SAME_STATE_DEFAULT:
				switch (parser->state) {
					case PARSE_INIT:
					case PARSE_OBJECT_OPEN_CURLY_BRACE:
					case PARSE_OBJECT_COMMA:
					case PARSE_KEY_ID_FINISH:
					case BEGIN_VALUE_PARSE:
					case PARSE_VALUE_FINISH:
					case PARSE_OBJECT_CLOSE_CURLY_BRACE:
					case PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN:
					case PARSE_ARRAY_CLOSE_BRACKET:
					case PARSE_ARRAY_CLOSE_BRACKET_AGAIN:
						break;
					case PARSE_KEY_ID:
					case PARSE_STRING:
					case PARSE_NUMBER_INTEGRAL:
					case PARSE_NUMBER_FRACTIONAL:
						if (!parser->iterateOnly)
							ret = UpdateByteArray(parser, nextChar);
						break;
					default:
						UtilsError("json_parser _IterateParser switch transition TO_SAME_STATE_DEFAULT");
						return EXIT_FAILURE;
						break;
				}
				break;
			case PARSE_INIT_TO_PARSE_OBJECT_OPEN_CURLY_BRACE:
			case PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_OBJECT_OPEN_CURLY_BRACE:
			case BEGIN_VALUE_PARSE_TO_PARSE_OBJECT_OPEN_CURLY_BRACE:
				if (!parser->iterateOnly)
					if (InitObjectMetadata(parser)) return EXIT_FAILURE;
					
				if (ArrayStack_Push(&parser->modeStack, &PARSER_OBJ)) return EXIT_FAILURE;
				parser->isParsingArray = false;
				
				parser->state = PARSE_OBJECT_OPEN_CURLY_BRACE;
				break;
			case PARSE_INIT_TO_PARSE_ARRAY_OPEN_BRACKET:
			case BEGIN_VALUE_PARSE_TO_PARSE_ARRAY_OPEN_BRACKET:
			case ARRAY_OPEN_BRACKET_AGAIN_TO_ARRAY_OPEN_BRACKET:
				if (!parser->iterateOnly)	
					if (InitArrayMetadata(parser)) return EXIT_FAILURE;
				
				if (ArrayStack_Push(&parser->modeStack, &PARSER_ARR)) return EXIT_FAILURE;
				parser->isParsingArray = true;
				
				parser->state = PARSE_ARRAY_OPEN_BRACKET;
				break;
			case ARRAY_OPEN_BRACKET_TO_ARRAY_OPEN_BRACKET_AGAIN:
				if (!parser->iterateOnly)	
					if (InitArrayMetadata(parser)) return EXIT_FAILURE;
				
				if (ArrayStack_Push(&parser->modeStack, &PARSER_ARR)) return EXIT_FAILURE;
				parser->isParsingArray = true;
				
				parser->state = PARSE_ARRAY_OPEN_BRACKET_AGAIN;
				break;
			case PARSE_KEY_ID_TO_PARSE_KEY_ID_FINISH:
				if (!parser->iterateOnly)
					ret = ParseKeyIdToParseKeyIdFinish(parser);
				parser->state = PARSE_KEY_ID_FINISH;
				break;
			case PARSE_STRING_TO_PARSE_VALUE_FINISH:
				if (!parser->iterateOnly) {
					if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
					if (UpdateByteArray(parser, (unsigned char) JP_STR)) return EXIT_FAILURE;
					if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
				}
				parser->state = PARSE_VALUE_FINISH;
				break;
			case PARSE_NUMBER_INTEGRAL_TO_PARSE_VALUE_FINISH:
			case PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_VALUE_FINISH:
			case PARSE_NUMBER_FRACTIONAL_TO_PARSE_VALUE_FINISH:
				if (!parser->iterateOnly)
					ret = ParseNumber(parser);
				parser->state = PARSE_VALUE_FINISH;
				break;
			case PARSE_NUMBER_INTEGRAL_TO_PARSE_ARRAY_CLOSE_BRACKET:
			case PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_ARRAY_CLOSE_BRACKET:
			case PARSE_NUMBER_FRACTIONAL_TO_PARSE_ARRAY_CLOSE_BRACKET:
				if (!parser->iterateOnly) {
					if (ParseNumber(parser)) return EXIT_FAILURE;
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				}
				RestoreIsParsingArray(parser);
				parser->state = PARSE_ARRAY_CLOSE_BRACKET;
				break;
			case PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
			case PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
			case PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
				if (!parser->iterateOnly) {
					if (ParseNumber(parser)) return EXIT_FAILURE;
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				}
				RestoreIsParsingArray(parser);
				parser->state = PARSE_OBJECT_CLOSE_CURLY_BRACE;
				break;
			case PARSE_NUMBER_INTEGRAL_TO_PARSE_OBJECT_COMMA:
			case PARSE_NUMBER_INTEGRAL_ZERO_TO_PARSE_OBJECT_COMMA:
			case PARSE_NUMBER_FRACTIONAL_TO_PARSE_OBJECT_COMMA:
				if (!parser->iterateOnly)
					ret = ParseNumber(parser);
				parser->state = PARSE_OBJECT_COMMA;
				break;
			case PARSE_NUMBER_INTEGRAL_TO_BEGIN_VALUE_PARSE:
			case PARSE_NUMBER_INTEGRAL_ZERO_TO_BEGIN_VALUE_PARSE:
			case PARSE_NUMBER_FRACTIONAL_TO_BEGIN_VALUE_PARSE:
				if (!parser->iterateOnly)
					ret = ParseNumber(parser);
				parser->state = BEGIN_VALUE_PARSE;
				break;
			case PARSE_BOOL_TRU_TO_PARSE_VALUE_FINISH:
				if (!parser->iterateOnly)
					ret = ParseBoolToParseValueFinish(parser, true);
				parser->state = PARSE_VALUE_FINISH;
				break;
			case PARSE_BOOL_FALS_TO_PARSE_VALUE_FINISH:
				if (!parser->iterateOnly)
					ret = ParseBoolToParseValueFinish(parser, false);
				parser->state = PARSE_VALUE_FINISH;
				break;
			case PARSE_NULL_NUL_TO_PARSE_VALUE_FINISH:
				if (!parser->iterateOnly) {
					if (UpdateByteArray(parser, (unsigned char) JP_NULL)) return EXIT_FAILURE;
					if (UpdateByteArray(parser, '\0')) return EXIT_FAILURE;
				}
				parser->state = PARSE_VALUE_FINISH;
				break;
			case PARSE_OBJECT_CLOSE_CURLY_BRACE_TO_PARSE_ARRAY_CLOSE_BRACKET:
			case PARSE_VALUE_FINISH_TO_PARSE_ARRAY_CLOSE_BRACKET:
			case PARSE_ARRAY_OPEN_BRACKET_TO_PARSE_ARRAY_CLOSE_BRACKET:
			case ARRAY_CLOSE_BRACKET_AGAIN_TO_ARRAY_CLOSE_BRACKET:
				//if (!parser->iterateOnly)
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				RestoreIsParsingArray(parser);
				parser->state = PARSE_ARRAY_CLOSE_BRACKET;
				break;
			case PARSE_VALUE_FINISH_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
			case PARSE_OBJECT_OPEN_CURLY_BRACE_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
			case PARSE_ARRAY_CLOSE_BRACKET_TO_PARSE_OBJECT_CLOSE_CURLY_BRACE:
			case OBJECT_CLOSE_CURLY_BRACE_AGAIN_TO_OBJECT_CLOSE_CURLY_BRACE:
				//if (!parser->iterateOnly)
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				RestoreIsParsingArray(parser);
				parser->state = PARSE_OBJECT_CLOSE_CURLY_BRACE;
				break;
			case OBJECT_CLOSE_CURLY_BRACE_TO_OBJECT_CLOSE_CURLY_BRACE_AGAIN:
				//if (!parser->iterateOnly)
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				RestoreIsParsingArray(parser);
				parser->state = PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN;
				break;
			case ARRAY_CLOSE_BRACKET_TO_ARRAY_CLOSE_BRACKET_AGAIN:
				//if (!parser->iterateOnly)
					//if (FinishParsingObjectOrArray(parser)) return EXIT_FAILURE;
				RestoreIsParsingArray(parser);
				parser->state = PARSE_ARRAY_CLOSE_BRACKET_AGAIN;
				break;
			default:
				UtilsError("json_parser _IterateParser switch transition main");
				return EXIT_FAILURE;
				break;
		}
	}
	
	if (LOG_LEVEL >= 7) {
		printf("DEBUG _IterateParser: Char %c State: %d iterateOnly: %d\n", nextChar, parser->state, \
			parser->iterateOnly);
	}
	
	if (ret != 0) {
		return ret;
	}

	return ret;
}

// List of possible states of the parser when the first character is parsed
// i.e. PARSE_BOOL_TR or PARSE_NULL_NUL cannot be on this list
// Neither can BEGIN_VALUE_PARSE be on this list
bool IsJustStartedParsingFirstValue(enum ParserState state) {
	return (state == PARSE_NUMBER_INTEGRAL ||
			state == PARSE_NUMBER_INTEGRAL_ZERO ||
			state == PARSE_NUMBER_NEG ||
			state == PARSE_NUMBER_FRACTIONAL ||
			state == PARSE_STRING ||
			state == PARSE_BOOL_T ||
			state == PARSE_BOOL_F ||
			state == PARSE_NULL_N);
}


/* Parsing functions end */

/* Interface functions start */

// TODO: Need assertions for value of byteArrIdx and byteArr where possible
void _CheckParserInterfaceState(struct ParserInterface *interface) {
	struct Parser *parser = &interface->_parser;
	
	if (parser->state != PARSE_END_OF_FILE) {
		switch (interface->_state) {
			case PARSER_INTERFACE_INITIALIZED:
				assert(parser->state == PARSE_INIT);
				break;
			case PARSER_INTERFACE_END_OF_FILE:
				assert(parser->state == PARSE_END_OF_FILE);
				break;
			case PARSER_INTERFACE_HAS_NEXT_TRUE:
				if (IsJustStartedParsingFirstValue(parser->state)) {
					if (!interface->skip) {
						assert(!parser->iterateOnly);
					} else {
						assert(parser->iterateOnly);
					}
				} else {
					switch (parser->state) {
						case PARSE_OBJECT_OPEN_CURLY_BRACE:
						case PARSE_OBJECT_COMMA:
							assert(!parser->isParsingArray);
							assert(parser->iterateOnly);
							break;
						case PARSE_ARRAY_OPEN_BRACKET:
						case PARSE_ARRAY_OPEN_BRACKET_AGAIN:
							assert(parser->isParsingArray);
							assert(parser->iterateOnly);
							break;
						case PARSE_KEY_ID:
							assert(!parser->isParsingArray);
							if (!interface->skip) {
								assert(!parser->iterateOnly);
							} else {
								assert(parser->iterateOnly);
							}
							break;
						case BEGIN_VALUE_PARSE:
							if (!interface->skip) {
								assert(!parser->iterateOnly);
							} else {
								assert(parser->iterateOnly);
							}
							break;
						default:
							UtilsError("_CheckParserInterfaceState PARSER_INTERFACE_HAS_NEXT_TRUE");
							assert(1 == 0);
							break;
					}
				}
				break;
			case PARSER_INTERFACE_HAS_NEXT_FALSE:
				assert(parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE || \
						parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET_AGAIN);
				break;
			case PARSER_INTERFACE_NEXT_IS_OBJ:
				assert(parser->state == PARSE_OBJECT_OPEN_CURLY_BRACE);
				assert(!parser->isParsingArray);
				assert(parser->iterateOnly);
				break;
			case PARSER_INTERFACE_NEXT_IS_ARR:
				assert(parser->state == PARSE_ARRAY_OPEN_BRACKET ||
						parser->state == PARSE_ARRAY_OPEN_BRACKET_AGAIN);
				assert(parser->isParsingArray);
				assert(parser->iterateOnly);
				break;
			case PARSER_INTERFACE_NEXT_IS_BOOL:
			case PARSER_INTERFACE_NEXT_IS_S_LONG:
			case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
			case PARSER_INTERFACE_NEXT_IS_STR:
			case PARSER_INTERFACE_NEXT_IS_NULL:
				switch (parser->state) {
					case PARSE_OBJECT_COMMA:
						assert(!parser->isParsingArray);
						break;
					case PARSE_OBJECT_CLOSE_CURLY_BRACE:
						break;
					case BEGIN_VALUE_PARSE:
						assert(parser->isParsingArray);
						break;
					case PARSE_ARRAY_CLOSE_BRACKET:
						break;
					default:
						UtilsError("_CheckParserInterfaceState PARSER_INTERFACE_NEXT_IS_X");
						assert(1 == 0);
						break;
				}
				break;
			case PARSER_INTERFACE_OBJ_EXPANDED:
				assert(parser->state == PARSE_OBJECT_OPEN_CURLY_BRACE);
				assert(!parser->isParsingArray);
				assert(parser->iterateOnly);
				break;
			case PARSER_INTERFACE_ARR_EXPANDED:
				assert(parser->state == PARSE_ARRAY_OPEN_BRACKET ||
						parser->state == PARSE_ARRAY_OPEN_BRACKET_AGAIN);
				assert(parser->isParsingArray);
				assert(parser->iterateOnly);
				break;
			case PARSER_INTERFACE_COLLAPSED:
				assert(parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE || \
						parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET_AGAIN);
				assert(parser->iterateOnly);
				break;
			default:
				UtilsError("_CheckParserInterfaceState state");
				assert(1 == 0);
				break;
		}
	}
}


enum ParserInterfaceCommand {
	P_CMD_GET_NEXT_TYPE,
	P_CMD_CHECK_HAS_NEXT,
	P_CMD_EXPAND,
	P_CMD_COLLAPSE
};


enum ParserInterfaceTransition {
	PIT__TO_SAME_STATE,
	INITIALIZED_TO_HAS_NEXT_X,
	HAS_NEXT_TRUE_TO_NEXT_IS_X,
	HAS_NEXT_FALSE_TO_COLLAPSED,
	NEXT_IS_OBJ_TO_OBJ_EXPANDED,
	NEXT_IS_ARR_TO_ARR_EXPANDED,
	OBJ_EXPANDED_TO_HAS_NEXT_X,
	ARR_EXPANDED_TO_HAS_NEXT_X,
	COLLAPSED_TO_HAS_NEXT_X,
	NEXT_IS_PRIMITIVE_TO_HAS_NEXT_X
};


/*
Return values:
	- 0: OK
	- EXIT_FAILURE: Error
	- PARSE_END_OF_FILE
	- PARSE_ERROR_UNEXPECTED_CHAR

*/
int _IterateParserInterface(struct ParserInterface *interface, const enum ParserInterfaceCommand cmd) {
	// Preconditions
	assert(interface != NULL);
	assert(interface->_state == PARSER_INTERFACE_INITIALIZED ||
			interface->_state == PARSER_INTERFACE_HAS_NEXT_TRUE ||
			interface->_state == PARSER_INTERFACE_HAS_NEXT_FALSE ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_OBJ ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_ARR ||
			interface->_state == PARSER_INTERFACE_OBJ_EXPANDED ||
			interface->_state == PARSER_INTERFACE_ARR_EXPANDED ||
			interface->_state == PARSER_INTERFACE_COLLAPSED ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_BOOL ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_S_LONG || 
			interface->_state == PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_STR ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_NULL);
	_CheckParserInterfaceState(interface);
	assert(cmd == P_CMD_CHECK_HAS_NEXT || cmd == P_CMD_GET_NEXT_TYPE || cmd == P_CMD_EXPAND || \
		cmd == P_CMD_COLLAPSE);
	
	int ret = 0;
	struct Parser *parser = &interface->_parser;
	enum ParserState *ps = &parser->state;
	
	// Shows valid commands available at each state
	switch (interface->_state) {
		case PARSER_INTERFACE_INITIALIZED:
			if (cmd != P_CMD_CHECK_HAS_NEXT) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_INITIALIZED");
				ret = EXIT_FAILURE;
			}
			
			break;
		case PARSER_INTERFACE_HAS_NEXT_TRUE:
			if (cmd != P_CMD_GET_NEXT_TYPE) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_HAS_NEXT_TRUE");
				ret = EXIT_FAILURE;
			}
			break;
		case PARSER_INTERFACE_HAS_NEXT_FALSE:
			if (cmd != P_CMD_COLLAPSE) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_HAS_NEXT_FALSE");
				ret = EXIT_FAILURE;
			}
			break;
		case PARSER_INTERFACE_NEXT_IS_OBJ:
		case PARSER_INTERFACE_NEXT_IS_ARR:
			if (cmd != P_CMD_EXPAND) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_NEXT_IS_OBJ/ARR");
				ret = EXIT_FAILURE;
			}
			break;
		case PARSER_INTERFACE_OBJ_EXPANDED:
		case PARSER_INTERFACE_ARR_EXPANDED:
			if (cmd != P_CMD_CHECK_HAS_NEXT) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_OBJ/ARR_EXPANDED");
				ret = EXIT_FAILURE;
			}
			break;
		case PARSER_INTERFACE_COLLAPSED:
			if (cmd != P_CMD_CHECK_HAS_NEXT) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_COLLAPSED");
				ret = EXIT_FAILURE;
			}
			break;
		case PARSER_INTERFACE_NEXT_IS_BOOL:
		case PARSER_INTERFACE_NEXT_IS_S_LONG:
		case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
		case PARSER_INTERFACE_NEXT_IS_STR:
		case PARSER_INTERFACE_NEXT_IS_NULL:
			if (cmd != P_CMD_CHECK_HAS_NEXT) {
				UtilsError("json_parser _IterateParserInterface switch cmd PARSER_INTERFACE_NEXT_IS_X");
				ret = EXIT_FAILURE;
			}
			break;
		default:
			UtilsError("json_parser _IterateParserInterface switch cmd validate");
			ret = EXIT_FAILURE;
			break;
	}
	
	if (!ret) {
		enum ParserInterfaceTransition transition = PIT__TO_SAME_STATE;
		
		/*
			Decision variables (keep small):
			interface->_state
			cmd
		*/
		switch (interface->_state) {
			case PARSER_INTERFACE_INITIALIZED:
				if (cmd == P_CMD_CHECK_HAS_NEXT) {
					transition = INITIALIZED_TO_HAS_NEXT_X;
				}
				break;
			case PARSER_INTERFACE_HAS_NEXT_TRUE:
				if (cmd == P_CMD_GET_NEXT_TYPE) {
					transition = HAS_NEXT_TRUE_TO_NEXT_IS_X;
				}
				break;
			case PARSER_INTERFACE_HAS_NEXT_FALSE:
				if (cmd == P_CMD_COLLAPSE) {
					transition = HAS_NEXT_FALSE_TO_COLLAPSED;
				}
				break;
			case PARSER_INTERFACE_NEXT_IS_OBJ:
				if (cmd == P_CMD_EXPAND) {
					transition = NEXT_IS_OBJ_TO_OBJ_EXPANDED;
				}
				break;
			case PARSER_INTERFACE_NEXT_IS_ARR:
				if (cmd == P_CMD_EXPAND) {
					transition = NEXT_IS_ARR_TO_ARR_EXPANDED;
				}
				break;
			case PARSER_INTERFACE_OBJ_EXPANDED:
				if (cmd == P_CMD_CHECK_HAS_NEXT) {
					transition = OBJ_EXPANDED_TO_HAS_NEXT_X;
				}
				break;
			case PARSER_INTERFACE_ARR_EXPANDED:
				if (cmd == P_CMD_CHECK_HAS_NEXT) {
					transition = ARR_EXPANDED_TO_HAS_NEXT_X;
				}
				break;
			case PARSER_INTERFACE_COLLAPSED:
				if (cmd == P_CMD_CHECK_HAS_NEXT) {
					transition = COLLAPSED_TO_HAS_NEXT_X;
				}
				break;
			case PARSER_INTERFACE_NEXT_IS_BOOL:
			case PARSER_INTERFACE_NEXT_IS_S_LONG:
			case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
			case PARSER_INTERFACE_NEXT_IS_STR:
			case PARSER_INTERFACE_NEXT_IS_NULL:
			 	if (cmd == P_CMD_CHECK_HAS_NEXT) {
			 		transition = NEXT_IS_PRIMITIVE_TO_HAS_NEXT_X;
				}
				break;
			default:
				UtilsError("json_parser _IterateParserInterface switch cmd execute");
				ret = EXIT_FAILURE;
				break;
		}
		
		/*
			Decision variables (keep small):
				
				parser->state
				parser->iterateOnly
				parser->byteArr ?
				parser->byteArrIdx ?
				parser->isParsingArray
			
			Members of the parser SM directly modified by the interface SM
			If this list is more than 3 to 4, refactor these so that they
			can happen directly in parser state transitions, to manage complexity:
				
				parser->iterateOnly
				parser->byteArr (Reset only)
				parser->byteArrIdx (Reset only)
				
		*/
		enum ParserState tempState = PARSE_NULL_STATE;
		switch (transition) {
			case INITIALIZED_TO_HAS_NEXT_X:
				while (*ps != PARSE_OBJECT_OPEN_CURLY_BRACE &&
						*ps != PARSE_ARRAY_OPEN_BRACKET) {
					if ((ret = _IterateParser(parser))) {
						break;
					}	
				}
				
				if (!ret) {
					assert(*ps == PARSE_OBJECT_OPEN_CURLY_BRACE || \
						*ps == PARSE_ARRAY_OPEN_BRACKET);
						
					interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
				}
				break;
			case HAS_NEXT_TRUE_TO_NEXT_IS_X:
				// If primitive, fully parse the value to get exact type, otherwise get type (object or array?)
				switch (*ps) {
					case PARSE_OBJECT_OPEN_CURLY_BRACE: // Always before EXPAND?
						interface->_state = PARSER_INTERFACE_NEXT_IS_OBJ;
						break;
					case PARSE_ARRAY_OPEN_BRACKET: // Always before EXPAND?
					case PARSE_ARRAY_OPEN_BRACKET_AGAIN:
						interface->_state = PARSER_INTERFACE_NEXT_IS_ARR;
						break;
					case PARSE_KEY_ID: // Always after EXPAND?
					case PARSE_OBJECT_COMMA:
					// Below: IsJustStartedParsingFirstValue states
					case BEGIN_VALUE_PARSE:
					case PARSE_NUMBER_INTEGRAL:
					case PARSE_NUMBER_INTEGRAL_ZERO:
					case PARSE_NUMBER_NEG:
					case PARSE_STRING:
					case PARSE_BOOL_T:
					case PARSE_BOOL_F:
					case PARSE_NULL_N:
						if (*ps == PARSE_KEY_ID || *ps == BEGIN_VALUE_PARSE || 
							IsJustStartedParsingFirstValue(*ps)) {
							if (!interface->skip)
								assert(!parser->iterateOnly);
						} else {
							assert(parser->iterateOnly == true);
							if (!interface->skip) {
								ResetByteArray(parser);
								parser->iterateOnly = false;
							}
						}
						
						// Combinational logic on:
						// isParsingArray
						// *ps
						if (parser->isParsingArray) {
							assert(*ps == BEGIN_VALUE_PARSE || IsJustStartedParsingFirstValue(*ps));
							if (*ps == BEGIN_VALUE_PARSE) {
								// Go to first character of value or opening curly brace or opening bracket
								while (*ps == BEGIN_VALUE_PARSE) {
									if ((ret = _IterateParser(parser))) break;
								}
							}
						} else {
							assert(*ps == PARSE_KEY_ID || *ps == PARSE_OBJECT_COMMA);
							// Key parsing happens here
							while (*ps != BEGIN_VALUE_PARSE) {
								if ((ret = _IterateParser(parser))) break;
							}
							
							if (!ret) {
								// Go to first character of value or opening curly brace or opening bracket
								while (*ps == BEGIN_VALUE_PARSE) {
									if ((ret = _IterateParser(parser))) break;
								}
							}
						}
						
						if (!ret) {
							assert(*ps == PARSE_OBJECT_OPEN_CURLY_BRACE || *ps == PARSE_ARRAY_OPEN_BRACKET ||
									IsJustStartedParsingFirstValue(*ps));
							
							if (*ps == PARSE_OBJECT_OPEN_CURLY_BRACE) {
								parser->iterateOnly = true;
								interface->_state = PARSER_INTERFACE_NEXT_IS_OBJ;
							} else if (*ps == PARSE_ARRAY_OPEN_BRACKET) {
								parser->iterateOnly = true;
								interface->_state = PARSER_INTERFACE_NEXT_IS_ARR;
							} else {
								// Parse primitive value
								while (*ps != PARSE_OBJECT_COMMA && \
										*ps != BEGIN_VALUE_PARSE && \
										*ps != PARSE_OBJECT_CLOSE_CURLY_BRACE && \
										*ps != PARSE_ARRAY_CLOSE_BRACKET) {
									if ((ret = _IterateParser(parser))) break;		
								}
								parser->iterateOnly = true;
								
								if (ret == 0) {
									if (!interface->skip) {
										enum ParserDataType valueType = \
											*(parser->byteArr + (parser->byteArrIdx - 2));
										switch (valueType) {
											case JP_BOOL:
												interface->_state = \
													PARSER_INTERFACE_NEXT_IS_BOOL;
												break;
											case JP_S_LONG:
												interface->_state = \
													PARSER_INTERFACE_NEXT_IS_S_LONG;
												break;
											case JP_LONG_DOUBLE:
												interface->_state = \
													PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE;
												break;
											case JP_STR:
												interface->_state = \
													PARSER_INTERFACE_NEXT_IS_STR;
												break;
											case JP_NULL:
												interface->_state = \
													PARSER_INTERFACE_NEXT_IS_NULL;
												break;
											default:
												UtilsError( \
								"json_parser _IterateParserInterface HAS_NEXT_TRUE_TO_NEXT_IS_X switch");
												ret = EXIT_FAILURE;
												break;
										}
									} else {
										// Hack: Specifically for __JsonParser_Skip, 
										// since no data is saved, in byteArr we set 
										// interface->_state to a random PARSER_INTERFACE_NEXT_IS_X state.
										interface->_state = PARSER_INTERFACE_NEXT_IS_BOOL;
									}
								}
							}
						}
						break;
					default:
						UtilsError("json_parser _IterateParserInterface HAS_NEXT_TRUE_TO_NEXT_IS_X");
						ret = EXIT_FAILURE;
						break;
				}
				break;
			case HAS_NEXT_FALSE_TO_COLLAPSED:
				assert(parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE || \
						parser->state == PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET || \
						parser->state == PARSE_ARRAY_CLOSE_BRACKET_AGAIN);
				interface->_state = PARSER_INTERFACE_COLLAPSED;
				
				// TODO: while modeStack len != same......
				
				break;
			case NEXT_IS_OBJ_TO_OBJ_EXPANDED:
				assert(*ps == PARSE_OBJECT_OPEN_CURLY_BRACE);
				// Nothing to be done, InitObjectMetadata already called
				interface->_state = PARSER_INTERFACE_OBJ_EXPANDED;
				break;
			case NEXT_IS_ARR_TO_ARR_EXPANDED:
				assert(*ps == PARSE_ARRAY_OPEN_BRACKET ||
						*ps == PARSE_ARRAY_OPEN_BRACKET_AGAIN);
				interface->_state = PARSER_INTERFACE_ARR_EXPANDED;
				break;
			case OBJ_EXPANDED_TO_HAS_NEXT_X:
				assert(*ps == PARSE_OBJECT_OPEN_CURLY_BRACE);
				ResetByteArray(parser);
				while (*ps == PARSE_OBJECT_OPEN_CURLY_BRACE) {
					if ((ret = _IterateParser(parser))) {
						break;
					}
				}
				
				if (!ret) {
					assert(*ps == PARSE_KEY_ID || *ps == PARSE_OBJECT_CLOSE_CURLY_BRACE);
					switch (*ps) {
						case PARSE_KEY_ID:
							if (!interface->skip) {
								ResetByteArray(parser);
								parser->iterateOnly = false;
							}
							interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
							break;
						case PARSE_OBJECT_CLOSE_CURLY_BRACE:
							interface->_state = PARSER_INTERFACE_HAS_NEXT_FALSE;
							break;
						default:
							UtilsError("json_parser _IterateParserInterface OBJ_EXPANDED_TO_HAS_NEXT_X");
							ret = EXIT_FAILURE;
							break;
					}
				}
				break;
			case ARR_EXPANDED_TO_HAS_NEXT_X:
				if (!interface->skip) {
					ResetByteArray(parser);
					parser->iterateOnly = false;
				}
				tempState = *ps;
				while (*ps == tempState) {
					if ((ret = _IterateParser(parser))) {
						break;
					}
				}
				
				if (!ret) {
					assert(*ps == BEGIN_VALUE_PARSE ||
							*ps == PARSE_ARRAY_CLOSE_BRACKET ||
							*ps == PARSE_ARRAY_OPEN_BRACKET ||
							*ps == PARSE_ARRAY_OPEN_BRACKET_AGAIN ||
							*ps == PARSE_OBJECT_OPEN_CURLY_BRACE ||
							IsJustStartedParsingFirstValue(*ps));
					if (IsJustStartedParsingFirstValue(*ps)) {
						interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
					} else {
						switch (*ps) {
							case BEGIN_VALUE_PARSE:
								interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
								break;
							case PARSE_ARRAY_OPEN_BRACKET:
							case PARSE_ARRAY_OPEN_BRACKET_AGAIN:
							case PARSE_OBJECT_OPEN_CURLY_BRACE:
								parser->iterateOnly = true;
								interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
								break;
							case PARSE_ARRAY_CLOSE_BRACKET:
								parser->iterateOnly = true;
								interface->_state = PARSER_INTERFACE_HAS_NEXT_FALSE;
								break;
							default:
								UtilsError("json_parser _IterateParserInterface OBJ_EXPANDED_TO_HAS_NEXT_X");
								ret = EXIT_FAILURE;
								break;
						}
					}
				}				
				break;
			case COLLAPSED_TO_HAS_NEXT_X:
				assert(*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE || \
						*ps == PARSE_ARRAY_CLOSE_BRACKET || \
						*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN || \
						*ps == PARSE_ARRAY_CLOSE_BRACKET_AGAIN);
				tempState = *ps;
				while (*ps == tempState) {
					if ((ret = _IterateParser(parser))) {
						break;
					}
				}
				
				if (!ret) {
					assert(*ps == BEGIN_VALUE_PARSE || *ps == PARSE_OBJECT_COMMA || \
						*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE || \
						*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN || \
						*ps == PARSE_ARRAY_CLOSE_BRACKET || \
						*ps == PARSE_ARRAY_CLOSE_BRACKET_AGAIN);
					if (*ps == BEGIN_VALUE_PARSE || *ps == PARSE_OBJECT_COMMA) {
						if (*ps == BEGIN_VALUE_PARSE) {
							assert(parser->isParsingArray);
							if (!interface->skip) {
								ResetByteArray(parser);
								parser->iterateOnly = false;
							}
						}
						interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
					} else {
						interface->_state = PARSER_INTERFACE_HAS_NEXT_FALSE;
					}
				}
				break;
			case NEXT_IS_PRIMITIVE_TO_HAS_NEXT_X:
				assert(*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE ||
						*ps == PARSE_OBJECT_COMMA ||
						*ps == PARSE_ARRAY_CLOSE_BRACKET ||
						*ps == BEGIN_VALUE_PARSE);
				if (*ps == PARSE_OBJECT_CLOSE_CURLY_BRACE || *ps == PARSE_ARRAY_CLOSE_BRACKET) {
					interface->_state = PARSER_INTERFACE_HAS_NEXT_FALSE;
				} else {
					if (*ps == BEGIN_VALUE_PARSE) {
						assert(parser->isParsingArray);
						if (!interface->skip) {
							ResetByteArray(parser);
							parser->iterateOnly = false;
						}
					}
					interface->_state = PARSER_INTERFACE_HAS_NEXT_TRUE;
				}
				break;
			default:
				UtilsError("json_parser _IterateParserInterface switch transition");
				ret = EXIT_FAILURE;
				break;
		}
	}
	
	// Postconditions
	assert(ret == 0 || ret == EXIT_FAILURE || ret == PARSE_END_OF_FILE || ret == PARSE_ERROR_UNEXPECTED_CHAR);
	
	if (ret == 0 || ret == PARSE_END_OF_FILE || ret == PARSE_ERROR_UNEXPECTED_CHAR)
		_CheckParserInterfaceState(interface);
	
	switch (ret) {
		case 0:
			assert(interface->_state == PARSER_INTERFACE_HAS_NEXT_TRUE ||
					interface->_state == PARSER_INTERFACE_HAS_NEXT_FALSE ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_OBJ ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_ARR ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_BOOL ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_S_LONG || 
					interface->_state == PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_STR ||
					interface->_state == PARSER_INTERFACE_NEXT_IS_NULL || 
					interface->_state == PARSER_INTERFACE_OBJ_EXPANDED ||
					interface->_state == PARSER_INTERFACE_ARR_EXPANDED ||
					interface->_state == PARSER_INTERFACE_COLLAPSED);
			break;
		case EXIT_FAILURE:
			break;
		case PARSE_END_OF_FILE:
			interface->_state = PARSER_INTERFACE_END_OF_FILE;
			break;
		case PARSE_ERROR_UNEXPECTED_CHAR:
			interface->_state = PARSER_INTERFACE_ERROR;
			break;
			
	}
	
	return ret;
}


/*
Return value: The bool value if errCode == 0

Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_BOOL as the return value.

*/
bool JsonParser_GetBoolValue(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	if (interface->_state != PARSER_INTERFACE_NEXT_IS_BOOL) {
		*errCode = EXIT_FAILURE;
		return EXIT_FAILURE;
	}
	
	unsigned char *byteArrPtr = interface->_parser.byteArr + interface->_parser.byteArrIdx;
	assert(*(byteArrPtr - 2) == JP_BOOL);
	
	return *((bool*) (byteArrPtr - 2 - 1));
}


/*
Return value: The long value if errCode == 0

Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_S_LONG as the return value.

*/
long JsonParser_GetSLongValue(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	if (interface->_state != PARSER_INTERFACE_NEXT_IS_S_LONG) {
		*errCode = EXIT_FAILURE;
		return EXIT_FAILURE;
	}
	
	unsigned char *byteArrPtr = interface->_parser.byteArr + interface->_parser.byteArrIdx;
	assert(*(byteArrPtr - 2) == JP_S_LONG);
	
	return *((long*) (byteArrPtr - 2 - 8));
}


/*
Return value: The long double value if errCode == 0

Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_LONG_DOUBLE as the return value.

*/
long double JsonParser_GetLongDoubleValue(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	if (interface->_state != PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE) {
		*errCode = EXIT_FAILURE;
		return EXIT_FAILURE;
	}
	
	unsigned char *byteArrPtr = interface->_parser.byteArr + interface->_parser.byteArrIdx;
	assert(*(byteArrPtr - 2) == JP_LONG_DOUBLE);
	
	return *((long double*) (byteArrPtr - 2 - 16));
}


/*
Return value: Pointer to null-terminated string if errCode == 0

Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_STR as the return value.

*/
char* JsonParser_GetStringValue(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	if (interface->_state != PARSER_INTERFACE_NEXT_IS_STR) {
		*errCode = EXIT_FAILURE;
		return NULL;
	}
	
	unsigned char *byteArrPtr = interface->_parser.byteArr + interface->_parser.byteArrIdx;
	assert(*(byteArrPtr - 2) == JP_STR);
	
	// Init ptr to location of null term
	byteArrPtr -= 3;
	assert(*byteArrPtr == '\0');
	
	// Is a null string if the previous byte from the current one is also a null term
	// (the null term representing the beginning of the byte array)
	if (*(byteArrPtr - 1) != '\0') {
		do {
			--byteArrPtr;		
		} while (*byteArrPtr != '\0');
		++byteArrPtr;
	}
	
	return (char*) byteArrPtr;
}


/*
Return value: A pointer to the start of the key value string.
	If the string is empty, points to a null character.

Error codes:
	- EXIT_FAILURE: The current position of the parser may be in an array, or
		it is in an object but the last call to the interface was not a call to JsonParser_GetNextType
		that returned without an error.

*/
const char * JsonParser_GetKeyValue(struct ParserInterface *interface, int * const errCode) { 
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	if (!(interface->_state == PARSER_INTERFACE_NEXT_IS_BOOL ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_S_LONG || 
			interface->_state == PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_STR ||
			interface->_state == PARSER_INTERFACE_NEXT_IS_NULL ||   
			interface->_state == PARSER_INTERFACE_NEXT_IS_ARR || 
			interface->_state == PARSER_INTERFACE_NEXT_IS_OBJ)) {
		*errCode = EXIT_FAILURE;
		return NULL;
	}
	
	// Set ptr to location of null term of key value string
	unsigned char *byteArrPtr = interface->_parser.byteArr + interface->_parser.byteArrIdx;
	if (interface->_state == PARSER_INTERFACE_NEXT_IS_ARR ||
		interface->_state == PARSER_INTERFACE_NEXT_IS_OBJ) {
		byteArrPtr -= 1;
	} else {
		// Init ptr to location of value type
		byteArrPtr -= 2;
		switch (interface->_state) {
			case PARSER_INTERFACE_NEXT_IS_BOOL:
				byteArrPtr -= 1;
				break;
			case PARSER_INTERFACE_NEXT_IS_S_LONG:
				byteArrPtr -= 8;
				break;
			case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
				byteArrPtr -= 16;
				break;
			case PARSER_INTERFACE_NEXT_IS_STR:
				// Go to beginning char of string value
				--byteArrPtr;
				if (*(byteArrPtr - 1) != '\0') {
					do {
						--byteArrPtr;		
					} while (*byteArrPtr != '\0');
					++byteArrPtr;
				}
				break;
			case PARSER_INTERFACE_NEXT_IS_ARR:
			case PARSER_INTERFACE_NEXT_IS_OBJ:
			case PARSER_INTERFACE_NEXT_IS_NULL:
				break;
			default:
				UtilsError("JsonParser_GetKeyValue switch");
				*errCode = EXIT_FAILURE;
				return NULL;
		}
		--byteArrPtr;
	}
	
	// Is a null string if the previous byte from the current one is also a null term
	// (the null term representing the beginning of the byte array)
	if (*(byteArrPtr - 1) != '\0') {
		do {
			--byteArrPtr;		
		} while (*byteArrPtr != '\0');
		++byteArrPtr;
	}
		
	return (char*) byteArrPtr;
}


/*
Return values:
	0: Object/array collapsed successfully.
	EXIT_FAILURE: Error
*/
int JsonParser_Collapse(struct ParserInterface *interface) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	int ret = _IterateParserInterface(interface, P_CMD_COLLAPSE);
	assert(ret == 0 || ret == EXIT_FAILURE);
	
	return ret;
}


/*
Return values:
	0: Object/array expanded successfully.
	EXIT_FAILURE: Error
*/
int JsonParser_Expand(struct ParserInterface *interface) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	int ret = _IterateParserInterface(interface, P_CMD_EXPAND);
	assert(ret == 0 || ret == EXIT_FAILURE);
	
	return ret;
}


/*
Return values:
	JP_OBJ: Object
	JP_ARR: Array
	
Error codes:
	- EXIT_FAILURE: Error
*/
enum ParserDataType JsonParser_GetNextType(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	enum ParserDataType dataType = 0;
	*errCode = 0;
	
	int ret = _IterateParserInterface(interface, P_CMD_GET_NEXT_TYPE);
	assert(ret == 0 || ret == EXIT_FAILURE);
	switch (ret) {
		case 0:
			switch (interface->_state) {
				case PARSER_INTERFACE_NEXT_IS_OBJ:
					dataType = JP_OBJ;
					break;
				case PARSER_INTERFACE_NEXT_IS_ARR:
					dataType = JP_ARR;
					break;
				case PARSER_INTERFACE_NEXT_IS_BOOL:
					dataType = JP_BOOL;
					break;
				case PARSER_INTERFACE_NEXT_IS_S_LONG:
					dataType = JP_S_LONG;
					break;
				case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
					dataType = JP_LONG_DOUBLE;
					break;
				case PARSER_INTERFACE_NEXT_IS_STR:
					dataType = JP_STR;
					break;
				case PARSER_INTERFACE_NEXT_IS_NULL:
					dataType = JP_NULL;
					break;
				default:
					UtilsError("json_parser JsonParser_GetNextType switch");
					*errCode = EXIT_FAILURE;
					break;
			}
			break;
		case EXIT_FAILURE:
			*errCode = ret;
			break;
	}
	
	// TODO: Postconditions
	
	return dataType;
}


/*
Return values:
	JP_OBJ: Object
	JP_ARR: Array
	
Error codes:
	- EXIT_FAILURE: Error
*/
enum ParserDataType JsonParser_GetCurrentType(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	enum ParserDataType dataType = 0;
	*errCode = 0;
	
	switch (interface->_state) {
		case PARSER_INTERFACE_NEXT_IS_OBJ:
			dataType = JP_OBJ;
			break;
		case PARSER_INTERFACE_NEXT_IS_ARR:
			dataType = JP_ARR;
			break;
		case PARSER_INTERFACE_NEXT_IS_BOOL:
			dataType = JP_BOOL;
			break;
		case PARSER_INTERFACE_NEXT_IS_S_LONG:
			dataType = JP_S_LONG;
			break;
		case PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE:
			dataType = JP_LONG_DOUBLE;
			break;
		case PARSER_INTERFACE_NEXT_IS_STR:
			dataType = JP_STR;
			break;
		case PARSER_INTERFACE_NEXT_IS_NULL:
			dataType = JP_NULL;
			break;
		default:
			UtilsError("json_parser JsonParser_GetCurrentType switch");
			*errCode = EXIT_FAILURE;
			break;
	}
	
	// TODO: Postconditions
	
	return dataType;
}


/*
Return values:
	true: The parser sees that a potential object is available to be parsed, although at this point
			cannot guarantee that there will be no parsing errors during the actual parsing of the
			object made with the next available command, JsonParser_GetNextType.
	false: There are no additional objects available within this object/array.

Error codes:
	- EXIT_FAILURE: Error
	- PARSE_END_OF_FILE
	- PARSE_ERROR_UNEXPECTED_CHAR
*/
bool JsonParser_HasNext(struct ParserInterface *interface, int * const errCode) {
	// Preconditions
	assert(interface != NULL);
	_CheckParserInterfaceState(interface);
	
	bool hasNext = false;
	*errCode = 0;
	
	int ret = _IterateParserInterface(interface, P_CMD_CHECK_HAS_NEXT);
	if (ret == 0) {
		switch (interface->_state) {
			case PARSER_INTERFACE_HAS_NEXT_TRUE:
				hasNext = true;
				break;
			case PARSER_INTERFACE_HAS_NEXT_FALSE:
				break;
			default:
				UtilsError("json_parser JsonParser_HasNext switch");
				ret = EXIT_FAILURE;
				break;
		}
	} else {
 		*errCode = ret;
	}
	
	return hasNext;
}


int _JsonParser_SkipObjArr(struct ParserInterface *interface, enum ParserDataType initialDataType) {
	// If the member is an object/array, "skip" the member by expanding, iterating through
	// all child members recursively, and then collapsing the member
	assert(initialDataType == JP_OBJ || initialDataType == JP_ARR);

#ifdef DEBUG
	printf("JsonParser_SkipObjArr; initialDataType: %d\n", initialDataType);
#endif

	int errCode = 0;

	// Turning on skip prevents saving unneeded parsed data into byteArr by permanently
	// keeping iterateOnly on
	interface->skip = true;
	interface->_parser.iterateOnly = true;
	
	errCode = JsonParser_Expand(interface);
	if (errCode != 0) {
		goto cleanup__JsonParser_SkipObjArr;
	}
	
	size_t levels = 1;
	while (levels) {
		bool hasNext = JsonParser_HasNext(interface, &errCode);
		if (errCode != 0) {
			goto cleanup__JsonParser_SkipObjArr;
		}
		
		if (!hasNext) {
			errCode = JsonParser_Collapse(interface);
			if (errCode != 0) {
				goto cleanup__JsonParser_SkipObjArr;
			}
			--levels;
		} else {
			enum ParserDataType dataType = JsonParser_GetNextType(interface, &errCode);
			if (errCode != 0) {
				goto cleanup__JsonParser_SkipObjArr;
			}
		
			if (dataType == JP_OBJ || dataType == JP_ARR) {
				errCode = JsonParser_Expand(interface);
				if (errCode != 0) {
					goto cleanup__JsonParser_SkipObjArr;
				}
				++levels;
													
			}
		}
	}
	
	cleanup__JsonParser_SkipObjArr:
	interface->skip = false;
	
	return errCode;
}


/*
Examples:
	x.y
	x[0]
	[2]
	obj
	["obj"]
	["x"].x[2][1].x.y.b[5]
	["objWithPeriodsInKey...."].x
	
	Single quotes currently unsupported (ex. ['obj'])
*/
bool JsonParser_GoTo(struct ParserInterface *interface, const char * const path, int *errCode) {
	// Preconditions
	assert(interface != NULL);
	assert(interface->_state == PARSER_INTERFACE_INITIALIZED);
	_CheckParserInterfaceState(interface);
	// TODO: Validate path
	
	size_t pathIdx = 0;
	while (path[pathIdx] == '[' || path[pathIdx] == '\"')
		++pathIdx;

	bool hasNext = JsonParser_HasNext(interface, errCode);
	if (*errCode != 0) {
		goto cleanup_JsonParser_GoTo;
	}
	
	if (!hasNext) {
		*errCode = EXIT_FAILURE;
		goto cleanup_JsonParser_GoTo;
	}
	
	enum ParserDataType dataType = JsonParser_GetNextType(interface, errCode);
	if (*errCode != 0) {
		goto cleanup_JsonParser_GoTo;
	} 
	if (!(dataType == JP_OBJ || dataType == JP_ARR)) {
		*errCode = EXIT_FAILURE;
		goto cleanup_JsonParser_GoTo;
	}
	
	*errCode = JsonParser_Expand(interface);
	if (*errCode != 0) {
		goto cleanup_JsonParser_GoTo;
	}

	unsigned char mode = 0;
	size_t arrTargetIdx = 0;
	if (dataType == JP_OBJ) {
		mode = PARSER_OBJ;
	} else {
		mode = PARSER_ARR;
		
		// Parse target idx in array
		errno = 0;
		char *endptr;
		arrTargetIdx = strtoull(path + pathIdx, &endptr, 10);
		if (!(endptr != path + pathIdx && errno != 0)) {
			UtilsError("JsonParser_GoTo strtoull");
			*errCode = EXIT_FAILURE;
			goto cleanup_JsonParser_GoTo;
		}
		assert(*endptr == ']');
	}
	
	size_t arrCurrTargetIdx = 0;
	bool locationFound = false;
	bool stopSearch = false;
	while (!stopSearch && !locationFound) {
		hasNext = JsonParser_HasNext(interface, errCode);
		if (*errCode != 0) {
			goto cleanup_JsonParser_GoTo;
		}
		
		if (!hasNext) {
			stopSearch = true;
			
		} else {
			dataType = JsonParser_GetNextType(interface, errCode);
			if (*errCode != 0) {
				goto cleanup_JsonParser_GoTo;
			}
		
			bool memberFound = false;
			if (mode == PARSER_OBJ) {
				const char * key = JsonParser_GetKeyValue(interface, errCode);
				if (*errCode != 0) {
					goto cleanup_JsonParser_GoTo;
				}
				
				if (strncmp(path + pathIdx, key, strlen(key)) == 0) {
					memberFound = true;
					
					// If this is the last member, set locationFound, else move path idx forward
					pathIdx += strlen(key);
					char c = path[pathIdx];
					++pathIdx;

					if (c == '\"') {
						*errCode = ValidateAndAdvanceStrIdx("]", path, &pathIdx); 
						if (*errCode != 0) goto cleanup_JsonParser_GoTo;
						c = path[pathIdx];
						++pathIdx;
					}

					switch (c) {
						case '.':
							break;
						case '[':
							c = path[pathIdx];
							if (c == '\"') {
								++pathIdx;
							}
							break;
						case '\0':
							locationFound = true;
							break;
						default:
							UtilsError("JsonParser_GoTo switch PARSER_OBJ 1 %c %lu", c, pathIdx);
							*errCode = EXIT_FAILURE;
							goto cleanup_JsonParser_GoTo;
					}
										
				} else {
					if (dataType == JP_OBJ || dataType == JP_ARR) {
						*errCode = _JsonParser_SkipObjArr(interface, dataType);
						if (*errCode != 0) {
							goto cleanup_JsonParser_GoTo;
						}
					}
				}
				
			} else { // PARSER_ARR
				if (arrCurrTargetIdx == arrTargetIdx) {
					memberFound = true;
					
					// If this is the last member, set locationFound, else move path idx forward
					while (path[pathIdx] != ']')
						++pathIdx;
					++pathIdx;
					char c = path[pathIdx];
					switch (c) {
						case '\0':
							locationFound = true;
							break;
						case '.':
							++pathIdx;
							break;
						case '[':
							++pathIdx;
							c = path[pathIdx];
							if (c == '\"') {
								++pathIdx;
							}
							break;
						default:
							UtilsError("JsonParser_GoTo switch PARSER_ARR 2 %c %lu", c, pathIdx);
							*errCode = EXIT_FAILURE;
							goto cleanup_JsonParser_GoTo;
					}
					
				} else {
					++arrCurrTargetIdx;
					if (dataType == JP_OBJ || dataType == JP_ARR) {
						*errCode = _JsonParser_SkipObjArr(interface, dataType);
						if (*errCode != 0) {
							goto cleanup_JsonParser_GoTo;
						}
					}
				}
			}
		
			if (memberFound && !locationFound) {
				if (dataType == JP_OBJ || dataType == JP_ARR) {
					if (dataType == JP_OBJ) {
						mode = PARSER_OBJ;
					} else {
						mode = PARSER_ARR;
						
						// Parse target idx in array
						errno = 0;
						char *endptr;
						arrTargetIdx = strtoull(path + pathIdx, &endptr, 10);
						if (!(endptr != path + pathIdx && errno == 0)) {
							UtilsError("JsonParser_GoTo strtoull");
							*errCode = EXIT_FAILURE;
							goto cleanup_JsonParser_GoTo;
						}
						assert(*endptr == ']');
						arrCurrTargetIdx = 0;
					}
					
					*errCode = JsonParser_Expand(interface);
					if (*errCode != 0) {
						goto cleanup_JsonParser_GoTo;
					}
					
				} else {
					// Error: Attempting to expand a member that is not an object/array
					stopSearch = true;
				}
			}
		}
	}
	
	cleanup_JsonParser_GoTo:
	
	return (!stopSearch && locationFound);
}


int JsonParser_Debug_PrintCurrent(struct ParserInterface *parserInterface) {
	int errCode = 0;
	
	enum ParserDataType dataType = JsonParser_GetCurrentType(parserInterface, &errCode);
	if (errCode) return errCode;

	if (dataType == JP_OBJ || dataType == JP_ARR) {
		if (dataType == JP_OBJ) {
			printf("Object\n");
		} else {
			printf("Array\n");
		}
		
	} else {
		bool boolValue = false;
		long longValue = 0;
		long double longDoubleValue = 0;
		char * strValue = NULL;
		switch (dataType) {
			case JP_BOOL:
				boolValue = JsonParser_GetBoolValue(parserInterface, &errCode);
				assert(errCode == 0);
				printf("bool %d\n", boolValue);
				break;
			case JP_S_LONG:
				longValue = JsonParser_GetSLongValue(parserInterface, &errCode);
				assert(errCode == 0);
				printf("long %ld\n", longValue);
				break;
			case JP_LONG_DOUBLE:
				longDoubleValue = JsonParser_GetLongDoubleValue(parserInterface, &errCode);
				assert(errCode == 0);
				printf("long double %Lf\n", longDoubleValue);
				break;
			case JP_STR:
				strValue = JsonParser_GetStringValue(parserInterface, &errCode);
				assert(errCode == 0);
				printf("str %s\n", strValue);
				break;
			case JP_NULL:
				printf("NULL\n");
				break;
			default:
				assert(1 == 0);
				break;
		}
	}
	
	return errCode;
}


void JsonParser_Debug_PrintAll(struct ParserInterface *parserInterface, int *errCode) {
	*errCode = 0;

	bool hasNext = JsonParser_HasNext(parserInterface, errCode);
	if (*errCode != 0) {
		assert(*errCode == PARSE_END_OF_FILE);
		return;
	}	
	assert(hasNext);
	
	enum ParserDataType dataType = JsonParser_GetNextType(parserInterface, errCode);
	assert(*errCode == 0);
	assert(dataType == JP_OBJ || dataType == JP_ARR);
	
	*errCode = JsonParser_Expand(parserInterface);
	assert(*errCode == 0);

	struct ArrayStack modeStack;
	*errCode = ArrayStack_Init(&modeStack, 1);
	assert(*errCode == 0);
	unsigned char mode = 0;
	if (dataType == JP_OBJ) {
		mode = PARSER_OBJ;
		assert(ArrayStack_Push(&modeStack, &PARSER_OBJ) == 0);
		printf("{\n");
	} else {
		mode = PARSER_ARR;
		assert(ArrayStack_Push(&modeStack, &PARSER_ARR) == 0);
		printf("[\n");
	}
	
	while (!ArrayStack_IsEmpty(&modeStack)) {
		//ArrayStack_Debug_Print(&modeStack);
		hasNext = JsonParser_HasNext(parserInterface, errCode);
		assert(*errCode == 0);
		if (!hasNext) {
			for (size_t i = 0; i < ArrayStack_Count(&modeStack) - 1; ++i)
				printf("\t");

			if (mode == PARSER_OBJ) {
				printf("}\n");
			} else {
				printf("]\n");
			}
			*errCode = ArrayStack_Pop(&modeStack, &mode);
			
			assert(*errCode == 0);
			if (!ArrayStack_IsEmpty(&modeStack))
				assert(ArrayStack_Peek(&modeStack, &mode) == 0);
			
			*errCode = JsonParser_Collapse(parserInterface);
			assert(*errCode == 0);
			
		} else {
			dataType = JsonParser_GetNextType(parserInterface, errCode);
			assert(*errCode == 0);
		
			for (size_t i = 0; i < ArrayStack_Count(&modeStack); ++i)
				printf("\t");

			if (mode == PARSER_OBJ) {
				const char * key = JsonParser_GetKeyValue(parserInterface, errCode);
				assert(*errCode == 0);
				printf("\"%s\": ", key);
			}
		
			if (dataType == JP_OBJ || dataType == JP_ARR) {
				if (dataType == JP_OBJ) {
					mode = PARSER_OBJ;
					assert(ArrayStack_Push(&modeStack, &PARSER_OBJ) == 0);
				} else {
					mode = PARSER_ARR;
					assert(ArrayStack_Push(&modeStack, &PARSER_ARR) == 0);
				}
				if (dataType == JP_OBJ) {
					printf("{\n");
				} else {
					printf("[\n");
				}
				
				*errCode = JsonParser_Expand(parserInterface);
				assert(*errCode == 0);
				
			} else {
				bool boolValue = false;
				long longValue = 0;
				long double longDoubleValue = 0;
				char * strValue = NULL;
				switch (dataType) {
					case JP_BOOL:
						boolValue = JsonParser_GetBoolValue(parserInterface, errCode);
						assert(*errCode == 0);

						char *boolStr = NULL;
						if (boolValue) {
							boolStr = "true";
						} else {
							boolStr = "false";
						}

						printf("bool %s\n", boolStr);
						break;
					case JP_S_LONG:
						longValue = JsonParser_GetSLongValue(parserInterface, errCode);
						assert(*errCode == 0);
						printf("long %ld\n", longValue);
						break;
					case JP_LONG_DOUBLE:
						longDoubleValue = JsonParser_GetLongDoubleValue(parserInterface, errCode);
						assert(*errCode == 0);
						printf("long double %Lf\n", longDoubleValue);
						break;
					case JP_STR:
						strValue = JsonParser_GetStringValue(parserInterface, errCode);
						assert(*errCode == 0);
						printf("str \"%s\"\n", strValue);
						break;
					case JP_NULL:
						printf("NULL\n");
						break;
					default:
						assert(1 == 0);
						break;
				}
			}
		}
	}	
}


int JsonParser_Init(struct ParserInterface *parserInterface, char *filePath) {
	assert(EXIT_FAILURE != PARSE_END_OF_FILE); // _IterateParserInterface

	// Init ParserInterface struct
	parserInterface->_state = PARSER_INTERFACE_INITIALIZED;
	parserInterface->skip = false;
	
	// Init Parser struct
	struct Parser *parser = &parserInterface->_parser;

	parser->state = PARSE_INIT;
	parser->lineNum = 0;
	parser->colNum = 0;
	
	parser->jsonFile = fopen(filePath, "rb");
	if (CheckNotNull(parser->jsonFile, "json_parser InitParser fopen")) {
		return EXIT_FAILURE;
	}
	
	parser->byteArr = malloc(16);
	*parser->byteArr = '\0'; // For when looping through byteArr to get the address of the first character of the first variable ID
	parser->byteArrMaxItems = 16;
	parser->byteArrIdx = 1;
	parser->addrArr = malloc(16 * sizeof VOID_PTR);
	parser->addrArrMaxItems = 16;
	parser->addrArrIdx = 0;
	
	// Init stacks
	if (CheckNotZero(ArrayStack_Init(&parser->modeStack, sizeof(unsigned char)), "json_parser InitParser modeStack") || \
		CheckNotZero(ArrayStack_Init(&parser->addrStack, sizeof VOID_PTR), "json_parser InitParser addrStack") || \
		CheckNotZero(ArrayStack_Init(&parser->memberCountStack, sizeof(size_t)), \
			"json_parser InitParser memberCountStack")) {
		return EXIT_FAILURE;
	}
	
	parser->isParsingArray = false;
	parser->iterateOnly = true;
	
	return 0;
}


void JsonParser_Free(struct ParserInterface *parserInterface) {
	struct Parser *parser = &parserInterface->_parser;

	fclose(parser->jsonFile); // TODO: error checking?
	parser->jsonFile = NULL;
	
	free(parser->byteArr);
	parser->byteArr = NULL;
	free(parser->addrArr);
	parser->addrArr = NULL;
	
	ArrayStack_Free(&parser->modeStack);
	ArrayStack_Free(&parser->addrStack);
	ArrayStack_Free(&parser->memberCountStack);
}


/* Interface functions end */

/*
int main(int argc, char *argv[]) {
	int mainRet;
	if ((mainRet = CheckAssumptions())) return EXIT_FAILURE;
	
	int ret = 0;
	
	if (argc != 3) {
		printf("json_parser options: [filePath] [objectPath]\n");
		return EXIT_FAILURE;
	}
	
	if (errno != 0) {
		return EXIT_FAILURE;
	}
	
	// goto cleanup beyond this point
		
	struct ParserInterface parserInterface;
	JsonParser_Init(&parserInterface, argv[1]);
	
	//JsonParser_Debug_PrintAll(&parserInterface, &ret);
	//assert(ret == 0);
	
	bool found = JsonParser_GoTo(&parserInterface, argv[2], &ret);
	assert(found);
	assert(ret == 0);
	
	ret = JsonParser_Debug_PrintCurrent(&parserInterface);
	assert(ret == 0);
	
	cleanup:
	JsonParser_Free(&parserInterface);
	
	return mainRet;
}
*/

