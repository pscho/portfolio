// TODO: [] strcpy replacement that guarantees dest array is long enough

#include "utils.h"


const void *VOID_PTR = NULL;

//const size_t MAX_STACK_SIZE = 10 * sizeof VOID_PTR;
const size_t MAX_STACK_SIZE = SIZE_MAX / sizeof VOID_PTR - sizeof VOID_PTR;

const unsigned short APPROX_STACK_CHUNK_SIZE = 8;


int CheckAssumptions() {
	int ret = 0;
	
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
		UtilsError("utils.checkAssumptions - bool size is not 1");
		return EXIT_FAILURE;
	}
	
	return 0;
}


void UtilsError(const char *msg) {
	fprintf(stderr, "%s\nerrno:\n", msg);
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


void* realloc_if_needed(void *ptr, size_t * const curr_size, const size_t * const curr_len, const int block_size) {
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



