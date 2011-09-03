/*
   LZ4 HC - High Compression Mode of LZ4
   Copyright (C) 2011, Yann Collet.
   GPL v2 License

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


//**************************************
// Includes
//**************************************
#include "lz4hc.h"
#include "mmc.h"


//**************************************
// Basic Types
//**************************************
#if defined(_MSC_VER) 
#define BYTE	unsigned __int8
#define U16		unsigned __int16
#define U32		unsigned __int32
#define S32		__int32
#else
#include <stdint.h>
#define BYTE	uint8_t
#define U16		uint16_t
#define U32		uint32_t
#define S32		int32_t
#endif


//**************************************
// Constants
//**************************************
#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)


//****************************
// Compression CODE
//****************************

int LZ4_compressHCCtx(void* ctx,
				 char* source, 
				 char* dest,
				 int isize)
{	
	BYTE	*ip = (BYTE*) source,
			*anchor = ip,
			*iend = ip + isize,
			*ilimit = iend - MINMATCH;

	BYTE	*op = (BYTE*) dest,  
			*ref,
			*orun, *l_end;
	
	int		len, length;
	U32		ml;


	// Main Loop
	while (ip < ilimit)
	{
		ml = MMC_InsertAndFindBestMatch (ctx, (char*)ip, iend-ip, (char**)(&ref));

		if (!ml) { ip++; continue; }

		// Encode Literal length
		length = ip - anchor;
		orun = op++;
		if (length>=(int)RUN_MASK) { *orun=(RUN_MASK<<ML_BITS); len = length-RUN_MASK; for(; len > 254 ; len-=255) *op++ = 255; *op++ = (BYTE)len; } 
		else *orun = (length<<ML_BITS);

		// Copy Literals
		l_end = op + length;
		while (op<l_end)  { *(U32*)op = *(U32*)anchor; op+=4; anchor+=4; }
		op = l_end;

		// Encode Offset
		*(U16*)op = (ip-ref); op+=2;

		// Encode MatchLength
		len = (int)(ml-MINMATCH);
		if (len>=(int)ML_MASK) { *orun+=ML_MASK; len-=ML_MASK; for(; len > 509 ; len-=510) { *op++ = 255; *op++ = 255; } if (len > 254) { len-=255; *op++ = 255; } *op++ = (BYTE)len; } 
		else *orun += len;			

		// Prepare next loop
		MMC_InsertMany (ctx, (char*)ip+1, ml-1);
		ip += ml;
		anchor = ip; 
	}

	// Encode Last Literals
	len = length = iend - anchor;
	if (length)
	{
		orun=op++;
		if (len>=(int)RUN_MASK) { *orun=(RUN_MASK<<ML_BITS); len-=RUN_MASK; for(; len > 254 ; len-=255) *op++ = 255; *op++ = (BYTE) len; } 
		else *orun = (len<<ML_BITS);
		for(;length>0;length-=4) { *(U32*)op = *(U32*)anchor; op+=4; anchor+=4; }
		op += length;    // correction
	}

	// End
	return (int) (((char*)op)-dest);
}


int LZ4_compressHC(char* source, 
				 char* dest,
				 int isize)
{
	void* ctx = MMC_Create (source);
	int result = LZ4_compressHCCtx(ctx, source, dest, isize);
	MMC_Free (&ctx);

	return result;
}




