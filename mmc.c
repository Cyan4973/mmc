/*
    MMC (Morphing Match Chain)
    Match Finder
    Copyright (C) Yann Collet 2010-2011

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, see <http://www.gnu.org/licenses/>,
    or write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
    - MMC homepage : http://fastcompression.blogspot.com/p/mmc-morphing-match-chain.html
    - MMC source repository : http://code.google.com/p/mmc/
*/

/* **********************************************************
* Includes
************************************************************/
#include <stdlib.h>
#define ALLOCATOR(s) calloc(1,s)
#define REALLOCATOR(p,s) realloc(p,s)
#define FREEMEM free
#include <string.h>
#define MEM_INIT memset

#include "mmc.h"
#include "mem.h"   /* basic types and mem access */



/* ***********************************************************
*  Constants
************************************************************/
#define MINMATCH 4                   // Note : for the time being, this cannot be changed
#define DICTIONARY_LOGSIZE 16        // Dictionary Size as a power of 2 (ex : 2^16 = 64K)
                                     // Total RAM allocated is 10x Dictionary (ex : Dictionary 64K ==> 640K)

#define MAXD (1 << DICTIONARY_LOGSIZE)
#define MAXD_MASK ((U32)(MAXD - 1))
#define MAX_DISTANCE (MAXD - 1)

#define HASH_LOG (DICTIONARY_LOGSIZE-1)
#define HASHTABLESIZE (1 << HASH_LOG)
#define HASH_MASK (HASHTABLESIZE - 1)

#define MAX_LEVELS_LOG (DICTIONARY_LOGSIZE-1)
#define MAX_LEVELS (1U<<MAX_LEVELS_LOG)
#define MAX_LEVELS_MASK (MAX_LEVELS-1)

#define NBCHARACTERS 256
#define NB_INITIAL_SEGMENTS 16

#define LEVEL_DOWN ((BYTE*)1)


/* **********************************************************
*  Local Types
************************************************************/
typedef struct
{
    const BYTE* levelUp;
    const BYTE* nextTry;
}selectNextHop_t;

typedef struct
{
    const BYTE* position;
    U32   size;
} segmentInfo_t;

typedef struct
{
    segmentInfo_t * segments;
    U16 start;
    U16 max;
} segmentTracker_t;

struct MMC_ctx_s
{
    const BYTE* beginBuffer;        /* First byte of data buffer being searched */
    const BYTE* lastPosInserted;    /* excluded */
    const BYTE* hashTable[HASHTABLESIZE];
    selectNextHop_t chainTable[MAXD];
    segmentTracker_t segments[NBCHARACTERS];
    const BYTE* levelList[MAX_LEVELS];
    const BYTE** trackPtr[NBCHARACTERS];
    U16 trackStep[NBCHARACTERS];
};  /* typedef'd to MMC_ctx within "mmc.h" */


/* **********************************************************
*  Macros
************************************************************/
#define HASH_VALUE(p)    MMC_hash(MEM_read32(p))
#define NEXT_TRY(p)      chainTable[(size_t)(p) & MAXD_MASK].nextTry
#define LEVEL_UP(p)      chainTable[(size_t)(p) & MAXD_MASK].levelUp
#define ADD_HASH(p)      { NEXT_TRY(p) = HashTable[HASH_VALUE(p)]; LEVEL_UP(p)=0; HashTable[HASH_VALUE(p)] = p; }
#define LEVEL(l)         levelList[(l)&MAX_LEVELS_MASK]


/* **********************************************************
*  Object Allocation
************************************************************/
MMC_ctx* MMC_create (void)
{
    return (MMC_ctx*) ALLOCATOR(sizeof(MMC_ctx));
}

size_t MMC_reset (MMC_ctx* MMC, const void* beginBuffer)
{
    MMC->beginBuffer = (const BYTE*)beginBuffer;
    MMC->lastPosInserted = MMC->beginBuffer;
    MEM_INIT(MMC->chainTable, 0, sizeof(MMC->chainTable));
    MEM_INIT(MMC->hashTable,  0, sizeof(MMC->hashTable));
    // Init RLE detector
    {
        int c;
        for (c=0; c<NBCHARACTERS; c++)
        {
            MMC->segments[c].segments = (segmentInfo_t *) REALLOCATOR(MMC->segments[c].segments, NB_INITIAL_SEGMENTS * sizeof(segmentInfo_t));
            MMC->segments[c].max = NB_INITIAL_SEGMENTS;
            MMC->segments[c].start = 0;
            MMC->segments[c].segments[0].size = -1;
            MMC->segments[c].segments[0].position = (const BYTE*)beginBuffer - (MAX_DISTANCE+1);
        }
    }
    return 1;
}


size_t MMC_free (MMC_ctx* ctx)
{
    // RLE dynamic structure releasing
    {
        int c;
        for (c=0; c<NBCHARACTERS; c++) FREEMEM(ctx->segments[c].segments);
    }
    FREEMEM(ctx);
    return 1;
}


/* *******************************************************************
*  Basic Search operations (Greedy / Lazy / Flexible parsing)
*********************************************************************/
static U32 MMC_hash(U32 u) { return (u * 2654435761U) >> (32-HASH_LOG); }

static void MMC_insert (MMC_ctx* ctx, const void* targetPtr);

size_t MMC_insertAndFindBestMatch (MMC_ctx* MMC, const void* inputPointer, size_t maxLength, const void** matchpos)
{
    segmentTracker_t * const Segments = MMC->segments;
    selectNextHop_t * const chainTable = MMC->chainTable;
    const BYTE** const HashTable = MMC->hashTable;
    const BYTE** const levelList = MMC->levelList;
    const BYTE*** const trackPtr = MMC->trackPtr;
    U16* trackStep = MMC->trackStep;
    const BYTE* ip = (const BYTE*)inputPointer;
    const BYTE* const iend = ip + maxLength;
    const BYTE*  ref;
    const BYTE** gateway;
    const BYTE*  currentP;
    U16 stepNb=0;
    U32 currentLevel, maxLevel;
    U32 ml, mlt, nbChars, sequence;

    MMC_insert(MMC, ip);

    ml = mlt = nbChars = 0;
    sequence = MEM_read32(ip);

    /* rle match finder */
    if (   ((U16)sequence==(U16)(sequence>>16))
        && ((BYTE)sequence == (BYTE)(sequence>>8)) )
    {
        BYTE c = (BYTE)sequence;
        U32 index = Segments[c].start;
        const BYTE* endSegment = ip+MINMATCH;

        while ((*endSegment==c) && (endSegment<iend)) endSegment++;
        nbChars = endSegment-ip;

        while (Segments[c].segments[index].size < nbChars) index--;

        if ((Segments[c].segments[index].position - nbChars) <= (ip - MAX_DISTANCE))
        {
            /* no "previous" segment within range */
            NEXT_TRY(ip) = LEVEL_UP(ip) = 0;
            if ((ip>MMC->beginBuffer) && (*(ip-1)==c))   /* obvious RLE solution */
            {
                *matchpos= ip-1;
                return nbChars;
            }
            return 0;
        }

        ref = NEXT_TRY(ip)= Segments[c].segments[index].position - nbChars;
        currentLevel = maxLevel = ml = nbChars;
        LEVEL(currentLevel) = ip;
        gateway = 0;   // work around due to erasing
        LEVEL_UP(ip) = 0;
        if (*(ip-1)==c) *matchpos = ip-1; else *matchpos = ref;     // "basis" to be improved upon
        if (nbChars==MINMATCH)
            gateway = &LEVEL_UP(ip);
        goto _FindBetterMatch;
    }

    /* MMC match finder */
    ref = HashTable[MMC_hash(sequence)];
    if (!ref) return 0;
    gateway = &LEVEL_UP(ip);
    currentLevel = maxLevel = MINMATCH-1;
    LEVEL(MINMATCH-1) = ip;

    // Collision detection & avoidance
    while ((ref) && ((ip-ref) < MAX_DISTANCE))
    {
        if (MEM_read32(ref) != sequence)
        {
            LEVEL(MINMATCH-1) = ref;
            ref = NEXT_TRY(ref);
            continue;
        }

        mlt = MINMATCH;
        while ((mlt<(U32)maxLength) && (*(ip+mlt)) == *(ref+mlt)) mlt++;

        if (mlt>ml)
        {
            ml = mlt;
            *matchpos = ref;
        }

        // Continue level mlt chain
        if (mlt<=maxLevel)
        {
            NEXT_TRY(LEVEL(mlt)) = ref; LEVEL(mlt) = ref;    // Completing chain at Level mlt
        }

        // New level creation
        else
        {
            if (gateway)            // Only guaranteed the first time (gateway is ip)
            {
                maxLevel++;
                *gateway = ref;
                LEVEL(maxLevel)=ref;                        // First element of level maxLevel
                if (mlt>maxLevel) gateway=&(LEVEL_UP(ref)); else gateway=0;
            }

            // Special case : no gwTo+1, but mlt>maxLevel
            else
            {
                gateway = &(LEVEL_UP(ref));
                NEXT_TRY(LEVEL(maxLevel)) = ref; LEVEL(maxLevel) = ref;        // Completing chain at Level maxLevel
            }
        }

        currentP = ref;
        NEXT_TRY(LEVEL(MINMATCH-1)) = NEXT_TRY(ref);        // Extraction from base level
        if (LEVEL_UP(ref))
        {
            ref=LEVEL_UP(ref);
            NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;    // Clean, because extracted
            currentLevel++;
            NEXT_TRY(LEVEL(MINMATCH)) = ref;
            break;
        }
        ref=NEXT_TRY(ref);
        NEXT_TRY(currentP) = 0;                             // initialisation, due to promotion; note that LEVEL_UP(ref)=0;
    }

    if (ml==0) return 0;  /* no match found */


_FindBetterMatch:
    while ((ref) && ((ip-ref) < MAX_DISTANCE))
    {
        /* Reset rolling counter for Secondary Promotions */
        if (!stepNb)
        {
            U32 i;
            for (i=0; i<NBCHARACTERS; i++) trackStep[i]=0;
            stepNb=1;
        }

        /* Match Count */
        mlt = currentLevel;
        while ((mlt<(U32)maxLength) && (*(ip+mlt)) == *(ref+mlt)) mlt++;

        /* First case : No improvement => continue on current chain */
        if (mlt==currentLevel)
        {
            BYTE c = *(ref+currentLevel);
            if (trackStep[c] == stepNb)                               // this wrong character was already met before
            {
                const BYTE* next = NEXT_TRY(ref);
                *trackPtr[c] = ref;                                   // linking
                NEXT_TRY(LEVEL(currentLevel)) = NEXT_TRY(ref);        // extraction
                if (LEVEL_UP(ref))
                {
                    NEXT_TRY(ref) = LEVEL_UP(ref);                    // Promotion
                    LEVEL_UP(ref) = 0;
                    trackStep[c] = 0;                                 // Shutdown chain (avoid overwriting when multiple unfinished chains)
                }
                else
                {
                    NEXT_TRY(ref) = LEVEL_DOWN;                        // Promotion, but link back to previous level for now
                    trackPtr[c] = &(NEXT_TRY(ref));                    // saving for next link
                }

                if (next==LEVEL_DOWN)
                {
                    NEXT_TRY(LEVEL(currentLevel)) = 0;                 // Erase the LEVEL_DOWN
                    currentLevel--; stepNb++;
                    next = NEXT_TRY(LEVEL(currentLevel));
                    while (next > ref) { LEVEL(currentLevel) = next; next = NEXT_TRY(next); }
                }
                ref = next;

                continue;
            }

            /* first time we see this character */
            if (LEVEL_UP(ref)==0)   // don't interfere if a serie has already started...
                // Note : to "catch up" the serie, it would be necessary to scan it, up to its last element
                // this effort would be useless if the chain is complete
                // Alternatively : we could keep that gw in memory, then scan the chain when finding it is not complete.
                // But would it be worth it ??
            {
                trackStep[c] = stepNb;
                trackPtr[c] = &(LEVEL_UP(ref));
            }

_continue_same_level:
            LEVEL(currentLevel) = ref;
            ref = NEXT_TRY(ref);
            if (ref == LEVEL_DOWN)
            {
                const BYTE* localCurrentP = LEVEL(currentLevel);
                const BYTE* next = NEXT_TRY(LEVEL(currentLevel-1));
                NEXT_TRY(localCurrentP) = 0;                            // Erase the LEVEL_DOWN
                while (next>localCurrentP) { LEVEL(currentLevel-1) = next; next = NEXT_TRY(next);}
                ref = next;
                currentLevel--; stepNb++;
            }
            continue;
        }

        /* Now, mlt > currentLevel */

        if (mlt>ml)
        {
            /* better solution found */
            ml = mlt;
            *matchpos = ref;
        }

        /* store into corresponding chain */
        if (mlt<=maxLevel)
        {
            NEXT_TRY(LEVEL(mlt)) = ref; LEVEL(mlt) = ref;        // Completing chain at Level mlt
_check_mmc_levelup:
            currentP = ref;
            NEXT_TRY(LEVEL(currentLevel)) = NEXT_TRY(ref);       // Extraction from base level
            if (LEVEL_UP(ref))
            {
                ref = LEVEL_UP(ref);                             // LevelUp
                NEXT_TRY(currentP) = LEVEL_UP(currentP) = 0;     // Clean, because extracted
                currentLevel++; stepNb++;
                NEXT_TRY(LEVEL(currentLevel)) = ref;             // We don't know yet ref's level, but just in case it would be only ==currentLevel...
            }
            else
            {
                ref = NEXT_TRY(ref);
                NEXT_TRY(currentP) = 0;                          // promotion to level mlt; note that LEVEL_UP(ref)=0;
                if (ref == LEVEL_DOWN)
                {
                    const BYTE* next = NEXT_TRY(LEVEL(currentLevel-1));
                    NEXT_TRY(LEVEL(currentLevel)) = 0;           // Erase the LEVEL_DOWN (which has been transfered)
                    while (next>currentP) { LEVEL(currentLevel-1) = next; next = NEXT_TRY(next); }
                    ref = next;
                    currentLevel--; stepNb++;
                }
            }
            continue;
        }

        // MaxLevel increase
        if (gateway)
        {
            *gateway = ref;
            maxLevel++;
            LEVEL(maxLevel) = ref;                               // First element of level max
            if (mlt>maxLevel) gateway=&(LEVEL_UP(ref)); else gateway=0;
            goto _check_mmc_levelup;
        }

        // Special case : mlt>maxLevel==currentLevel, no Level_up nor gateway
        if ((maxLevel==currentLevel) && (!(LEVEL_UP(ref))))
        {
            gateway = &(LEVEL_UP(ref));   /* note : *gateway = 0 */
            goto _continue_same_level;
        }

        // Special case : mlt>maxLevel==currentLevel, Level_up available, but no gateway
        if (maxLevel==currentLevel)
        {
            LEVEL(currentLevel) = ref;
            ref = LEVEL_UP(ref);
            maxLevel++;
            currentLevel++; stepNb++;
            continue;
        }

        /* Special case : mlt>maxLevel, but no gw; Note that we don't know about level_up yet */
        {
            gateway = &(LEVEL_UP(ref));
            NEXT_TRY(LEVEL(maxLevel)) = ref; LEVEL(maxLevel) = ref;   /* Completing chain of maxLevel */
            goto _check_mmc_levelup;
        }

    }

    if (gateway) *gateway=ip-MAX_DISTANCE-1;    /* early end trick */
    stepNb++;

    /* prevent match beyond buffer */
    if ((ip+ml)>iend) ml = iend-ip;

    return ml;
}


static size_t MMC_insert_once (MMC_ctx* MMC, const void* ptr, size_t max)
{
    segmentTracker_t * Segments = MMC->segments;
    selectNextHop_t * chainTable = MMC->chainTable;
    const BYTE** HashTable = MMC->hashTable;
    const BYTE* ip = (const BYTE*)ptr;
    const BYTE* iend = ip+max;
    const BYTE* beginBuffer = MMC->beginBuffer;

    /* Stream updater */
    if ((MEM_read16(ip) == MEM_read16(ip+2)) && (*ip == *(ip+1)))
    {
        BYTE c = *ip;
        U32 nbForwardChars, nbPreviousChars, segmentSize, n=MINMATCH;
        const BYTE* endSegment = ip+MINMATCH;
        const BYTE* baseStreamP = ip;

        iend += MINMATCH;
        while ((*endSegment==c) && (endSegment<iend)) endSegment++;
        if (endSegment == iend) return (iend-ip);     /* skip the whole forward segment; we'll start again later */
        nbForwardChars = endSegment-ip;
        while ((baseStreamP>beginBuffer) && (baseStreamP[-1]==c)) baseStreamP--;
        nbPreviousChars = ip-baseStreamP;
        segmentSize = nbForwardChars + nbPreviousChars;
        if (segmentSize > MAX_DISTANCE-1) segmentSize = MAX_DISTANCE-1;

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

        if (Segments[c].segments[Segments[c].start].position <= (ip-MAX_DISTANCE))
            Segments[c].start = 0;   /* no large enough serie within range */

        for ( ; n<=segmentSize ; n++)
        {
            NEXT_TRY(endSegment-n) = Segments[c].segments[Segments[c].start].position - n;
            LEVEL_UP(endSegment-n) = 0;
        }

        /* overflow protection : new segment smaller than previous, but too many segments in memory */
        if (Segments[c].start > Segments[c].max-2)
        {
            int beginning=0;
            U32 i;

            Segments[c].max *= 2;
            Segments[c].segments = (segmentInfo_t *) REALLOCATOR (Segments[c].segments, (Segments[c].max)*sizeof(segmentInfo_t));
            while (Segments[c].segments[beginning].position < (ip-MAX_DISTANCE)) beginning++;
            i = beginning;
            while (i<=Segments[c].start) { Segments[c].segments[i - (beginning-1)] = Segments[c].segments[i]; i++; }
            Segments[c].start -= (beginning-1);

        }
        Segments[c].start++;
        Segments[c].segments[Segments[c].start].position = endSegment;
        Segments[c].segments[Segments[c].start].size = segmentSize;

        return (endSegment-ip-(MINMATCH-1));
    }

    /* Normal update */
    NEXT_TRY(ip) = HashTable[HASH_VALUE(ip)];
    LEVEL_UP(ip)=0;
    HashTable[HASH_VALUE(ip)] = ip;

    return 1;
}


static void MMC_insert (MMC_ctx* ctx, const void* targetPtr)
{
    const BYTE* current = ctx->lastPosInserted;
    while  (current<(const BYTE*)targetPtr) current += MMC_insert_once (ctx, current, ((const BYTE*)targetPtr) - current);
    ctx->lastPosInserted = targetPtr;
}


