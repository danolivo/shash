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
