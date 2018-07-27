#include "assert.h"
#include "math.h"
#include "shash.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "stdio.h"

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

void check(SHTAB* shtab)
{
	assert(shtab != NULL);
	assert(shtab->Elements != NULL);
	assert(shtab->lineptr != NULL);
	assert(shtab->used != NULL);
}

SHTAB*
SHASH_Create(SHTABCTL shctl)
{
	SHTAB*	shtab = (SHTAB *) calloc(1, sizeof(SHTAB));

	assert(shctl.ElementSize != 0);
	assert(shctl.ElementSize >= shctl.KeySize);

	shtab->Header = shctl;
	/* Add one element as sign of empty value */
	shtab->Header.ElementsMaxNum++;
	shtab->Elements = (char *) calloc(shtab->Header.ElementsMaxNum, shctl.ElementSize);
	shtab->lineptr = (uint64 *) calloc(shtab->Header.ElementsMaxNum, sizeof(uint64));
	shtab->used = (bool *) calloc(shtab->Header.ElementsMaxNum, sizeof(bool));
	shtab->nElements = 0;
	shtab->SeqScanCurElem = 0;

	return shtab;
}

void
SHASH_Clean(SHTAB* shtab)
{
	check(shtab);
	/*
	 * We not free memory: only clean 'used' field and set number of elems at 0
	 */
	memset(shtab->used, false, shtab->Header.ElementsMaxNum);
	shtab->nElements = 0;
}

void
SHASH_Destroy(SHTAB* shtab)
{
	check(shtab);

	free(shtab->Elements);
	free(shtab->lineptr);
	free(shtab->used);
	free(shtab);
}

uint64
SHASH_Entries(SHTAB* shtab)
{
	return shtab->nElements;
}

void
SHASH_SeqReset(SHTAB* shtab)
{
	shtab->SeqScanCurElem = 0;
}

void*
SHASH_SeqNext(SHTAB* shtab)
{
	check(shtab);
	if (SHASH_Entries(shtab) == shtab->SeqScanCurElem)
		return NULL;
	else
		return &shtab->Elements[shtab->lineptr[shtab->SeqScanCurElem++]*shtab->Header.ElementSize];
}

void*
SHASH_Search(SHTAB* shtab, void *keyPtr, HASHACTION action, bool *foundPtr)
{
	uint64	index;

	check(shtab);
	assert(shtab->nElements <= shtab->Header.ElementsMaxNum);

	index = shtab->Header.HashFunc(keyPtr, shtab->Header.KeySize, shtab->Header.ElementsMaxNum);
	assert(index < shtab->Header.ElementsMaxNum);

	for(;;)
	{
		void	*result;
		uint64	pos = index*shtab->Header.ElementSize;

		if (!shtab->used[index])
		{
			if (foundPtr != NULL)
				*foundPtr = false;

			/* Empty position found */
			switch (action)
			{
			case HASH_FIND:
				result = NULL;
				break;
			case HASH_ENTER:
				if ((float)shtab->nElements/(float)(shtab->Header.ElementsMaxNum-1) >= shtab->Header.FillFactor)
					result = NULL;
				else
				{
					memset(&shtab->Elements[pos], 0, shtab->Header.ElementSize);
					memcpy(&(shtab->Elements[pos]), keyPtr, shtab->Header.KeySize);
					shtab->used[index] = true;
					shtab->lineptr[shtab->nElements++] = index;
					result = &(shtab->Elements[pos]);
				}
				break;
			default:
				assert(0);
			}

			return result;
		}

		/* Element is used */
		if (shtab->Header.CompFunc(keyPtr, &shtab->Elements[pos]))
		{
			if (foundPtr != NULL)
				*foundPtr = true;

			return &(shtab->Elements[pos]);
		}

		/* Go to next element */
		index = (index+1)%(shtab->Header.ElementsMaxNum);
	}

	assert(0);
	return NULL;
}

typedef struct {
	int	blkno;
	int hits;
} WorkItem;

#define MAX_NUM	(10)

uint64
DefaultHashValueFunc(void *key, uint64 size, uint64 base)
{
	char	byte;
	uint64	sum = 0;
	int i;

	for (i = 0; i < size; i++)
	{
		uint64 x = ((char *)key)[i];
		sum += x*x*x*x + x*x*x + x*x + x + 1;
	}

	return sum%base;
}

bool
DefaultCompareFunc(void* bucket1, void* bucket2)
{
	WorkItem	*item1 = (WorkItem *) bucket1;
	WorkItem	*item2 = (WorkItem *) bucket2;

	if (item1->blkno == item2->blkno)
		return true;
	else
		return false;
}

int main()
{
	SHTABCTL	shctl;
	SHTAB		*test;
	bool		found;
	WorkItem	item;
	WorkItem	*res;
	int blkno;
	int hits;

	shctl.FillFactor = 1;
	shctl.ElementsMaxNum = MAX_NUM;
	shctl.ElementSize = sizeof(WorkItem);
	shctl.KeySize = sizeof(int);
	shctl.HashFunc = DefaultHashValueFunc;
	shctl.CompFunc = DefaultCompareFunc;
	item.blkno = 5;
	item.hits = 0;

	test = SHASH_Create(shctl);
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, HASH_ENTER, &found);
	assert(!found  && (res != NULL));
	assert((res->blkno == 5) && (res->hits == 0));

	item.blkno = 101;
	item.hits = 0;
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, HASH_ENTER, &found);
	assert(!found  && (res != NULL));
	assert((res->blkno == 101) && (res->hits == 0));

	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, HASH_FIND, &found);
	assert((found) && (res != NULL));
	assert((res->blkno == 101) && (res->hits == 0));
	res->hits++;

	blkno = 4;
	res = (WorkItem *) SHASH_Search(test, (void *)&blkno, HASH_FIND, &found);
	assert(!found);
	assert(res == NULL);
	for (blkno=0; blkno<test->Header.ElementsMaxNum; blkno++)
	{
		WorkItem *a = (WorkItem *) &test->Elements[blkno*test->Header.ElementSize];
		printf("[%d] DATA: blk=%d hits=%d used=%d el=%lu\n", blkno, a->blkno, a->hits, test->used[blkno], test->nElements);
	}
//	for (SHASH_SeqReset(test); (res = (WorkItem *) SHASH_SeqNext(test)) != NULL; )
//		printf("--> State: %d %d\n", res->blkno, res->hits);

	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, HASH_FIND, &found);
	assert(found);
	assert((res->blkno == 101) && (res->hits == 1));
	res->hits++;

	SHASH_Clean(test);

	blkno = 1000;
	hits = 0;
	while ((res = SHASH_Search(test, (void *)&blkno, HASH_ENTER, &found)) != NULL)
	{
		assert(!found);
		blkno++;
		res->hits = ++hits;
		printf("-> elems=%lu\n", test->nElements);
	}
	printf("FULL TABLE: %d\n", blkno);

	item.blkno = 1005;
	res = SHASH_Search(test, (void *)&item.blkno, HASH_ENTER, &found);
	assert(found);
	assert(res->hits == 6);

	for (item.blkno = 0; item.blkno < SHASH_Entries(test); item.blkno++)
	{
		int a = test->lineptr[item.blkno];
		WorkItem *b = (WorkItem *)&test->Elements[a*test->Header.ElementSize];
		printf("[%d/%d]: blkno=%d hits=%d\n", item.blkno, a, b->blkno, b->hits);
	}

	SHASH_Destroy(test);
	printf("TEST PASSED\n");
	return 0;
}
