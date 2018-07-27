/*
 * shash.h
 *
 *  Created on: 27 июл. 2018 г.
 *      Author: andrey
 */

#ifndef SHASH_H_
#define SHASH_H_

#include "stdbool.h"

/* hash_search operations */
typedef enum
{
	HASH_FIND,
	HASH_ENTER,
} HASHACTION;

typedef	unsigned long int uint64;
typedef uint64 (*HashValueFunc) (void *key, uint64 size, uint64 base);
typedef bool (*CompareFunc) (void* bucket1, void* bucket2);


typedef struct SHTABCTL
{
	uint64			ElementSize;
	uint64			KeySize;
	uint64			ElementsMaxNum;
	float			FillFactor;
	HashValueFunc	HashFunc;
	CompareFunc		CompFunc;
} SHTABCTL;

typedef struct SHTAB
{
	SHTABCTL		Header;
	char			*Elements;
	uint64			nElements;
	uint64			*lineptr; /* List of pointers to assigned hash table entries */
	bool			*used;
	uint64			SeqScanCurElem;
} SHTAB;

extern SHTAB* SHASH_Create(SHTABCTL shctl);
extern void SHASH_Clean(SHTAB* shtab);
extern void SHASH_Destroy(SHTAB* shtab);
extern uint64 SHASH_Entries(SHTAB* shtab);
extern void SHASH_SeqReset(SHTAB* shtab);
extern void* SHASH_SeqNext(SHTAB* shtab);
extern void* SHASH_Search(SHTAB* shtab, void *keyPtr, HASHACTION action, bool *foundPtr);

#endif /* SHASH_H_ */
