#include <math.h>
#include <float.h>
#include "utils.h"


enum ParserDataType { // Max count: UCHAR_MAX
	JP_OBJ,
	JP_ARR,
	JP_BOOL,
	JP_S_CHAR,
	JP_S_INT,
	JP_S_LONG,
	JP_FLOAT,
	JP_LONG_DOUBLE,
	JP_STR,
	JP_NULL
};

/* Internal use only */
enum ParserState {
	PARSE_NULL_STATE,
	PARSE_INIT,
	PARSE_OBJECT_OPEN_CURLY_BRACE,
	PARSE_ARRAY_OPEN_BRACKET,
	PARSE_ARRAY_OPEN_BRACKET_AGAIN,
	PARSE_OBJECT_COMMA,
	PARSE_KEY_ID,
	PARSE_KEY_ID_ESCAPE_CHAR,
	PARSE_KEY_ID_UNICODE_0,
	PARSE_KEY_ID_UNICODE_1,
	PARSE_KEY_ID_UNICODE_2,
	PARSE_KEY_ID_UNICODE_3,
	PARSE_STRING,
	PARSE_STRING_ESCAPE_CHAR,
	PARSE_STRING_UNICODE_0,
	PARSE_STRING_UNICODE_1,
	PARSE_STRING_UNICODE_2,
	PARSE_STRING_UNICODE_3,
	PARSE_KEY_ID_FINISH,
	BEGIN_VALUE_PARSE,
	PARSE_NUMBER_INTEGRAL,
	PARSE_NUMBER_INTEGRAL_ZERO,
	PARSE_NUMBER_NEG,
	PARSE_NUMBER_FRACTIONAL_TENTHS_PLACE,
	PARSE_NUMBER_FRACTIONAL,
	PARSE_BOOL_T,
	PARSE_BOOL_TR,
	PARSE_BOOL_TRU,
	PARSE_BOOL_F,
	PARSE_BOOL_FA,
	PARSE_BOOL_FAL,
	PARSE_BOOL_FALS,
	// PARSE_OBJECT_CLOSE_CURLY_BRACE and PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN
	// alternates between each other if there are more than two subsequent close curly braces
	PARSE_OBJECT_CLOSE_CURLY_BRACE, 
	PARSE_OBJECT_CLOSE_CURLY_BRACE_AGAIN,
	// PARSE_ARRAY_CLOSE_BRACKET and PARSE_ARRAY_CLOSE_BRACKET_AGAIN
	// alternates between each other if there are more than two subsequent close brackets
	PARSE_ARRAY_CLOSE_BRACKET,
	PARSE_ARRAY_CLOSE_BRACKET_AGAIN,
	PARSE_NULL_N,
	PARSE_NULL_NU,
	PARSE_NULL_NUL,
	PARSE_VALUE_FINISH,
	PARSE_END_OF_FILE, // EOF and error enum values must be > 1
	PARSE_ERROR_UNEXPECTED_CHAR
};

/* Internal use only */
struct Parser {
	enum ParserState state;
	
	//char *keyIdBuffer;
	//size_t keyIdBufferIdx;
	//size_t keyIdBufferSize;
	//bool isParsingEscapeChar;
	
	FILE *jsonFile;
	
	unsigned char *byteArr;
	size_t byteArrIdx;
	size_t byteArrMaxItems;
	
	size_t lineNum, colNum;
	void **addrArr;
	size_t addrArrIdx;
	size_t addrArrMaxItems;
	struct ArrayStack modeStack;
	struct ArrayStack addrStack;
	struct ArrayStack memberCountStack;
	bool isParsingArray;
	
	// Members modifiable directly (keep small, max 3 to 4, to manage complexity):
	
	// Turned off when the parser is in the state BEGIN_VALUE_PARSE or in IsJustStartedParsingFirstValue
	// Accessed by ParserInterface
	bool iterateOnly;
};

/* Internal use only */
enum ParserInterfaceState {
	PARSER_INTERFACE_INITIALIZED,
	PARSER_INTERFACE_HAS_NEXT_TRUE,
	PARSER_INTERFACE_HAS_NEXT_FALSE,
	PARSER_INTERFACE_NEXT_IS_OBJ,
	PARSER_INTERFACE_NEXT_IS_ARR,
	PARSER_INTERFACE_NEXT_IS_BOOL,
	PARSER_INTERFACE_NEXT_IS_S_LONG,
	PARSER_INTERFACE_NEXT_IS_LONG_DOUBLE,
	PARSER_INTERFACE_NEXT_IS_STR,
	PARSER_INTERFACE_NEXT_IS_NULL,
	PARSER_INTERFACE_OBJ_EXPANDED,
	PARSER_INTERFACE_ARR_EXPANDED,
	PARSER_INTERFACE_COLLAPSED,
	PARSER_INTERFACE_END_OF_FILE,
	PARSER_INTERFACE_ERROR
};

/*
   Main struct used for interacting with the parser.
   To use the JSON parser, create a variable of this struct type and
   call JsonParser_Init.
*/
struct ParserInterface {
	enum ParserInterfaceState _state;
	struct Parser _parser;
	
	// Members modifiable directly (keep small, max 3 to 4, to manage complexity)
	bool skip; // Only used in _JsonParser_SkipObjArr
};


/*
   Always needs to be called prior to using the parser. Takes in the [filePath] as an argument and
   attempts to open it.

*/
int JsonParser_Init(struct ParserInterface *parserInterface, char *filePath); 


/*
   Always needs to be called after finishing using the parser to free allocated memory.

*/
void JsonParser_Free(struct ParserInterface *parserInterface);


/*
   Return value: The bool value if errCode == 0

   Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_BOOL as the return value.

*/
bool JsonParser_GetBoolValue(struct ParserInterface *interface, int * const errCode);


/*
   Return value: The long value if errCode == 0

   Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_S_LONG as the return value.

*/
long JsonParser_GetSLongValue(struct ParserInterface *interface, int * const errCode);


/*
   Return value: The long double value if errCode == 0

   Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_LONG_DOUBLE as the return value.

*/
long double JsonParser_GetLongDoubleValue(struct ParserInterface *interface, int * const errCode); 


/*
   Return value: Pointer to null-terminated string if errCode == 0
	If the string is empty, points to a null terminator.

   Error codes:
	- EXIT_FAILURE: The last call to the interface was not a call to JsonParser_GetNextType
					that returned without an error and had JP_STR as the return value.

*/
char* JsonParser_GetStringValue(struct ParserInterface *interface, int * const errCode); 


/*
   Return value: A pointer to the start of the key value string.
	If the string is empty, points to a null terminator.

   Error codes:
	- EXIT_FAILURE: The current position of the parser may be in an array, or
		it is in an object but the last call to the interface was not a call to JsonParser_GetNextType
		that returned without an error.

*/
const char * JsonParser_GetKeyValue(struct ParserInterface *interface, int * const errCode);


/*
   When JsonParser_HasNext returns false, this function should be called to move the parser
   forward. After calling JsonParser_Collapse, JsonParser_HasNext should be called.

   Return values:
	0: Object/array collapsed successfully.
	All other values: Error
*/
int JsonParser_Collapse(struct ParserInterface *interface); 


/*
   When JsonParser_GetNextType returns either a JP_OBJ or a JP_ARR, this function should be called to
   move the parser forward. After calling JsonParser_Expand, JsonParser_HasNext should be called.

   Return values:
	0: Object/array expanded successfully.
	All other values: Error
*/
int JsonParser_Expand(struct ParserInterface *interface); 


/*
   Return values:
   	JP_OBJ: Object
	JP_ARR: Array
	JP_BOOL: Boolean
	JP_S_LONG: Signed long
	JP_LONG_DOUBLE: Signed floating number
	JP_STR: String
	JP_NULL: NULL value
*/
enum ParserDataType JsonParser_GetNextType(struct ParserInterface *interface, int * const errCode); 


/*
   Return values:
   	JP_OBJ: Object
	JP_ARR: Array
	JP_BOOL: Boolean
	JP_S_LONG: Signed long
	JP_LONG_DOUBLE: Signed floating number
	JP_STR: String
	JP_NULL: NULL value
*/
enum ParserDataType JsonParser_GetCurrentType(struct ParserInterface *interface, int * const errCode); 


/*
   Return values:
	true: The parser sees that a potential object is available to be parsed, although at this point
			cannot guarantee that there will be no parsing errors during the actual parsing of the
			object made with the next available command, JsonParser_GetNextType.
	false: There are no additional objects available within this object/array.
*/
bool JsonParser_HasNext(struct ParserInterface *interface, int * const errCode); 


/*
   Moves the parser to the location specified by [path], if it exists.
   Once navigation is successful, the type of the object at path can be
   retrieved via JsonParser_GetCurrentType, and the value can be retrieved by
   the relevant JsonParser_Get[TYPE]Value function. 

   Example paths:
	x.y
	x[0]
	[2]
	obj
	["obj"]
	["x"].x[2][1].x.y.b[5]
	["objWithPeriodsInKey...."].x
	
	Single quotes currently unsupported (ex. ['obj'])

   Note: If sending the value of the path from a shell (such as when using the "goto" executable in the repo)
   	 quotes in the path need to be escaped. Ex: ./goto "sample.json" [\"students\"][23][\"first_name\"]
   
*/
bool JsonParser_GoTo(struct ParserInterface *interface, const char * const path, int *errCode); 


/*
   Debug function for printing the value the object last parsed by the parser.
   Normal usage should use JsonParser_GetDataType followed by JsonParser_Get[TYPE]Value.

*/
int JsonParser_Debug_PrintCurrent(struct ParserInterface *parserInterface);


/*
   Debug function for iterating through the entire JSON and printing all members.

*/
void JsonParser_Debug_PrintAll(struct ParserInterface *parserInterface, int *errCode);



