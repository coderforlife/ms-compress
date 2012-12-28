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


#include "xpress_huff.h"

#include "XpressDictionary.h"
#include "Bitstream.h"
#include "HuffmanDecoder.h"
#include "HuffmanEncoder.h"

#ifdef VERBOSE_DECOMPRESSION
#include <stdio.h>
#include <ctype.h>
#endif


////////////////////////////// General Definitions and Functions ///////////////////////////////////
#define MAX_OFFSET		0xFFFF
#define CHUNK_SIZE		0x10000

#define STREAM_END		0x100
#define STREAM_END_LEN_1	1
//#define STREAM_END_LEN_1	1<<4 // if STREAM_END&1

#define SYMBOLS			0x200
#define HALF_SYMBOLS	0x100

#define MIN_DATA		HALF_SYMBOLS + 4 // the 512 Huffman lens + 2 uint16s for minimal bitstream

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

typedef XpressDictionary<MAX_OFFSET, CHUNK_SIZE> Dictionary;
typedef HuffmanEncoder<15, SYMBOLS> Encoder;
typedef HuffmanDecoder<15, SYMBOLS> Decoder;


////////////////////////////// Compression Functions ///////////////////////////////////////////////
static const byte Log2Table[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
	/*-1*/0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
	LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};
inline static byte highbit(uint32_t x) { uint_fast16_t y = x >> 8; return y ? 8 + Log2Table[y] : Log2Table[x]; } // returns 0x0 - 0xF
WARNINGS_PUSH()
WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()
static size_t xh_compress_lz77(const_bytes in, int32_t /* * */ in_len, const_bytes in_end, bytes out, uint32_t symbol_counts[], Dictionary* d)
{
	int32_t rem = /* * */ in_len;
	uint32_t mask;
	const const_bytes in_orig = in, out_orig = out;
	uint32_t* mask_out;
	byte i;
	
	d->Fill(in);
	memset(symbol_counts, 0, SYMBOLS*sizeof(uint32_t));

	////////// Count the symbols and write the initial LZ77 compressed data //////////
	// A uint32 mask holds the status of each subsequent byte (0 for literal, 1 for offset / length)
	// Literals are stored using a single byte for their value
	// Offset / length pairs are stored in the following manner:
	//   Offset: a uint16
	//   Length: for length-3:
	//     0x0000 <= length <  0x000000FF  length-3 as byte
	//     0x00FF <= length <= 0x0000FFFF  0xFF + length-3 as uint16
	//     0xFFFF <  length <= 0xFFFFFFFF  0xFF + 0x0000 + length-3 as uint32
	// The number of bytes between uint32 masks is >=32 and <=160 (5*32)
	//   with the exception that the a length > 0x10002 could be found, but this is longer than a chunk and would immediately end the chunk
	//   if it is the last one, then we need 4 additional bytes, but we don't have to take it into account in any other way
	while (rem > 0)
	{
		mask = 0;
		mask_out = (uint32_t*)out;
		out += sizeof(uint32_t);

		// Go through each bit
		for (i = 0; i < 32 && rem > 0; ++i)
		{
			uint32_t len, off;
			mask >>= 1;
			//d->Add(in);
			if (rem >= 3 && (len = d->Find(in, &off)) >= 3)
			{
				// TODO: allow len > rem
				if (len > rem) { len = rem; }
				
				//d->Add(in + 1, len - 1);

				// Write offset / length
				*(uint16_t*)out = (uint16_t)off;
				out += 2;
				in += len;
				rem -= len;
				len -= 3;
				if (len > 0xFFFF) { *out = 0xFF; *(uint16_t*)(out+1) = 0; *(uint32_t*)(out+3) = len; out += 7; }
				if (len >= 0xFF)  { *out = 0xFF; *(uint16_t*)(out+1) = (uint16_t)len; out += 3; }
				else              { *out = (byte)len; ++out; }
				mask |= 0x80000000; // set the highest bit

				// Create a symbol from the offset and length
				++symbol_counts[(highbit(off) << 4) | MIN(0xF, len) | 0x100];
			}
			else
			{
				// Write the literal value (which is the symbol)
				++symbol_counts[*out++ = *in++];
				--rem;
			}
		}

		// Save mask
		*mask_out = mask;
	}
	
	// Set the total number of bytes read from in
	/* *in_len -= rem; */
	mask >>= (32-i); // finish moving the value over
	if (in_orig+ /* * */ in_len == in_end)
	{
		// Add the end of stream symbol
		if (i == 32)
		{
			// Need to add a new mask since the old one is full with just one bit set
			*(uint32_t*)out = 1;
			out += 4;
		}
		else
		{
			// Add to the old mask
			mask |= 1 << i; // set the highest bit
		}
		memset(out, 0, 3);
		out += 3;
		++symbol_counts[STREAM_END];
	}
	*mask_out = mask;

	// Return the number of bytes in the output
	return out - out_orig;
}
WARNINGS_POP()
static const uint16_t OffsetMasks[16] = // (1 << O) - 1
{
	0x0000, 0x0001, 0x0003, 0x0007,
	0x000F, 0x001F, 0x003F, 0x007F,
	0x00FF, 0x01FF, 0x03FF, 0x07FF,
	0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
};
static size_t xh_compress_encode(const_bytes in, size_t in_len, bytes out, size_t out_len, Encoder *encoder)
{
	uint_fast16_t i;
	ptrdiff_t rem = (ptrdiff_t)in_len;
	uint32_t mask;
	const_bytes end;
	OutputBitstream bstr(out, out_len);

	// Write the encoded compressed data
	// This involves parsing the LZ77 compressed data and re-writing it with the Huffman code
	while (rem > 0)
	{
		// Handle a fragment
		// Bit mask tells us how to handle the next 32 symbols, go through each bit
		for (i = 32, mask = *(uint32_t*)in, in += 4, rem -= 4; mask && rem > 0; --i, mask >>= 1)
		{
			if (mask & 1) // offset / length symbol
			{
				uint_fast16_t off, sym;
				uint32_t len;
				byte O;

				// Get the LZ77 offset and length
				off = *(uint16_t*)in;
				len = in[2];
				in += 3; rem -= 3;
				if (len == 0xFF)
				{
					len = *(uint16_t*)in;
					in += 2; rem -= 2;
					if (len == 0x0000)
					{
						len = *(uint32_t*)in;
						in += 4; rem -= 4;
					}
				}

				// Write the Huffman code then extra offset bits and length bytes
				O = highbit(off);
				// len is already -= 3
				off &= OffsetMasks[O]; // (1 << O) - 1)
				sym = (uint_fast16_t)((O << 4) | MIN(0xF, len) | 0x100);
				if (!encoder->EncodeSymbol(sym, &bstr))						{ break; }
				if (len >= 0xF)
				{
					if (len >= 0xFF + 0xF)
					{
						if (!bstr.WriteRawByte(0xFF))						{ break; }
						if (len > 0xFFFF)
						{
							if (!bstr.WriteRawUInt16(0x0000) || !bstr.WriteRawUInt32(len))	{ break; }
						}
						else if (!bstr.WriteRawUInt16((uint16_t)len))		{ break; }
					}
					else if (!bstr.WriteRawByte((byte)(len - 0xF)))			{ break; }
				}
				if (!bstr.WriteBits(off, O))								{ break; }
			}
			else
			{
				// Write the literal symbol
				if (!encoder->EncodeSymbol(*in, &bstr))						{ break; }
				++in; --rem;
			}
		}
		if (rem < 0) { break; }
		if (rem < i) { i = (byte)rem; }

		// Write the remaining literal symbols
		for (end = in+i; in != end && encoder->EncodeSymbol(*in, &bstr); ++in);
		if (in != end)														{ break; }
		rem -= i;
	}

	// Write end of stream symbol and return insufficient buffer or the compressed size
	if (rem > 0) { PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	return bstr.Finish(); // make sure that the write stream is finished writing
}
size_t xpress_huff_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if (in_len == 0) { return 0; }

	bytes buf = (bytes)malloc((in_len >= CHUNK_SIZE) ? 0x1200B : ((in_len + 31) / 32 * 36 + 4 + 7)); // for every 32 bytes in "in" we need up to 36 bytes in the temp buffer + maybe an extra uint32 length symbol + up to 7 for the EOS
	if (buf == NULL) { return 0;  } // errno already set

	const bytes out_orig = out;
	const const_bytes in_end = in+in_len;
	Dictionary d(in, in_end);
	Encoder encoder;

	// Go through each chunk except the last
	while (in_len > CHUNK_SIZE)
	{
		if (out_len < MIN_DATA) { PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; free(buf); return 0; }

		////////// Perform the initial LZ77 compression //////////
		uint32_t symbol_counts[SYMBOLS]; // 4*512 = 2 kb
		size_t buf_len = xh_compress_lz77(in, CHUNK_SIZE, in_end, buf, symbol_counts, &d);
		//if (buf_len == 0) { free(buf); return 0; } // errno already set
	
		////////// Create the Huffman codes/lens and write the Huffman prefix codes as lengths //////////
		const_bytes lens = encoder.CreateCodes(symbol_counts);
		if (lens == NULL) { free(buf); return 0; } // errno already set
		for (uint_fast16_t i = 0, i2 = 0; i < HALF_SYMBOLS; ++i, i2+=2) { out[i] = (lens[i2+1] << 4) | lens[i2]; }

		////////// Encode compressed data //////////
		size_t done = xh_compress_encode(buf, buf_len, out+=HALF_SYMBOLS, out_len-=HALF_SYMBOLS, &encoder);
		if (done == 0) { free(buf); return 0; } // errno already set

		// Update all the positions and lengths
		in     += CHUNK_SIZE; out     += done;
		in_len -= CHUNK_SIZE; out_len -= done;
	}

	// Do the last chunk
	if (out_len < MIN_DATA) { PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; out = out_orig; }
	else if (in_len == 0) // implies end_of_stream
	{
		memset(out, 0, MIN_DATA);
		out[STREAM_END>>1] = STREAM_END_LEN_1;
		out += MIN_DATA;
	}
	else
	{
		////////// Perform the initial LZ77 compression //////////
		uint32_t symbol_counts[SYMBOLS]; // 4*512 = 2 kb
		size_t buf_len = xh_compress_lz77(in, (int32_t)in_len, in_end, buf, symbol_counts, &d);
		//if (buf_len == 0) { free(buf); return 0; } // errno already set

		////////// Create the Huffman codes/lens and write the Huffman prefix codes as lengths //////////
		const_bytes lens = encoder.CreateCodes(symbol_counts);
		if (lens == NULL) { free(buf); return 0; } // errno already set
		for (uint_fast16_t i = 0, i2 = 0; i < HALF_SYMBOLS; ++i, i2+=2) { out[i] = (lens[i2+1] << 4) | lens[i2]; }

		////////// Encode compressed data //////////
		size_t done = xh_compress_encode(buf, buf_len, out+=HALF_SYMBOLS, out_len-=HALF_SYMBOLS, &encoder);
		if (done == 0) { out = out_orig; } // errno already set
		out += done;
	}

	// Cleanup
	free(buf);

	// Return the total number of compressed bytes
	return out - out_orig;
}


////////////////////////////// Decompression Functions /////////////////////////////////////////////
static bool xpress_huff_decompress_chunk(const_bytes in, size_t in_len, size_t* in_pos, bytes out, size_t out_len, size_t* out_pos, const const_bytes out_origin, bool* end_of_stream, Decoder *decoder)
{
#ifdef VERBOSE_DECOMPRESSION
	bool last_literal = false;
#endif
	size_t i = 0;
	InputBitstream bstr(in, in_len);
	do
	{
		uint_fast16_t sym = decoder->DecodeSymbol(&bstr);
		if (sym == INVALID_SYMBOL)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); errno = E_INVALID_DATA; return false; }
		else if (sym == STREAM_END && bstr.RemainingRawBytes() == 0 && bstr.MaskIsZero()) { *end_of_stream = true; break; }
		else if (sym < 0x100)
		{
			if (i == out_len)												{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return false; }
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
#ifdef PRINT_ERRORS
			if (off == 0xFFFFFFFF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read %u bits for offset\n", sym); errno = E_INVALID_DATA; return false; }
			if ((out+i-(off+=1<<sym)) < out_origin)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out+i, off, out_origin); errno = E_INVALID_DATA; return false; }
#else
			if (off == 0xFFFFFFFF || (out+i-(off+=1<<sym)) < out_origin)	{ errno = E_INVALID_DATA; return false; }
#endif
			if (len == 0xF)
			{
				if (bstr.RemainingRawBytes() < 1)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read extra byte for length\n"); errno = E_INVALID_DATA; return false; }
				else if ((len = bstr.ReadRawByte()) == 0xFF)
				{
					if (bstr.RemainingRawBytes() < 2)						{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read two bytes for length\n"); errno = E_INVALID_DATA; return false; }
					if ((len = bstr.ReadRawUInt16()) == 0)
					{
						if (bstr.RemainingRawBytes() < 4)					{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read four bytes for length\n"); errno = E_INVALID_DATA; return false; }
						len = bstr.ReadRawUInt32();
					}
					if (len < 0xF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Invalid length specified\n"); errno = E_INVALID_DATA; return false; }
					len -= 0xF;
				}
				len += 0xF;
			}
			len += 3;
			bstr.Skip((byte)sym);

			if (i + len > out_len)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return false; }

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
	return true;
}
size_t xpress_huff_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes out_start = out;
	size_t in_pos = 0, out_pos = 0;
	bool end_of_stream = false;
	Decoder decoder;
	byte codeLengths[SYMBOLS];
	do
	{
		if (in_len < MIN_DATA)
		{
			if (in_len) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); errno = E_INVALID_DATA; }
			return 0;
		}
		for (uint_fast16_t i = 0, i2 = 0; i < HALF_SYMBOLS; ++i)
		{
			codeLengths[i2++] = (in[i] & 0xF);
			codeLengths[i2++] = (in[i] >>  4);
		}
		if (!decoder.SetCodeLengths(codeLengths)) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Unable to resolve Huffman codes\n", MIN_DATA); errno = E_INVALID_DATA; return 0; }
		if (!xpress_huff_decompress_chunk(in+=HALF_SYMBOLS, in_len-=HALF_SYMBOLS, &in_pos, out, out_len, &out_pos, out_start, &end_of_stream, &decoder)) { return 0; } // errno already set
		in  += in_pos;  in_len  -= in_pos;
		out += out_pos; out_len -= out_pos;
	} while (!end_of_stream);
	return out-out_start;
}
