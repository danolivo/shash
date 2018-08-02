/*
 * shash.c
 *
 * Lightweight hash tables without dynamic expansion
 *
 *
 * Copyright (c) 1996-2018, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/hash/shash.c
 *
 *-------------------------------------------------------------------------
 */

#include "assert.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "stdio.h"

#include "shash.h"

#define pfree(x)	(free(x))
#define palloc(x)	(calloc(1, x))
#define Assert(x)	(assert(x))

static void check(SHTAB* shtab);

static void
check(SHTAB* shtab)
{
	Assert(shtab != NULL);
	Assert(shtab->Elements != NULL);
	Assert(shtab->state != NULL);
}

SHTAB*
SHASH_Create(SHTABCTL shctl)
{
	SHTAB*	shtab = (SHTAB *) palloc(sizeof(SHTAB));

	Assert(shctl.ElementSize != 0);
	Assert(shctl.ElementSize >= shctl.KeySize);

	shtab->Header = shctl;
	shtab->HTableSize = shtab->Header.ElementsMaxNum*(2. - shtab->Header.FillFactor) + 1;
	Assert(shtab->HTableSize > shtab->Header.ElementsMaxNum);

	/* Add one element as sign of empty value */
	shtab->Elements = (char *) palloc(shtab->HTableSize * shctl.ElementSize);
	shtab->state = (HESTATE *) palloc(shtab->HTableSize * sizeof(HESTATE));
	shtab->nElements = 0;
	shtab->SeqScanCurElem = 0;

	return shtab;
}

void
SHASH_Clean(SHTAB* shtab)
{
	int i;

	check(shtab);

	/*
	 * We do not free memory: only clean 'used' field and set number of elems at 0
	 */
	for (i = 0; i < shtab->HTableSize; i++)
		shtab->state[i] = SHASH_NUSED;
	shtab->nElements = 0;
	shtab->SeqScanCurElem = 0;
}

void
SHASH_Destroy(SHTAB* shtab)
{
	check(shtab);

	pfree(shtab->Elements);
	pfree(shtab->state);
	pfree(shtab);
}

uint64
SHASH_Entries(SHTAB* shtab)
{
	check(shtab);

	Assert((shtab->nElements >= 0) && (shtab->nElements <= shtab->Header.ElementsMaxNum));
	return shtab->nElements;
}

#define ELEM(index) (&shtab->Elements[index*shtab->Header.ElementSize])

void
SHASH_SeqReset(SHTAB* shtab)
{
	check(shtab);

	shtab->SeqScanCurElem = 0;
}

void*
SHASH_SeqNext(SHTAB* shtab)
{
	check(shtab);

	if (shtab->SeqScanCurElem == shtab->HTableSize)
		return NULL;

	Assert(shtab->SeqScanCurElem < shtab->HTableSize);

	do
	{
		if (shtab->state[shtab->SeqScanCurElem] == SHASH_USED)
			return ELEM(shtab->SeqScanCurElem++);
		else
		 shtab->SeqScanCurElem++;
	}
	while (shtab->SeqScanCurElem < shtab->HTableSize);

	return NULL;
}

#include "limits.h"

void*
SHASH_Search(SHTAB* shtab, void *keyPtr, SHASHACTION action, bool *foundPtr)
{
	uint64	index,
			first,
			first_removed_index = ULONG_MAX;
	void	*result = NULL;

	check(shtab);

	Assert(shtab->nElements <= shtab->Header.ElementsMaxNum);

	first = index = shtab->Header.HashFunc(keyPtr, shtab->Header.KeySize, shtab->HTableSize);
	Assert(index < shtab->HTableSize);

	if (foundPtr != NULL)
		*foundPtr = false;

	/*
	 * Main hash table search cycle
	 */
	for(;;)
	{
		if (shtab->state[index] == SHASH_NUSED)
		{
			/* Empty position found */
			switch (action)
			{
			case SHASH_ENTER:
				if (shtab->nElements < shtab->Header.ElementsMaxNum)
				{
					if (first_removed_index != ULONG_MAX)
						index= first_removed_index;

					memset(ELEM(index), 0, shtab->Header.ElementSize);
					memcpy(ELEM(index), keyPtr, shtab->Header.KeySize);
					shtab->state[index] = SHASH_USED;
					shtab->nElements++;
					result = ELEM(index);
				}
				break;
			case SHASH_FIND:
			case SHASH_REMOVE:
				break;
			default:
				Assert(0);
			}

			return result;
		}

		if ((shtab->state[index] == SHASH_USED) && (shtab->Header.CompFunc(keyPtr, ELEM(index))))
		{
			/*
			 * We found the element exactly
			 */
			if (foundPtr != NULL)
				*foundPtr = true;

			if (action == SHASH_REMOVE)
			{
				/* Data will be cleaned by ENTER operation */
				shtab->state[index] = SHASH_REMOVED;
				shtab->nElements--;
			}

			return ELEM(index);
		}

		if ((shtab->state[index] == SHASH_REMOVED) && (action == SHASH_ENTER))
			/* We can use this element for SHASH_ENTER, potentially */
			if (first_removed_index == ULONG_MAX)
				first_removed_index = index;

		/* Go to next element */
		index = (index+1)%(shtab->HTableSize);

		if (index == first)
		{
			/*
			 * We made all-table search cycle.
			 */
			if ((action == SHASH_ENTER) && (first_removed_index != ULONG_MAX) && (shtab->nElements < shtab->Header.ElementsMaxNum))
			{
				index = first_removed_index;
				memset(ELEM(index), 0, shtab->Header.ElementSize);
				memcpy(ELEM(index), keyPtr, shtab->Header.KeySize);
				shtab->state[index] = SHASH_USED;
				shtab->nElements++;
				result = ELEM(index);
			}
			else
				result = NULL;
			return result;
		}
	}

	Assert(0);
	return NULL;
}

uint64
DefaultHashValueFunc(void *key, uint64 size, uint64 base)
{
	uint64	sum = 0;
	int i;

	for (i = 0; i < size; i++)
	{
		uint64 x = ((char *)key)[i];
		sum += x*x*x*x + x*x*x + x*x + x + 1;
	}

	return sum%base;
}
