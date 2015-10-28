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
    - MMC source repository : https://github.com/Cyan4973/mmc
*/

#ifndef MMC_H
#define MMC_H

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include <stddef.h>   /* size_t */


/* **********************************************************
*  Object Allocation
************************************************************/
typedef struct MMC_ctx_s MMC_ctx;   /* incomplete type */

MMC_ctx* MMC_create (void);
size_t   MMC_reset  (MMC_ctx* ctx, const void* beginBuffer);
size_t   MMC_free   (MMC_ctx* ctx);

/**
MMC_create : (Note : Dictionary Size is a compilation directive !)
            BYTE* startBuffer : first byte of data buffer being searched
            return : Pointer to MMC Data Structure; NULL = error
MMC_reset : reset MMC_Data;
            Note : MMC_Init is automatically called within MMC_Create, so this is only useful for later initializations;
            return : 1 = OK; 0 = error;
MMC_free : free memory from MMC Data Structure; caution : pointer MMC_Data must be valid !
            return : 1+ = OK; 0 = error;
*/

/* ***********************************************************
*  Search operations
*************************************************************/

size_t MMC_insertAndFindBestMatch (MMC_ctx* ctx, const void* inputPointer, size_t maxLength, const void** matchpos);

/**
MMC_insertAndFindBestMatch :
    @inputPointer : position being inserted & searched
    @maxLength : maximum match length autorized
    @return : length of Best Match
            if return == 0, no match was found
            if return > 0, match position is stored into *matchpos
*/


#if defined (__cplusplus)
}
#endif

#endif   /* MMC_H */
