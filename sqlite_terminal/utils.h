#ifndef UTILS_H
#define UTILS_H

// [] TODO: Comments
// [] TODO: Make ArrayList and ArrayStack functions consistent
// [] TODO: Verify stack is initialized when calling stackFree?
// [] TODO: Ideas from Jon Blow talk: https://youtu.be/TH9VCN6UkyQ?si=EzkRWO1g49Hpo6rg&t=2687
//	Follow multiple return values pattern (errcode parameter for functions)
//	Custom free() that errors when ptr is null (detect free() on freed)
//	ALWAYS set ptr to null after free
//	Dereferencing freed memory error?
//	"Macros are anti-debug"; run a code generator on macros for debug builds?
//	"no header files"
//	Refactorability:
//		. vs ->; maybe always use ->?
//		Optional types (Null checking/? types): Don't force them; maybe a preprocessor that adds not null asserts?

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <execinfo.h>
#include <string.h>
#include <errno.h>

#define NDEBUG
#include <assert.h>

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

#define CHECK(goto_label) if (ret != 0) goto goto_label;

extern const void *VOID_PTR;

extern const short LOG_LEVEL;

struct ArrayStack {
	void *_stack;
	size_t _elemSize;
	size_t _maxItems;
	size_t _top;
	size_t _maxStackSize;
	size_t _stackChunkSize;
};

struct ArrayList {
	void *_list;
	size_t _elemSize;
	size_t _maxItems;
	size_t _length;
	size_t _maxListItems;
	size_t _listChunkItemCount;
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

unsigned long long UnsafePow(const unsigned int x, const unsigned int y);

// String functions

size_t strlcat_s(char * const dest, size_t const numElem, size_t const dest_count, char const * const src,
			 size_t const src_count);

size_t strncpy_s(char * const dest, size_t const numElem, char const * const src,
			 size_t const n);

char utf8str_iterate(char const * const str, size_t const remainingNumElem);

/*
	utf8str_len: Returns the count of UTF-8 codepoints in the string starting at str.
	Assumes str is not null.
	Does not include the ending null character in the count.
	errCode is set to -1 if the string does not end with a null term, 0 otherwise.
	
	ex: utf8str_len("aՀ桁ᐰ🁀🁰", 18, &errCode) -> returns 6 // TODO: Check
*/
size_t utf8str_len(char const * const str, size_t const numElem, int * const errCode);

// End string functions

void *realloc_if_needed(void *ptr, size_t * const curr_size, size_t const * const curr_len, int const block_size);


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

size_t ArrayStack_Count(struct ArrayStack *stack);

void ArrayStack_Debug_Print(const struct ArrayStack * const stack);


// ArrayList functions

int ArrayList_Init(struct ArrayList * const list, const size_t elemSize, const int approxChunkSize);

int ArrayList_Add(struct ArrayList * const list, const void *srcAddr);

int ArrayList_GetCopy(const struct ArrayList * const list, const size_t idx, void *destAddr);

void* ArrayList_GetRef(const struct ArrayList * const list, const size_t idx, int * const errCode);

int ArrayList_Delete(struct ArrayList * const list, const size_t idx);

void ArrayList_Free(struct ArrayList * const list); 

size_t ArrayList_Length(struct ArrayList * const list); 

void ArrayList_Debug_Print(const struct ArrayList * const list); 


// JSON functions



#endif

