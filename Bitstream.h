// ms-compress: implements Microsoft compression algorithms
// Copyright (C) 2012  Jeffrey Bush  jeff@coderforlife.com
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


////////////////////////////// Bitstreams //////////////////////////////////////////////////////////
// A bitstream that allows either reading or writing, but not both at the same time.
// It reads uint16s for bits and 16 bits can be reliably read at a time

#ifndef BITSTREAM_H
#define BITSTREAM_H
#include "compression-api.h"

EXTERN_C_START

struct _Bitstream
{
	union
	{
		const_bytes in;	// Reading only: The input byte array
		bytes out;		// Writing only: The output byte array
	} data;
	size_t index, len;	// The current position and length of the stream
	uint32_t mask;		// The next bits to be read/written in the bitstream
	byte bits;			// The number of bits in mask that are valid
	uint16_t* pntr[2];	// Writing only: the uint16's to write the data in mask to when there are enough bits
};
typedef struct _Bitstream InputBitstream;
typedef struct _Bitstream OutputBitstream;


// Reading functions:
void BSReadInit(InputBitstream* bstr, const_bytes in, size_t len);
uint32_t BSPeek(const InputBitstream* bstr, byte n);
void BSSkip(InputBitstream* bstr, byte n);
byte BSReadBit(InputBitstream* bstr);
uint32_t BSReadBits(InputBitstream* bstr, byte n);


// Writing functions:
void BSWriteInit(OutputBitstream* bstr, bytes out, size_t len);
bool BSWriteBits(OutputBitstream* bstr, uint32_t b, byte n);
bool BSWriteByte(OutputBitstream* bstr, byte b);
void BSWriteFinish(OutputBitstream* bstr);


EXTERN_C_END

#endif
