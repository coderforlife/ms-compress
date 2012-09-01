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

static void lzx_compress_translate_block(const const_bytes start, bytes buf, const const_bytes end) // const int32_t translation_size
{
	while ((buf = (bytes)memchr(buf, 0xE8, end - buf)) != NULL)
	{
		int32_t pos = (int32_t)(buf++ - start);
		int32_t relValue = GET_UINT32(buf);
		if (relValue >= -pos && relValue < 12000000) // translation_size
		{
			uint32_t absValue = relValue > 0 ? relValue - 12000000 : relValue + pos; // translation_size
			SET_UINT32(buf, absValue);
		}
		buf += 4;
	}
}

static void lzx_compress_translate(bytes buf, size_t len) // const int32_t translation_size
{
	const const_bytes start = buf;
	if (len > 0x40000000) { len = 0x40000000; }
	while (len > (1 << 15))
	{
		lzx_compress_translate_block(start, buf, buf + 0x8000 - 6); // translation_size
		len -= 1 << 15;
		buf += 1 << 15;
	}
	if (len >= 6)
		lzx_compress_translate_block(start, buf, buf + len - 6); // translation_size
}


size_t lzx_compress_core(const_bytes in, size_t in_len, bytes out, size_t out_len, bool wimMode, uint32_t numPosLenSlots)
{
	if (out_len < 3) { return 0; }
	
	OutputBitstream bits(out, out_len);

	//int32_t translation_size = 12000000;
	if (!wimMode)
	{
		if (!bits.WriteBit(1) || !bits.WriteUInt32(12000000)) { return 0; }; // translation_size
	}

	//// Write header
	//if (!bits.WriteBits(kBlockTypeUncompressed, kNumBlockTypeBits)) { return 0; }
	//if (wimMode)
	//{
	//	if (in_len == 0x8000)
	//	{
	//		if (!bits.WriteBit(1)) { return 0; }
	//	}
	//	else if (!bits.WriteBit(0) || !bits.WriteBits((uint32_t)in_len, 16)) { return 0; }
	//}
	//else if (!bits.WriteManyBits(SIZE, kUncompressedBlockSizeNumBits)) { return 0; }

	//// Write block
	//if (out_len < in_len + 20) { return 0; }
	//memcpy(out + 20, in, in_len);
	//return in_len;

	return 0;
}
