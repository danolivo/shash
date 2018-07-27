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


