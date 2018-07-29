/*
 * test.c
 *
 *  Created on: 27 июл. 2018 г.
 *      Author: andrey
 */

#include "assert.h"
#include "stdio.h"
#include "shash.h"

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

void print(SHTAB *shtab, const char* comment)
{
	WorkItem	*res;

	printf("START PRINT %s\n", comment);
	for (SHASH_SeqReset(shtab); (res = (WorkItem *) SHASH_SeqNext(shtab)) != NULL; )
		printf("--> [%lu] %d %d\n", shtab->SeqScanCurElem-1, res->blkno, res->hits);
	printf("END PRINT\n");
}
void print_full_state(SHTAB *shtab, const char* comment)
{
	WorkItem	*res;
	uint64		i;

	printf("START PRINT %s. ElementsMaxNum=%lu tableSize=%lu factor=%3.1f Elems=%lu\n", comment, shtab->Header.ElementsMaxNum, shtab->HTableSize, shtab->Header.FillFactor, SHASH_Entries(shtab));
	for (i=0; i<shtab->HTableSize; i++)
	{
		uint64 pos = i*shtab->Header.ElementSize;
		WorkItem *item = (WorkItem *) &shtab->Elements[pos];
		printf("--> [%lu] blk=%d, hits=%d state=%d\n", i, item->blkno, item->hits, shtab->state[i]);
	}
	printf("END PRINT\n");
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

	shctl.FillFactor = 0.5;
	shctl.ElementsMaxNum = MAX_NUM;
	shctl.ElementSize = sizeof(WorkItem);
	shctl.KeySize = sizeof(int);
	shctl.HashFunc = DefaultHashValueFunc;
	shctl.CompFunc = DefaultCompareFunc;
	item.blkno = 5;
	item.hits = 0;

	test = SHASH_Create(shctl);
	print(test, "F1");
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, SHASH_ENTER, &found);
	res->hits = 11;
	assert(!found  && (res != NULL));
	print(test, "F2");
	assert((res->blkno == 5) && (res->hits == 11));
	item.blkno = 101;
	item.hits = 0;
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, SHASH_ENTER, &found);
	res->hits = 1;
	assert(!found  && (res != NULL));
	assert((res->blkno == 101) && (res->hits == 1));
	print(test, "F3");
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, SHASH_FIND, &found);
	assert(found);
	assert(res != NULL);
	assert((res->blkno == 101) && (res->hits == 1));
	res->hits++;

	blkno = 4;
	res = (WorkItem *) SHASH_Search(test, (void *)&blkno, SHASH_FIND, &found);
	assert(!found);
	assert(res == NULL);
	print(test, "F4");
	res = (WorkItem *) SHASH_Search(test, (void *)&item.blkno, SHASH_FIND, &found);
	assert(found);
	assert((res->blkno == 101) && (res->hits == 2));
	res->hits++;

	printf("REMOVING TEST\n");
	SHASH_Clean(test);
	blkno=0; hits=0;
	printf("Add\n");
	while ( (res = SHASH_Search(test, (void *)&blkno, SHASH_ENTER, &found)) != NULL )
	{
		res->hits = hits++;
		blkno++;
	}
	printf("Number of entries: %lu/%lu\n", SHASH_Entries(test), test->Header.ElementsMaxNum);
	blkno--;

	printf("REMOVE\n");
	while ( (res = SHASH_Search(test, (void *)&blkno, SHASH_REMOVE, &found)) != NULL )
	{
		printf("REMOVE blkno=%d\n", blkno);
		assert(found);
		blkno--;
	}
	blkno = 111;
	assert(SHASH_Entries(test) == 0);
	print_full_state(test, "FULL PRINT AFTER DEL");
	res = SHASH_Search(test, (void *)&blkno, SHASH_ENTER, &found);
	assert(res != NULL);
	assert(!found);
	print_full_state(test, "FULL PRINT-2.1");
	res = SHASH_Search(test, (void *)&blkno, SHASH_ENTER, &found);
	assert(res != NULL);
	assert(found);
	printf("ELEMS=%lu\n", SHASH_Entries(test));
	assert(SHASH_Entries(test) == 1);
	print_full_state(test, "FULL PRINT-2");
	printf("Number of entries: %lu/%lu\n", SHASH_Entries(test), test->Header.ElementsMaxNum);

	blkno = 1000;
	hits = 0;
	while ((res = SHASH_Search(test, (void *)&blkno, SHASH_ENTER, &found)) != NULL)
	{
		assert(!found);
		blkno++;
		res->hits = ++hits;
//		printf("-> elems=%lu\n", test->nElements);
	}
	print_full_state(test, "FULL PRINT-3");

	item.blkno = 1005;
	res = SHASH_Search(test, (void *)&item.blkno, SHASH_ENTER, &found);
	assert(found);
	assert(res->hits == 6);

	SHASH_Destroy(test);
	printf("TEST PASSED\n");
	return 0;
}


