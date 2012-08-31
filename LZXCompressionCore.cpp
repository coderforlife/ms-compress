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

#include "LZXCompressionCore.h"

#include "Bitstream.h"
#include "HuffmanEncoder.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

static void lzx_compress_translate(bytes in, size_t in_len, int32_t translation_size)
{
	if (in_len >= 6)
	{
		const_bytes start = in, end = in + MIN(in_len, 0x3FFFFFFF) - 6;
		while ((in = (bytes)memchr(in, 0xE8, end - in)) != NULL)
		{
			int32_t pos = (int32_t)(in++ - start);
			int32_t relValue = GET_UINT32(in);
			if (relValue >= -pos && relValue < translation_size)
			{
				uint32_t absValue = relValue > 0 ? relValue - translation_size : relValue + pos;
				SET_UINT32(in, absValue);
			}
			in += 4;
		}
	}
}

size_t lzx_compress_core(const_bytes in, size_t in_len, bytes out, size_t out_len, bool wimMode, uint32_t numPosLenSlots)
{

}
