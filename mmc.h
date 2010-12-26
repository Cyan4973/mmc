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
// Basic Types
//************************************************************
#define BYTE unsigned char
#define U16  unsigned short
#define U32  unsigned long


//************************************************************
// Constants
//************************************************************
#define MINMATCH 4					// Note : for the time being, this cannot be changed
#define DICTIONARY_LOGSIZE 16		// Dictionary Size as a power of 2


//************************************************************
// Creation & Destruction
//************************************************************

void* MMC_Create (BYTE* beginBuffer, BYTE* endBuffer);
U32 MMC_Init (void* MMC_Data, BYTE* beginBuffer, BYTE* endBuffer);
U32 MMC_Free (void** MMC_Data);

/*
MMC_Create : note that dictionary Size is a compilation directive !
			BYTE* startBuffer, BYTE* endBuffer : first and last+1 byte of data buffer being searched
			return : Pointer to MMC Data Structure; NULL = error
MMC_Init : reset MMC_Data; 
			MMC_Init is automatically called within MMC_Create, so this is only useful on later initialization;
			return : 0 = error; 1 = OK;
MMC_Free : free memory from MMC Data Structure; pointer MMC_Data should be valid; 
			return : 0 = error, 1+ = Nb of bytes free;
*/

//************************************************************
// Basic Search operations (Greedy / Lazy / Flexible parsing)
//************************************************************

U32 MMC_InsertAndFindBestMatch (void* MMC_Data, BYTE* ip, BYTE** r);
U32 MMC_Insert1 (void* MMC_Data, BYTE* ip);
U32 MMC_InsertMany (void* MMC_Data, BYTE* ip, U32 length);

/*
MMC_InsertAndFindBestMatch :
	p : position being inserted & tested
	return : length of Best Match (which is >= MINMATCH)
			if return == 0, no match was found
			if return > 0, then match position is into r
MMC_InsertMany :
		return : 0 = error; 1+ = Nb of bytes inserted;
MMC_Insert1 :
		return : 0 = error; 1+ = Nb of bytes inserted;
*/

//************************************************************
// Advanced Search operations (Optimal parsing)
//************************************************************

U32 MMC_InsertAndFindFirstMatch (void* MMC_Data, BYTE* ip, BYTE** r);
U32 MMC_FindBetterMatch (void* MMC_Data, BYTE** r);

/*
MMC_InsertAndFindFirstMatch :
	p : position being inserted & tested
	return : length of First Match found (which is likely to be MINMATCH, but could be more too)
			if return > 0, then match position is into r
MMC_FindBetterMatch :
	note : there is no "p" input => not needed, we just continue from previous search position (stateful)
	return : length of Better Match found (which is always > than previous match length)
			if no better match is found, result is 0
			if return > 0, then match position is into r
*/

