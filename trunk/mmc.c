/*
   MMC (Morphing Match Chain)
   Match Finder
   Copyright (C) Yann Collet 2010,

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

//************************************************************
// Includes
//************************************************************
#include "mmc.h"

// Allocator definitions
#include <stdlib.h>
#define ALLOCATOR malloc
#define FREEMEM free


//************************************************************
// Constants
//************************************************************
#define NBCHARACTERS 256
#define MAXNBSEGMENTS 256

#define MAX_LEVELS_LOG 16
#define MAX_LEVELS (1U<<MAX_LEVELS_LOG)
#define MAX_LEVELS_MASK (MAX_LEVELS-1)

#define HASH_LOG ((DICTIONARY_LOGSIZE/2)+9)  // HASH_LOG = 17 (128K) for Dictionary of 64K
#define HASHTABLESIZE (1 << HASH_LOG)
#define HASH_MASK (HASHTABLESIZE - 1)

#define MAXD (1U<<DICTIONARY_LOGSIZE)
#define MAXD_MASK (MAXD - 1)
#define MAX_DISTANCE (MAXD - 1)


//************************************************************
// Local Types
//************************************************************
struct selectNextHop
{
	BYTE* levelUp;
	BYTE* nextTry;
};

struct segmentInfo
{
	BYTE* position;
	U32   size;
};

struct segmentTracker
{
	struct segmentInfo segments[MAXNBSEGMENTS];
	U32 start;
};

struct MMC_Data_Structure
{
	BYTE* beginBuffer;		// First byte of data buffer being searched
	BYTE* endBuffer;		// Last+1 byte of data buffer being searched
	BYTE* hashTable[HASHTABLESIZE];
	struct selectNextHop chainTable[MAXD];
	struct segmentTracker segments[NBCHARACTERS];
	BYTE* levelList[MAX_LEVELS];
};


//************************************************************
// Macro
//************************************************************
#define HASH_FUNCTION(i)	((i * 2654435761) >> ((MINMATCH*8)-HASH_LOG))
#define HASH_VALUE(p)		HASH_FUNCTION(*(U32*)p)
#define HASH_POINTER(p)		HashTable[HASH_VALUE(p)]
#define NEXT_TRY(p)			chainTable[(U32)(p) & MAXD_MASK].nextTry 
#define LEVEL_UP(p)			chainTable[(U32)(p) & MAXD_MASK].levelUp
#define ADD_HASH(p)			{ NEXT_TRY(p) = HashTable[HASH_VALUE(p)]; LEVEL_UP(p)=0; HashTable[HASH_VALUE(p)] = p; }
#define LEVEL(l)			levelList[(l)&MAX_LEVELS_MASK]


//************************************************************
// Creation & Destruction
//************************************************************
void* MMC_Create (BYTE* beginBuffer, BYTE* endBuffer)
{
	void* r;
	r = ALLOCATOR(sizeof(struct MMC_Data_Structure));
	MMC_Init(r, beginBuffer, endBuffer);
	return r;
}


U32 MMC_Init (void* MMC_Data, BYTE* beginBuffer, BYTE* endBuffer)
{
	struct MMC_Data_Structure * MMC = (struct MMC_Data_Structure *) MMC_Data;

	MMC->beginBuffer = beginBuffer;
	MMC->endBuffer = endBuffer;
	memset(MMC->hashTable, 0, sizeof(MMC->hashTable));
	memset(MMC->chainTable, 0, sizeof(MMC->chainTable));
	memset(MMC->segments, 0, sizeof(MMC->segments)); { U32 i; for (i=0; i<NBCHARACTERS; i++) { MMC->segments[i].segments[0].size = -1; MMC->segments[i].segments[0].position = beginBuffer-(MAX_DISTANCE+1); } }

	return 1;
}


U32 MMC_Free (void** MMC_Data)
{
	FREEMEM(*MMC_Data);
	*MMC_Data = NULL;
	return (sizeof(struct MMC_Data_Structure));
}


//************************************************************
// Basic Search operations (Greedy / Lazy / Flexible parsing)
//************************************************************
U32 MMC_InsertAndFindBestMatch (void* MMC_Data, BYTE* ip, BYTE** r)
{
	struct MMC_Data_Structure * MMC = (struct MMC_Data_Structure *) MMC_Data;
	BYTE* iend = MMC->endBuffer;
	struct segmentTracker * Segments = MMC->segments;
	struct selectNextHop * chainTable = MMC->chainTable;
	BYTE** levelList = MMC->levelList;
	BYTE** HashTable = MMC->hashTable;
	BYTE* ref;
	BYTE** gateway;
	U32 currentLevel, maxLevel;
	U32 matchLength, mlt, nbChars, sequence;

	matchLength = mlt = nbChars = 0;
	sequence = *(U32*)ip;

	// Special Stream match finder
	if ((U16)sequence==(U16)(sequence>>16))
	if ((BYTE)sequence == (BYTE)(sequence>>8))
	{
		BYTE c = (BYTE)sequence;
		U32 index = Segments[c].start;
		BYTE* endSegment = ip+4;

		while ((*endSegment==c) && (endSegment<iend)) endSegment++; 
		nbChars = endSegment-ip;

		while (Segments[c].segments[index].size < nbChars) index--;

		if ((Segments[c].segments[index].position - nbChars) <= (ip - MAX_DISTANCE))      // no large enough previous serie within range
		{
			NEXT_TRY(ip) = LEVEL_UP(ip) = 0;    // no "previous" segment within range
			if (*(ip-1)==c)				// obvious RLE solution
			{
				*r = ip-1;
				matchLength = nbChars;
				return matchLength;
			}
			if (nbChars==MINMATCH)  	// isolated cccc sequence
			{
				Segments[c].start = 1;
				Segments[c].segments[1].position = ip + MINMATCH;
				Segments[c].segments[1].size = MINMATCH;
				return 0;
			}
			// No solution at position ip
			// but there will be one at ip+1
			return 0;
		}

		ref = NEXT_TRY(ip)= Segments[c].segments[index].position - nbChars;
		currentLevel = maxLevel = matchLength = nbChars;
		LEVEL(currentLevel) = ip;
		gateway = 0; // work around due to erasing
		LEVEL_UP(ip) = 0;
		if (*(ip-1)==c) *r = ip-1; else *r = ref;     // "basis" to be improved upon
		if (nbChars==MINMATCH) 
		{
			MMC_Insert1(MMC, ip);
			gateway = &LEVEL_UP(ip);
		}
		goto _FindBetterMatch;
	}


	// MMC Sequence match finder
	ref=HashTable[HASH_FUNCTION(sequence)];
	ADD_HASH(ip);
	if (!ref) return 0;
	gateway = &LEVEL_UP(ip);
	currentLevel = maxLevel = MINMATCH-1;
	LEVEL(currentLevel) = ip;

	// Collision detection & avoidance
	while ((ref) && ((ip-ref) < MAX_DISTANCE))
	{

		if (*(U32*)ref != sequence)
		{
			LEVEL(currentLevel) = ref; 
			ref = NEXT_TRY(ref);
			continue;
		}

		mlt = MINMATCH;
		while (*(ip+mlt) == *(ref+mlt)) mlt++;

		if (mlt>matchLength)
		{
			matchLength = mlt; 
			*r = ref; 
		}

_place_into_base_chain:
		if (mlt<=maxLevel)
		{
			BYTE* currentP = ref;
			NEXT_TRY(LEVEL(mlt)) = ref; LEVEL(mlt) = ref;		// Completing chain at Level mlt
			NEXT_TRY(LEVEL(MINMATCH-1)) = NEXT_TRY(ref);		// Extraction from base level
			if (LEVEL_UP(ref))
			{
				ref=LEVEL_UP(ref);
				NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;	// Clean, because extracted
				currentLevel++;
				NEXT_TRY(LEVEL(MINMATCH)) = ref;
				break;
			}
			ref=NEXT_TRY(ref);
			NEXT_TRY(currentP) = 0;								// initialisation, due to promotion; note that LEVEL_UP(ref)=0;
			continue;
		}

		// New level creation
		if (gateway)
		{
			BYTE* currentP = ref;
			maxLevel++;
			*gateway = ref;
			LEVEL(maxLevel)=ref;								// First element of level max 
			if (mlt>maxLevel) gateway=&(LEVEL_UP(ref)); else gateway=0;
			NEXT_TRY(LEVEL(MINMATCH-1)) = NEXT_TRY(ref);		// Extraction from base level
			if (LEVEL_UP(ref))
			{ 
				ref=LEVEL_UP(ref);
				NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;    // initialisation, due to promotion 
				currentLevel++;
				NEXT_TRY(LEVEL(MINMATCH)) = ref;				// dont know position of new ref yet, but it is at least MINMATCH; linking in case it would only MINMATCH
				break;
			} 
			ref=NEXT_TRY(ref);
			NEXT_TRY(currentP) = 0;								// initialisation, due to promotion; note that LEVEL_UP(ref)=0;
			continue;
		}

		// Special case : no gateway, but mlt>maxLevel
		{
			gateway = &(LEVEL_UP(ref));
			mlt = maxLevel;
			goto _place_into_base_chain;
		}
	}
	
	if (!matchLength) return 0;  // no match found


	// looking for further length of matches
_FindBetterMatch:
	while ((ref) && ((ip-ref) < MAX_DISTANCE))
	{

		mlt = currentLevel;
		while (*(ip+mlt) == *(ref+mlt)) mlt++;

		// First case : No improvement => continue on current chain
		if (mlt==currentLevel)
		{
			LEVEL(currentLevel) = ref; 
			ref = NEXT_TRY(ref);
			continue;
		}

		// Now, mlt > currentLevel
		if (mlt>matchLength)
		{
			matchLength = mlt; 
			*r = ref; 
		}


_place_into_chain:
		// placing into corresponding chain
		if (mlt<=maxLevel)
		{
			BYTE* currentP = ref;
			NEXT_TRY(LEVEL(mlt)) = ref; LEVEL(mlt) = ref;		// Completing chain at Level mlt
			NEXT_TRY(LEVEL(currentLevel)) = NEXT_TRY(ref);		// Extraction from base level
			if (LEVEL_UP(ref)) 
			{ 
				ref=LEVEL_UP(ref); 
				NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;	// Clean, because extracted
				currentLevel++; 
				NEXT_TRY(LEVEL(currentLevel)) = ref;
			} 
			else 
			{ 
				ref=NEXT_TRY(ref); 
				NEXT_TRY(currentP) = 0;							// initialisation, due to promotion; note that LEVEL_UP(ref)=0;
			}
			continue;
		}

		// New level creation, with maxLevel > currentLevel
		if (gateway)
		{
			BYTE* currentP = ref;
			maxLevel++;
			*gateway = ref;
			if (mlt>maxLevel) gateway=&(LEVEL_UP(ref)); else gateway=0;
			LEVEL(maxLevel)=ref;								// First element of level max > current+1
			NEXT_TRY(LEVEL(currentLevel)) = NEXT_TRY(ref);		// Extraction from base level
			if (LEVEL_UP(ref)) 
			{ 
				ref=LEVEL_UP(ref); 
				NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;    // initialisation, due to promotion 
				currentLevel++; 
				NEXT_TRY(LEVEL(currentLevel)) = ref;
			} 
			else 
			{ 
				ref=NEXT_TRY(ref); 
				NEXT_TRY(currentP) = 0;							// initialisation, due to promotion; note that LEVEL_UP(ref)=0;
			}
			continue;
		}

		// Special case : mlt>maxLevel, but maxLevel==currentLevel, no Level_up nor gateway
		if ((maxLevel==currentLevel) && (!(LEVEL_UP(ref))))
		{
			gateway = &(LEVEL_UP(ref));    // note : *gateway = 0
			LEVEL(currentLevel)=ref;
			ref = NEXT_TRY(ref);
			continue;
		}

		// Special case : mlt>maxLevel, but no gateway, maxLevel==currentLevel, and hopefully Level_up available
		if (maxLevel==currentLevel)
		{
			ref = LEVEL_UP(ref);
			currentLevel++;
			maxLevel++;
			continue;
		}

		// Special case : no gateway, but mlt>maxLevel
		{
			gateway = &(LEVEL_UP(ref));
			mlt = maxLevel;
			goto _place_into_chain;
		}

	}

	if (gateway) *gateway=ip-MAX_DISTANCE-1;    // early end trick

	// prevent match beyond buffer
	if (ip + matchLength > iend) matchLength=iend-ip;

	return matchLength;
}


static U32 MMC_Insert (void* MMC_Data, BYTE* ip, U32 max)
{
	struct MMC_Data_Structure * MMC = (struct MMC_Data_Structure *) MMC_Data;
	struct segmentTracker * Segments = MMC->segments;
	struct selectNextHop * chainTable = MMC->chainTable;
	BYTE** HashTable = MMC->hashTable;
	BYTE* iend = ip+max;
	BYTE* beginBuffer = MMC->beginBuffer;

	// Stream updater
	if ((*(U16*)ip == *(U16*)(ip+2)) && (*ip == *(ip+1)))
	{
		BYTE c=*ip;
		U32 nbForwardChars, nbPreviousChars, segmentSize, n=MINMATCH;
		BYTE* endSegment=ip+4;
		BYTE* baseStreamP=ip-1;

		iend += MINMATCH;
		while ((*endSegment==c) && (endSegment<iend)) endSegment++; 
		if (endSegment == iend) return (iend-ip);			// Stop update here : we'll get this match sorted out on next pass.
		nbForwardChars = endSegment-ip;
		while ((baseStreamP>beginBuffer) && (*baseStreamP==c)) baseStreamP--; baseStreamP++; nbPreviousChars = ip-baseStreamP;
		segmentSize = nbForwardChars + nbPreviousChars;
		if (segmentSize >= MAXD) segmentSize = MAXD-1;

		while (Segments[c].segments[Segments[c].start].size <= segmentSize)
		{
			if (Segments[c].segments[Segments[c].start].position <= (ip-MAX_DISTANCE)) break;
			for ( ; n<=Segments[c].segments[Segments[c].start].size ; n++) 
			{
				NEXT_TRY(endSegment-n) = Segments[c].segments[Segments[c].start].position - n;
				LEVEL_UP(endSegment-n) = 0;
			}
			Segments[c].start--;
		}
	
		if (Segments[c].segments[Segments[c].start].position <= (ip-MAX_DISTANCE)) Segments[c].start = 0;   // no large enough serie within range

		for ( ; n<=segmentSize ; n++) 
		{
			NEXT_TRY(endSegment-n) = Segments[c].segments[Segments[c].start].position - n;
			LEVEL_UP(endSegment-n) = 0;
		}

		Segments[c].start++;
		Segments[c].segments[Segments[c].start].position = endSegment;
		Segments[c].segments[Segments[c].start].size = segmentSize;

		return (endSegment-ip-(MINMATCH-1));
	}

	//Normal update
	ADD_HASH(ip); 
	return 1; 
}


U32 MMC_Insert1 (void* MMC_Data, BYTE* ip, U32 length)
{
	MMC_Insert (MMC_Data, ip, 1);
	return 1;
}

U32 MMC_InsertMany (void* MMC_Data, BYTE* ip, U32 length)
{
	BYTE* iend = ip+length;
	while  (ip<iend) ip += MMC_Insert (MMC_Data, ip, iend-ip);
	return length;
}


//************************************************************
// Advanced Search operations (Optimal parsing)
//************************************************************

U32 MMC_InsertAndFindFirstMatch (void* MMC_Data, BYTE* ip, BYTE** r)
{
	return 0;
}


U32 MMC_FindBetterMatch (void* MMC_Data, BYTE** r)
{
	return 0;
}


