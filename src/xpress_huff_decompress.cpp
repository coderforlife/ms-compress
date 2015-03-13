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


#include "../include/mscomp/internal.h"

#ifdef MSCOMP_WITH_XPRESS_HUFF

#include "../include/xpress_huff.h"
#include "../include/mscomp/Bitstream.h"
#include "../include/mscomp/HuffmanDecoder.h"

#define PRINT_ERROR(...) // TODO: remove

////////////////////////////// General Definitions and Functions ///////////////////////////////////
#define MAX_OFFSET		0xFFFF
#define CHUNK_SIZE		0x10000

#define STREAM_END		0x100
#define STREAM_END_LEN_1	1
//#define STREAM_END_LEN_1	1<<4 // if STREAM_END&1

#define SYMBOLS			0x200
#define HALF_SYMBOLS	0x100
#define HUFF_BITS_MAX	15

#define MIN_DATA		HALF_SYMBOLS + 4 // the 512 Huffman lens + 2 uint16s for minimal bitstream

typedef HuffmanDecoder<HUFF_BITS_MAX, SYMBOLS> Decoder;


////////////////////////////// Decompression Functions /////////////////////////////////////////////
static MSCompStatus xpress_huff_decompress_chunk(const_bytes in, const size_t in_len, size_t* in_pos, bytes out, const size_t out_len, size_t* out_pos, const const_bytes out_origin, bool* end_of_stream, Decoder *decoder)
{
//	const const_bytes                  in_endx  = in  + in_len  - 10; // 6 bytes for the up-to 30 bits we may need (along with the 16-bit alignments the bitstream does) + 1 for an extra length byte + some extra for good measure
	const const_bytes out_start = out, out_end = out + out_len, out_endx = out_end - FAST_COPY_ROOM, out_end_chunk = out + CHUNK_SIZE, out_endx_chunk = out_end_chunk - FAST_COPY_ROOM;
	InputBitstream bstr(in, in_len);

	uint_fast16_t sym;
	uint32_t len, off;
	uint_fast8_t off_bits;

	// Fast decompression - minimal bounds checking
	while (LIKELY(bstr.RemainingRawBytes() >= 7 && out < MIN(out_endx_chunk, out_endx)))
	{
		sym = decoder->DecodeSymbolFast(&bstr);
		if (UNLIKELY(sym == INVALID_SYMBOL))						{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); return MSCOMP_DATA_ERROR; }
		else if (sym < 0x100) { *out++ = (byte)sym; }
		else
		{
			len = sym & 0xF;
			off_bits = (uint_fast8_t)((sym>>4) & 0xF);
			off = bstr.Peek(off_bits) + (1 << off_bits);
			if (len == 0xF)
			{
				if ((len = bstr.ReadRawByte()) == 0xFF)
				{
					if (UNLIKELY(bstr.RemainingRawBytes() < 7+6)) { goto CHECKED_LENGTH; }
					if (UNLIKELY((len = bstr.ReadRawUInt16()) == 0)) { len = bstr.ReadRawUInt32(); }
					if (UNLIKELY(len < 0xF))						{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Invalid length specified\n"); return MSCOMP_DATA_ERROR; }
					len -= 0xF;
				}
				len += 0xF;
			}
			len += 3;
			bstr.Skip(off_bits);
			const const_bytes o = out-off;
			if (UNLIKELY(o < out_origin))							{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Invalid offset"); return MSCOMP_DATA_ERROR; }
			FAST_COPY(out, o, len, off, out_endx,
				if (UNLIKELY(out + len > out_end)) { return MSCOMP_BUF_ERROR; }
				goto CHECKED_COPY);
		}
	}

	// Slow decompression - full bounds checking
	while (out < out_end_chunk || !bstr.MaskIsZero()) /* end of chunk, not stream */
	{
		sym = decoder->DecodeSymbol(&bstr);
		if (UNLIKELY(sym == INVALID_SYMBOL))						{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); return MSCOMP_DATA_ERROR; }
		else if (sym == STREAM_END && bstr.RemainingRawBytes() == 0 && bstr.MaskIsZero()) { *end_of_stream = true; break; }
		else if (sym < 0x100)
		{
			if (UNLIKELY(out == out_end))							{ PRINT_ERROR("XPRESS Huffman Decompression Error: Insufficient buffer\n"); return MSCOMP_BUF_ERROR; }
			*out++ = (byte)sym;
		}
		else
		{
			len = sym & 0xF;
			off_bits = (uint_fast8_t)((sym>>4) & 0xF);
			if (UNLIKELY(off_bits > bstr.RemainingBits()))			{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read %u bits for offset\n", sym); return MSCOMP_DATA_ERROR; }
			off = bstr.Peek(off_bits) + (1 << off_bits);
			if (len == 0xF)
			{
				if (UNLIKELY(bstr.RemainingRawBytes() < 1))			{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read extra byte for length\n"); return MSCOMP_DATA_ERROR; }
				else if ((len = bstr.ReadRawByte()) == 0xFF)
				{
CHECKED_LENGTH:		if (UNLIKELY(bstr.RemainingRawBytes() < 2))		{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read two bytes for length\n"); return MSCOMP_DATA_ERROR; }
					if (UNLIKELY((len = bstr.ReadRawUInt16()) == 0))
					{
						if (UNLIKELY(bstr.RemainingRawBytes() < 4))	{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Unable to read four bytes for length\n"); return MSCOMP_DATA_ERROR; }
						len = bstr.ReadRawUInt32();
					}
					if (UNLIKELY(len < 0xF))						{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Invalid length specified\n"); return MSCOMP_DATA_ERROR; }
					len -= 0xF;
				}
				len += 0xF;
			}
			len += 3;
			bstr.Skip(off_bits);
			if (UNLIKELY((out-off) < out_origin))					{ PRINT_ERROR("XPRESS Huffman Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out+i, off, out_origin); return MSCOMP_DATA_ERROR; }
			if (out + len > out_end)								{ PRINT_ERROR("XPRESS Huffman Decompression Error: Insufficient buffer\n"); return MSCOMP_BUF_ERROR; }
			if (off == 1)
			{
				memset(out, out[-1], len);
				out += len;
			}
			else
			{
				const_bytes end;
CHECKED_COPY:	for (end = out + len; out < end; ++out) { *out = *(out-off); }
			}
		}
	}
	*in_pos = bstr.RawPosition();
	if (!*end_of_stream && decoder->DecodeSymbol(&bstr) == STREAM_END && bstr.RemainingRawBytes() == 0 && bstr.MaskIsZero())
	{
		*in_pos = bstr.RawPosition();
		*end_of_stream = true;
	}
	*out_pos = out - out_start;

	return MSCOMP_OK;
}
MSCompStatus xpress_huff_decompress(const_bytes in, size_t in_len, bytes out, size_t* _out_len)
{
	const const_bytes out_start = out;
	size_t in_pos = 0, out_pos = 0, out_len = *_out_len;
	bool end_of_stream = false;
	Decoder decoder;
	byte code_lengths[SYMBOLS];
	do
	{
		if (UNLIKELY(in_len < MIN_DATA))
		{
			if (in_len) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); return MSCOMP_DATA_ERROR; }
			return MSCOMP_OK;
		}
		for (uint_fast16_t i = 0, i2 = 0; i < HALF_SYMBOLS; ++i)
		{
			code_lengths[i2++] = (in[i] & 0xF);
			code_lengths[i2++] = (in[i] >>  4);
		}
		if (UNLIKELY(!decoder.SetCodeLengths(code_lengths))) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Unable to resolve Huffman codes\n"); return MSCOMP_DATA_ERROR; }
		MSCompStatus err = xpress_huff_decompress_chunk(in+=HALF_SYMBOLS, in_len-=HALF_SYMBOLS, &in_pos, out, out_len, &out_pos, out_start, &end_of_stream, &decoder);
		if (UNLIKELY(err != MSCOMP_OK)) { return err; }
		in  += in_pos;  in_len  -= in_pos;
		out += out_pos; out_len -= out_pos;
	} while (!end_of_stream);
	*_out_len = out-out_start;
	return MSCOMP_OK;
}

#endif
