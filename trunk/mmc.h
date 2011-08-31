/*
   MMC (Morphing Match Chain)
   Match Finder
   Copyright (C) Yann Collet 2010-2011

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
// Constants
//************************************************************
#define MINMATCH 4					// Note : for the time being, this cannot be changed
#define DICTIONARY_LOGSIZE 16		// Dictionary Size as a power of 2 (ex : 2^16 = 64K)
									// Total RAM allocated is 10x Dictionary (ex : Dictionary 64K ==> 640K)


//************************************************************
// Creation & Destruction
//************************************************************

void* MMC_Create (char* beginBuffer);
int MMC_Init (void* MMC_Data, char* beginBuffer);
int MMC_Free (void** MMC_Data);

/*
MMC_Create : (Note : Dictionary Size is a compilation directive !)
			BYTE* startBuffer : first byte of data buffer being searched
			return : Pointer to MMC Data Structure; NULL = error
MMC_Init : reset MMC_Data; 
			Note : MMC_Init is automatically called within MMC_Create, so this is only useful for later initializations;
			return : 1 = OK; 0 = error; 
MMC_Free : free memory from MMC Data Structure; caution : pointer MMC_Data must be valid !
			return : 1+ = OK; 0 = error;
*/

//************************************************************
// Basic Search operations (Greedy / Lazy / Flexible parsing)
//************************************************************

int MMC_InsertAndFindBestMatch (void* MMC_Data, char* inputPointer, int maxLength, char** matchpos);
int MMC_Insert1 (void* MMC_Data, char* inputPointer);
int MMC_InsertMany (void* MMC_Data, char* inputPointer, int length);

/*
MMC_InsertAndFindBestMatch :
	inputPointer : position being inserted & searched
	maxLength : maximum match length autorized
	return : length of Best Match
			if return == 0, no match was found
			if return > 0, then match position is into matchpos
MMC_Insert1 :
		inputPointer : position being inserted
		return : 1+ = Nb of bytes inserted; 0 = error; 
MMC_InsertMany :
		inputPointer : start position of segment being inserted
		return : 1+ = Nb of bytes inserted; 0 = error; 
*/

//************************************************************
// Advanced Search operations (Optimal parsing)
//************************************************************

/*
Not completed (yet)

int MMC_InsertAndFindFirstMatch (void* MMC_Data, char* inputPointer, int maxLength, char** matchpos);
int MMC_FindBetterMatch (void* MMC_Data, char** matchpos);
*/

/*
MMC_InsertAndFindFirstMatch :
	inputPointer : position being inserted & tested
	return : length of First Match found (which is likely to be MINMATCH, but could be more too)
			if return > 0, then match position is into r
MMC_FindBetterMatch :
	note : there is no "p" input => not needed, we just continue from previous search position (stateful)
	return : length of Better Match found (which is always > than previous match length)
			if no better match is found, result is 0
			if return > 0, then match position is into r
*/

