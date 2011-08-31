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


#if defined (__cplusplus)
extern "C" {
#endif


//****************************
// Simple Functions
//****************************

int LZ4_compressHC (char* source, char* dest, int isize);
int LZ4_uncompress (char* source, char* dest, int osize);

/*
LZ4_compressHC :
	return : the number of bytes in compressed buffer dest
	note : destination buffer must be already allocated. 
		To avoid any problem, size it to handle worst cases situations (input data not compressible)
		Worst case size is : "inputsize + 0.4%", with "0.4%" being at least 8 bytes.

LZ4_uncompress :
	return : the number of bytes read in the source buffer
			 If the source stream is malformed, the function will stop decoding and return a negative result, indicating the byte position of the faulty instruction
			 This version never writes beyond dest + osize, and is therefore protected against malicious data packets
	note 1 : osize is the output size, therefore the original size
	note 2 : destination buffer must be already allocated
*/


//****************************
// Advanced Functions
//****************************

int LZ4_uncompress_unknownOutputSize (char* source, char* dest, int isize, int maxOutputSize);

/*
LZ4_uncompress_unknownOutputSize :
	return : the number of bytes decoded in the destination buffer (necessarily <= maxOutputSize)
			 If the source stream is malformed, the function will stop decoding and return a negative result, indicating the byte position of the faulty instruction
			 This version never writes beyond dest + maxOutputSize, and is therefore protected against malicious data packets
	note 1 : isize is the input size, therefore the compressed size
	note 2 : destination buffer must already be allocated, with at least maxOutputSize bytes
	note 3 : this version is slower by up to 10%, and is therefore not recommended for general use
*/




#if defined (__cplusplus)
}
#endif
