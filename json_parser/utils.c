// TODO: [] strcpy replacement that guarantees dest array is long enough

#include "utils.h"


const void *VOID_PTR = NULL;

//const size_t MAX_STACK_SIZE = 10 * sizeof VOID_PTR;
const size_t MAX_STACK_SIZE = SIZE_MAX / sizeof VOID_PTR - sizeof VOID_PTR;

const unsigned short APPROX_STACK_CHUNK_SIZE = 8;


int CheckAssumptions() {
	// #1 Assume all data pointers and void pointer are the same size
	bool *boolPtr = NULL;
	char *charPtr = NULL;
	signed char *signedCharPtr = NULL;
	unsigned char *unsignedCharPtr = NULL;
	short *shortPtr = NULL;
	unsigned short *unsignedShortPtr = NULL;
	int *intPtr = NULL;
	unsigned *unsignedPtr = NULL;
	long *longPtr = NULL;
	unsigned long *unsignedLongPtr = NULL;
	long long *longLongPtr = NULL;
	unsigned long long *unsignedLongLongPtr = NULL;
	float *floatPtr = NULL;
	double *doublePtr = NULL;
	long double *longDoublePtr = NULL;
	
	char voidPtrSize = sizeof(VOID_PTR);
	if (sizeof(boolPtr) != voidPtrSize || sizeof(charPtr) != voidPtrSize || sizeof(signedCharPtr) != voidPtrSize || \
		sizeof(unsignedCharPtr) != voidPtrSize || sizeof(shortPtr) != voidPtrSize || sizeof(unsignedShortPtr) != voidPtrSize || \
		sizeof(intPtr) != voidPtrSize || sizeof(unsignedPtr) != voidPtrSize || sizeof(longPtr) != voidPtrSize || \
		sizeof(unsignedLongPtr) != voidPtrSize || sizeof(longLongPtr) != voidPtrSize || sizeof(unsignedLongLongPtr) != voidPtrSize || \
		sizeof(floatPtr) != voidPtrSize || sizeof(doublePtr) != voidPtrSize || sizeof(longDoublePtr) != voidPtrSize) {
			UtilsError("utils.checkAssumptions - Pointer size mismatch");
			return EXIT_FAILURE;
	}
	
	// #2 Assume 64-bit addresses
	// Dependents:
	// - utils.ArrayStack
	// - json_parser.c
	if (sizeof(VOID_PTR) != 8) {
		UtilsError("utils.checkAssumptions - Not 64-bit");
		return EXIT_FAILURE;
	}
	
	/* // #3 Assume sizeof signed long is 8
	// Dependents:
	// - json_parser.c
	if (sizeof(signed long) != 8) {
		UtilsError("utils.checkAssumptions - signed long size is not 8");
		return EXIT_FAILURE;
	} */
	
	// #4 Assume bool size is 1
	// Dependents:
	// - json_parser.c
	if (sizeof(bool) != 1) {
		UtilsError("utils.checkAssumptions - mool size is not 1");
		return EXIT_FAILURE;
	}
	
	return 0;
}


void UtilsError(const char *msg) {
	fprintf(stderr, "%s\nerrnn:\n", msg);
	perror(NULL);
	fprintf(stderr, "Stacktrace:\n");
	
	void *stacks[USHRT_MAX];
	int numAddrs = backtrace(stacks, USHRT_MAX);
	char **symbols = backtrace_symbols(stacks, numAddrs);
	if (!symbols) {
		fprintf(stderr, "UtilsError - backtrace_symbols failed\n");
		return;
	}
	
	if (numAddrs == USHRT_MAX) {
		fprintf(stderr, "Note: # of addresses exceeded buffer\n");
	}
	
	for (int i = 0; i < numAddrs; ++i) {
		fprintf(stderr, "%s\n", symbols[i]);
	}
	
	if (LOG_LEVEL >= 7) {
		assert(2 + 2 == 5); // Trigger error
	}
	
	free(symbols);
	return;	
}


bool CheckNotNull(const void *ptr, const char *contextMsg) {
	if (ptr != NULL) {
		return false;
	}
	
	UtilsError(contextMsg);
	return true;
}


bool CheckNotZero(int val, const char *contextMsg) {
	if (val == 0) {
		return false;
	}
	
	UtilsError(contextMsg);
	return true;
}


float Square(const float x) {
	return x * x;
}


unsigned long long UnsafePow(const unsigned int x, const unsigned int y) {
	if (y == 0) return 1;
	
	unsigned long long product = 1;
	for (int i = 0; i < y; ++i) {
		product *= x;
	}
	
	return product;
}


// String functions


size_t strlcat_s(char * const dest, size_t const numElem, size_t const dest_count, char const * const src,
			 size_t const src_count) {
	// Preconditions
	assert(dest != NULL);
	assert(dest[dest_count] == '\0');
	assert(src != NULL);
	assert(src[src_count] == '\0');
	assert(numElem >= dest_count + src_count + 1);

	if (numElem == 1) return 0;
	
	size_t dIdx = dest_count;
	for (size_t sIdx = 0; sIdx < src_count; ++sIdx) {
		dest[dIdx] = src[sIdx];
		++dIdx;
	}
	dest[dIdx] = '\0';
	
	return dest_count + src_count;
}


size_t strncpy_s(char * const dest, size_t const numElem, char const * const src,
			 size_t const n) {
	// Preconditions
	assert(dest != NULL);
	assert(src != NULL);
	assert(numElem >= n + 1);
	
	size_t num_read = 0;
	for (int i = 0; i < n; ++i) {
		char c = src[i];
		if (c == '\0') break;
		
		dest[i] = src[i];
		++num_read;
	}
	dest[num_read] = '\0';
	
	return num_read;
}


char utf8str_iterate(char const * const str, size_t const remainingNumElem) {
	// Preconditions
	assert(str != NULL);
	assert(remainingNumElem > 0);
	if (remainingNumElem == 1) assert(*str == '\0');
	
	char ret = -1;
	unsigned char const c = (unsigned char) *str;
	if (c < 0x80) {
		ret = 1;
	} else if (c < 0xe0 && remainingNumElem >= 2) {
		ret = 2;
	} else if (c < 0xf0 && remainingNumElem >= 3) {
		ret = 3;
	} else if (c < 0xf8 && remainingNumElem >= 4) {
		ret = 4;
	}
	
	return ret;
}


size_t utf8str_len(char const * const str, size_t const numElem, int * const errCode) {
	// Preconditions
	assert(str != NULL);
	
	*errCode = 0;
	size_t count = 0;
	
	size_t idx = 0;
	unsigned char c = (unsigned char) *str;
	while (c != '\0' && idx < numElem - 1) {
		if (c < 0x80) {
			++idx;
			++count;
		} else if (c < 0xe0) {
			idx += 2;
			++count;
		} else if (c < 0xf0) {
			idx += 3;
			++count;
		} else if (c < 0xf8) {
			idx += 4;
			++count;
		}
		c = str[idx];
	}
	
	if (idx > numElem - 1) *errCode = -1; // Not a null-terminated string

	return count;
}


// End string functions


void* realloc_if_needed(void *ptr, size_t * const curr_size, size_t const * const curr_len, int const block_size) {
	// Preconditions
	// errno
	//assert(errno == 0); // TODO: What is setting errno in sqlite_terminal.c w/o an error
	// ptr
	assert(ptr != NULL);
	// curr_size
	assert(curr_size != NULL);
	assert(*curr_size != 0);
	// curr_len
	assert(curr_len != NULL);
	assert(*curr_len <= *curr_size - 1);
	// block_size
	assert(block_size > 0);

	void *ret_val = NULL;

	if (*curr_len == *curr_size - 1) {
		if (SIZE_MAX - *curr_size < block_size) {
			errno = ERANGE;
		} else {
			*curr_size = (*curr_size + block_size) - ((*curr_size + block_size) % block_size);
			ret_val = realloc(ptr, *curr_size);
		}
	} else {
		ret_val = ptr;
	}
	
	// Postconditions
	assert(*curr_size != 0);
	
	return ret_val;
}


// Stack implementation functions


int ArrayStack_Init(struct ArrayStack *stack, const size_t elemSize) {
	stack->_stackChunkSize = (APPROX_STACK_CHUNK_SIZE / elemSize) * elemSize;
	stack->_stack = malloc(stack->_stackChunkSize);
	if (CheckNotNull(stack->_stack, "ArrayStack_Init - stack malloc")) {
		return EXIT_FAILURE;
	}
	
	stack->_elemSize = elemSize;
	stack->_top = 0;
	stack->_maxItems = stack->_stackChunkSize / stack->_elemSize;
	stack->_maxStackSize = SIZE_MAX / stack->_elemSize - stack->_elemSize;
	
	return 0;
}


int ArrayStack_Push(struct ArrayStack *stack, const void *srcAddr) {
	if (stack->_top == stack->_maxItems) {
		if (stack->_maxItems * stack->_elemSize == stack->_maxStackSize) {
			UtilsError("ArrayStack_Push - Max stack size reached");
			return EXIT_FAILURE;
		} else { // Increase stack size
			size_t newSize = stack->_maxItems * stack->_elemSize + stack->_stackChunkSize;
			newSize = newSize > stack->_maxStackSize ? stack->_maxStackSize : newSize;
			stack->_stack = realloc(stack->_stack, newSize);
			if (CheckNotNull(stack->_stack, "ArrayStack_Push - stack realloc")) {
				return EXIT_FAILURE;
			}
			
			stack->_maxItems = newSize / stack->_elemSize;
		}
	}
	
	void *destAddr = stack->_stack + (stack->_top * stack->_elemSize);
	memcpy(destAddr, srcAddr, stack->_elemSize); // TODO: Check that memory doesn't overlap?
	++stack->_top;

	return 0;
}


int ArrayStack_Peek(const struct ArrayStack *stack, void *destAddr) {
	if (stack->_top == 0) {
		UtilsError("ArrayStack_Pop - Empty stack");
		return EXIT_FAILURE;
	}
	
	void *srcAddr = stack->_stack + (stack->_top - 1 * stack->_elemSize);
	memcpy(destAddr, srcAddr, stack->_elemSize); // TODO: Check that memory doesn't overlap?
	
	return 0;
}


int ArrayStack_Pop(struct ArrayStack *stack, void *destAddr) {
	if (stack->_top == 0) {
		UtilsError("ArrayStack_Pop - Empty stack");
		return EXIT_FAILURE;
	}
	
	--stack->_top;
	void *srcAddr = stack->_stack + (stack->_top * stack->_elemSize);
	memcpy(destAddr, srcAddr, stack->_elemSize); // TODO: Check that memory doesn't overlap?
	
	// Check if stack should be shrinked
	if (stack->_top != 0 && stack->_top * stack->_elemSize == stack->_maxItems * stack->_elemSize - stack->_stackChunkSize) {
		size_t newSize = stack->_maxItems * stack->_elemSize - stack->_stackChunkSize;
		stack->_stack = realloc(stack->_stack, newSize);
		if (CheckNotNull(stack->_stack, "ArrayStack_Pop - stack realloc")) {
			return EXIT_FAILURE;
		}
		
		stack->_maxItems = newSize / stack->_elemSize;
	}
	
	return 0;
}


bool ArrayStack_IsEmpty(const struct ArrayStack *stack) {
	return stack->_top == 0;
}


void ArrayStack_Free(struct ArrayStack *stack) {
	free(stack->_stack);
	stack->_stack = NULL;	
}


size_t ArrayStack_Count(struct ArrayStack *stack) {
	return stack->_top;
}


void ArrayStack_Debug_Print(const struct ArrayStack * const stack) {
	short maxWidth = 16;
	short currWidth = 0;
	
	for (int i = 0; i < stack->_top * stack->_elemSize; ++i) {
		if (currWidth == maxWidth) {
			printf("\n");
			currWidth = 0;
		}
		
		printf("%X ", *((char *) (stack->_stack) + i));
		++currWidth;
	}
}

// Stack implementation functions end

// ArrayList functions
int ArrayList_Init(struct ArrayList * const list, const size_t elemSize, const int approxChunkSize) {
	if (approxChunkSize < elemSize) {
		UtilsError("ArrayList_Init - approxChunkSize smaller than elemSize");
		return EXIT_FAILURE;
	}

	list->_elemSize = elemSize;
	list->_listChunkItemCount = (approxChunkSize / list->_elemSize);
	list->_list = malloc(list->_listChunkItemCount * list->_elemSize);
	if (CheckNotNull(list->_list, "ArrayList_Init - list malloc")) {
		return EXIT_FAILURE;
	}
	
	list->_length = 0;
	list->_maxItems = list->_listChunkItemCount;
	list->_maxListItems = ((SIZE_MAX / (list->_listChunkItemCount * list->_elemSize)) - 12) * list->_listChunkItemCount;
	
	return 0;
}


int ArrayList_Add(struct ArrayList * const list, const void *srcAddr) {
	if (list->_length == list->_maxItems) {
		if (list->_maxItems >= list->_maxListItems) {
			UtilsError("ArrayList_Add - Max list size reached");
			return EXIT_FAILURE;
		} else { // Increase list size
			size_t newSize = (list->_maxItems + list->_listChunkItemCount) * list->_elemSize;
			list->_list = realloc(list->_list, newSize);
			if (CheckNotNull(list->_list, "ArrayList_Add - list realloc")) {
				return EXIT_FAILURE;
			}
			
			list->_maxItems = newSize / list->_elemSize;
		}
	}
	
	void *destAddr = list->_list + (list->_length * list->_elemSize);
	memcpy(destAddr, srcAddr, list->_elemSize); // TODO: Check that memory doesn't overlap?
	++list->_length;

	return 0;
}


int ArrayList_GetCopy(const struct ArrayList * const list, const size_t idx, void *destAddr) {
	if (idx > list->_maxItems - 1) {
		UtilsError("ArrayList_GetCopy - Index out of bounds");
		return EXIT_FAILURE;
	}

	void *srcAddr = list->_list + (idx * list->_elemSize);
	memcpy(destAddr, srcAddr, list->_elemSize); // TODO: Check that memory doesn't overlap?
	
	return 0;
}


void* ArrayList_GetRef(const struct ArrayList * const list, const size_t idx, int * const errCode) {
	assert(*errCode == 0);
	if (idx > list->_maxItems - 1) {
		UtilsError("ArrayList_GetRef - Index out of bounds");
		*errCode = EXIT_FAILURE;
		return NULL;
	}

	void *srcAddr = list->_list + (idx * list->_elemSize);
	return srcAddr; 
}


int ArrayList_Delete(struct ArrayList * const list, const size_t idx) {
	if ((list->_length == 0) || (idx > list->_length - 1)) {
		UtilsError("ArrayList_Delete - Index out of bounds");
		return EXIT_FAILURE;
	}

	if (idx == list->_length - 1) {
		;
	} else if (idx == 0) {
		memmove(list->_list, list->_list + list->_elemSize, (list->_length - 1) * list->_elemSize);
	} else {
		memmove(list->_list + (idx * list->_elemSize),
			list->_list + ((idx + 1) * list->_elemSize),
			(list->_length - idx - 1) * list->_elemSize);	
	}
	--list->_length;

	// Check if list should be shrinked
	if (list->_length < list->_maxItems - list->_listChunkItemCount) {
		size_t newSize = (list->_maxItems - list->_listChunkItemCount) * list->_elemSize;
		list->_list = realloc(list->_list, newSize);
		if (CheckNotNull(list->_list, "ArrayStack_Delete - list realloc"))
			return EXIT_FAILURE;

		list->_maxItems = newSize / list->_elemSize;
	}
	
	return 0;
}


void ArrayList_Free(struct ArrayList * const list) {
	free(list->_list);
	list->_list = NULL;	
}


size_t ArrayList_Length(struct ArrayList * const list) {
	return list->_length;
}


void ArrayList_Debug_Print(const struct ArrayList * const list) {
	short maxWidth = 16;
	short currWidth = 0;

	printf("_list: %p\n_elemSize: %lu\n_maxItems: %lu\n_length: %lu\n_maxListItems: %lu\n_listChunkItemCount: %lu\n",
		list->_list, list->_elemSize, list->_maxItems, list->_length, list->_maxListItems, list->_listChunkItemCount);

	for (int i = 0; i < list->_maxItems * list->_elemSize; ++i) {
		if (currWidth == maxWidth) {
			printf("\n");
			currWidth = 0;
		}
		
		printf("%X ", *((char *) (list->_list) + i));
		++currWidth;
	}
	printf("\n");
}

// ArrayList functions end
