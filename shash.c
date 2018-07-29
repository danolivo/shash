#include "assert.h"
#include "math.h"
#include "shash.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "stdio.h"

void check(SHTAB* shtab)
{
	assert(shtab != NULL);
	assert(shtab->Elements != NULL);
	assert(shtab->state != NULL);
}

SHTAB*
SHASH_Create(SHTABCTL shctl)
{
	SHTAB*	shtab = (SHTAB *) calloc(1, sizeof(SHTAB));

	assert(shctl.ElementSize != 0);
	assert(shctl.ElementSize >= shctl.KeySize);

	shtab->Header = shctl;
	shtab->HTableSize = shtab->Header.ElementsMaxNum*(2. - shtab->Header.FillFactor) + 1;
	assert(shtab->HTableSize > shtab->Header.ElementsMaxNum);
	/* Add one element as sign of empty value */
	shtab->Elements = (char *) calloc(shtab->HTableSize, shctl.ElementSize);
	shtab->state = (HESTATE *) calloc(shtab->HTableSize, sizeof(HESTATE));
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

	free(shtab->Elements);
	free(shtab->state);
	free(shtab);
}

uint64
SHASH_Entries(SHTAB* shtab)
{
	check(shtab);

	assert((shtab->nElements >= 0) && (shtab->nElements <= shtab->Header.ElementsMaxNum));
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

	assert(shtab->SeqScanCurElem < shtab->HTableSize);

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

	assert(shtab->nElements <= shtab->Header.ElementsMaxNum);

	first = index = shtab->Header.HashFunc(keyPtr, shtab->Header.KeySize, shtab->HTableSize);
	assert(index < shtab->HTableSize);

	if (foundPtr != NULL)
		*foundPtr = false;

	/*
	 * Main hash table search cycle
	 */
	for(;;)
	{
//		uint64	pos = index*shtab->Header.ElementSize;

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
				assert(0);
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

	assert(0);
	return NULL;
}
