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

#ifdef VERBOSE_DECOMPRESSION
#include <stdio.h>
#include <ctype.h>
#endif

#define PRINT_ERROR(...) // TODO: remove

////////////////////////////// General Definitions and Functions ///////////////////////////////////
#define MAX_OFFSET		0xFFFF
#define CHUNK_SIZE		0x10000

#define STREAM_END		0x100
#define STREAM_END_LEN_1	1
//#define STREAM_END_LEN_1	1<<4 // if STREAM_END&1

#define SYMBOLS			0x200
#define HALF_SYMBOLS	0x100

#define MIN_DATA		HALF_SYMBOLS + 4 // the 512 Huffman lens + 2 uint16s for minimal bitstream

typedef HuffmanDecoder<15, SYMBOLS> Decoder;


////////////////////////////// Decompression Functions /////////////////////////////////////////////
static MSCompStatus xpress_huff_decompress_chunk(const_bytes in, size_t in_len, size_t* in_pos, bytes out, size_t out_len, size_t* out_pos, const const_bytes out_origin, bool* end_of_stream, Decoder *decoder)
{
#ifdef VERBOSE_DECOMPRESSION
	bool last_literal = false;
#endif
	size_t i = 0;
	InputBitstream bstr(in, in_len);
	do
	{
		uint_fast16_t sym = decoder->DecodeSymbol(&bstr);
		if (sym == INVALID_SYMBOL)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); return MSCOMP_DATA_ERROR; }
		else if (sym == STREAM_END && bstr.RemainingRawBytes() == 0 && bstr.MaskIsZero()) { *end_of_stream = true; break; }
		else if (sym < 0x100)
		{
			if (i == out_len)												{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); return MSCOMP_BUF_ERROR; }
#ifdef VERBOSE_DECOMPRESSION
			if (!last_literal)
			{
				printf("\nLiterals: ");
				last_literal = true;
			}
			if (isprint(sym)) { printf("%c", (char)sym); }
			else if (sym == '\0') { printf("\\0"); }
			else if (sym == '\a') { printf("\\a"); }
			else if (sym == '\b') { printf("\\b"); }
			else if (sym == '\t') { printf("\\t"); }
			else if (sym == '\n') { printf("\\n"); }
			else if (sym == '\v') { printf("\\v"); }
			else if (sym == '\f') { printf("\\f"); }
			else if (sym == '\r') { printf("\\r"); }
			else if (sym == '\\') { printf("\\\\"); }
			else { printf("\\x%02X", sym); }
#endif
			out[i++] = (byte)sym;
		}
		else
		{
			uint32_t len = sym & 0xF, off = bstr.Peek((byte)(sym = ((sym>>4) & 0xF)));
#ifdef MSCOMP_WITH_ERROR_MESSAGES
			if (off == 0xFFFFFFFF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read %u bits for offset\n", sym); return MSCOMP_DATA_ERROR; }
			if ((out+i-(off+=1<<sym)) < out_origin)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out+i, off, out_origin); return MSCOMP_DATA_ERROR; }
#else
			if (off == 0xFFFFFFFF || (out+i-(off+=1<<sym)) < out_origin)	{ return MSCOMP_DATA_ERROR; }
#endif
			if (len == 0xF)
			{
				if (bstr.RemainingRawBytes() < 1)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read extra byte for length\n"); return MSCOMP_DATA_ERROR; }
				else if ((len = bstr.ReadRawByte()) == 0xFF)
				{
					if (bstr.RemainingRawBytes() < 2)						{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read two bytes for length\n"); return MSCOMP_DATA_ERROR; }
					if ((len = bstr.ReadRawUInt16()) == 0)
					{
						if (bstr.RemainingRawBytes() < 4)					{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read four bytes for length\n"); return MSCOMP_DATA_ERROR; }
						len = bstr.ReadRawUInt32();
					}
					if (len < 0xF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Invalid length specified\n"); return MSCOMP_DATA_ERROR; }
					len -= 0xF;
				}
				len += 0xF;
			}
			len += 3;
			bstr.Skip((byte)sym);

			if (i + len > out_len)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); return MSCOMP_BUF_ERROR; }

#ifdef VERBOSE_DECOMPRESSION
			printf("\nMatch: %d @ %d", len, off);
			last_literal = false;
#endif

			if (off == 1)
			{
				memset(out+i, out[i-1], len);
				i += len;
			}
			else
			{
				size_t end;
				for (end = i + len; i < end; ++i) { out[i] = out[i-off]; }
			}
		}
	} while (i < CHUNK_SIZE || !bstr.MaskIsZero()); /* end of chunk, not stream */
	*in_pos = bstr.RawPosition();
	if (!*end_of_stream && decoder->DecodeSymbol(&bstr) == STREAM_END && bstr.RemainingRawBytes() == 0 && bstr.MaskIsZero())
	{
		*in_pos = bstr.RawPosition();
		*end_of_stream = true;
	}
	*out_pos = i;

#ifdef VERBOSE_DECOMPRESSION
	printf("\n");
#endif
	return MSCOMP_OK;
}
MSCompStatus xpress_huff_decompress(const_bytes in, size_t in_len, bytes out, size_t* _out_len)
{
	const const_bytes out_start = out;
	size_t in_pos = 0, out_pos = 0, out_len = *_out_len;
	bool end_of_stream = false;
	Decoder decoder;
	byte codeLengths[SYMBOLS];
	do
	{
		if (in_len < MIN_DATA)
		{
			if (in_len) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); return MSCOMP_DATA_ERROR; }
			return MSCOMP_OK;
		}
		for (uint_fast16_t i = 0, i2 = 0; i < HALF_SYMBOLS; ++i)
		{
			codeLengths[i2++] = (in[i] & 0xF);
			codeLengths[i2++] = (in[i] >>  4);
		}
		if (!decoder.SetCodeLengths(codeLengths)) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Unable to resolve Huffman codes\n", MIN_DATA); return MSCOMP_DATA_ERROR; }
		MSCompStatus err = xpress_huff_decompress_chunk(in+=HALF_SYMBOLS, in_len-=HALF_SYMBOLS, &in_pos, out, out_len, &out_pos, out_start, &end_of_stream, &decoder);
		if (err != MSCOMP_OK) { return err; }
		in  += in_pos;  in_len  -= in_pos;
		out += out_pos; out_len -= out_pos;
	} while (!end_of_stream);
	*_out_len = out-out_start;
	return MSCOMP_OK;
}

#endif
