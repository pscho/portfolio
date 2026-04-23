// [] TODO: Verify stack is initialized when calling stackFree?

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <execinfo.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef UTILS_H
#error "utils.h already defined"
#endif

#ifndef UTILS_H

#define UTILS_H

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
	((byte) & 0x80 ? '1' : '0'), \
	((byte) & 0x40 ? '1' : '0'), \
	((byte) & 0x20 ? '1' : '0'), \
	((byte) & 0x10 ? '1' : '0'), \
	((byte) & 0x08 ? '1' : '0'), \
	((byte) & 0x04 ? '1' : '0'), \
	((byte) & 0x02 ? '1' : '0'), \
	((byte) & 0x01 ? '1' : '0')


extern const void *VOID_PTR;

struct ArrayStack {
	void *_stack;
	size_t _elemSize;
	size_t _maxItems;
	size_t _top;
	size_t _maxStackSize;
	size_t _stackChunkSize;
};


struct JsonObject {
	char *_typePtr;
	bool _inArray;
};


int CheckAssumptions();

void UtilsError(const char *msg);

bool CheckNotNull(const void *ptr, const char *contextMsg);

bool CheckNotZero(int val, const char *contextMsg);

float Square(const float x);

void *realloc_if_needed(void *ptr, size_t * const curr_size, const size_t * const curr_len, const int block_size);

// Stack implementation functions

// Allocates dynamic memory (malloc) for a stack holding elements of [size] size.
int ArrayStack_Init(struct ArrayStack *stack, const size_t elemSize);

// Copies (memcpy) data of size ArrayStack._elemSize from srcAddr and adds it to the top of the stack.
int ArrayStack_Push(struct ArrayStack *stack, const void *srcAddr);

// Copies (memcpy) data of size ArrayStack._elemSize from the top of the stack to destAddr.
int ArrayStack_Peek(const struct ArrayStack *stack, void *destAddr);

// Copies (memcpy) data of size ArrayStack._elemSize from the top of the stack to destAddr and removes the element from the stack.
int ArrayStack_Pop(struct ArrayStack *stack, void *destAddr);

bool ArrayStack_IsEmpty(const struct ArrayStack *stack);

// Must be called to free memory allocated in arrayStackInit.
void ArrayStack_Free(struct ArrayStack *stack);

// JSON functions



#endif

