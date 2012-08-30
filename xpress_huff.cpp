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

//#include "LZNT1Dictionary.h"
#include "XpressDictionary.h"
#include "Bitstream.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif


////////////////////////////// General Definitions and Functions ///////////////////////////////////
#define MAX_OFFSET		0xFFFF
#define CHUNK_SIZE		0x10000

#define STREAM_END		0x100
#define STREAM_END_LEN_1	1
//#define STREAM_END_LEN_1	1<<4 // if STREAM_END&1

#define SYMBOLS			0x200
#define HALF_SYMBOLS	0x100
#define INVALID			0xFFFF

#define MIN_DATA		HALF_SYMBOLS + 4 // the 512 Huffman lens + 2 uint16s for minimal bitstream

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

typedef XpressDictionary<MAX_OFFSET, CHUNK_SIZE> Dictionary;

// Merge-sorts syms[l, r) using conditions[syms[x]]
// Use merge-sort so that it is stable, keeping symbols in increasing order
template<typename T> // T is either uint32_t or byte
static void msort(uint16_t* syms, uint16_t* temp, T* conditions, uint_fast16_t l, uint_fast16_t r)
{
	uint_fast16_t len = r - l;
	if (len <= 1) { return; }
	
	// Not required to do these special in-place sorts, but is a bit more efficient
	else if (len == 2)
	{
		if (conditions[syms[l+1]] < conditions[syms[ l ]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; }
		return;
	}
	else if (len == 3)
	{
		if (conditions[syms[l+1]] < conditions[syms[ l ]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; }
		if (conditions[syms[l+2]] < conditions[syms[l+1]]) { uint16_t t = syms[l+2]; syms[l+2] = syms[l+1]; syms[l+1] = t;
			if (conditions[syms[l+1]]<conditions[syms[l]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; } }
		return;
	}
	
	// Merge-Sort
	else
	{
		uint_fast16_t m = l + (len >> 1), i = l, j = l, k = m;
		
		// Divide and Conquer
		msort(syms, temp, conditions, l, m);
		msort(syms, temp, conditions, m, r);
		memcpy(temp+l, syms+l, len*sizeof(uint16_t));
		
		// Merge
		while (j < m && k < r) syms[i++] = (conditions[temp[k]] < conditions[temp[j]]) ? temp[k++] : temp[j++]; // if == then does j which is from the lower half, keeping stable
			 if (j < m) memcpy(syms+i, temp+j, (m-j)*sizeof(uint16_t));
		else if (k < r) memcpy(syms+i, temp+k, (r-k)*sizeof(uint16_t));
	}
}

////////////////////////////// Compression Functions ///////////////////////////////////////////////
static const byte Log2Table[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
	/*-1*/0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
	LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};
static const uint16_t OffsetMasks[16] =
{
	0x0000, 0x0001, 0x0003, 0x0007,
	0x000F, 0x001F, 0x003F, 0x007F,
	0x00FF, 0x01FF, 0x03FF, 0x07FF,
	0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
};
inline static byte highbit(uint32_t x) { uint_fast16_t y = x >> 8; return y ? 8 + Log2Table[y] : Log2Table[x]; } // returns 0x0 - 0xF
static size_t xh_lz77_compress(const_bytes in, int32_t* in_len, const_bytes in_end, bytes out, uint32_t symbol_counts[], Dictionary* d)
{
	int32_t rem = *in_len;
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
			if (rem >= 3 && (len = d->Find(in, &off)) >= 3)
			{
				// TODO: allow len > rem
				if (len > rem) { len = rem; }

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
				++symbol_counts[*out = *in];
				++out;
				++in;
				--rem;
			}
		}

		// Save mask
		*mask_out = mask;
	}
	
	// Set the total number of bytes read from in
	*in_len -= rem;
	mask >>= (32-i); // finish moving the value over
	if (in_orig+*in_len == in_end)
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
static bool xh_create_codes(uint32_t symbol_counts[], uint16_t huffman_codes[], byte huffman_lens[]) // 3 kb stack
{
	uint16_t* syms, syms_by_count[SYMBOLS], syms_by_len[SYMBOLS], temp[SYMBOLS]; // 3*2*512 = 3 kb
	uint_fast16_t i, j, len, pos, s;

	memset(huffman_codes, 0, SYMBOLS*sizeof(uint16_t));
	memset(huffman_lens,  0, SYMBOLS*sizeof(byte));

	// Fill the syms_by_count, syms_by_length, and huffman_lens with the symbols that were found
	for (i = 0, len = 0; i < SYMBOLS; ++i) { if (symbol_counts[i]) { syms_by_count[len] = (uint16_t)i; syms_by_len[len++] = (uint16_t)i; huffman_lens[i] = 0xF; } }


	////////// Get the Huffman lengths //////////
	if (len == 1)
	{
		huffman_lens[syms_by_count[0]] = 1; // never going to happen, but the code below would probably assign a length of 0 which is not right
	}
	else
	{
		///// Package-Merge Algorithm /////
		typedef struct _collection // 516 bytes each
		{
			uint_fast16_t count;
			byte symbols[SYMBOLS];
		} collection;
		collection* cols = (collection*)malloc(32*sizeof(collection)), *next_cols = (collection*)malloc(32*sizeof(collection)), *temp_cols; // 32.25 kb initial allocation
		uint_fast16_t cols_cap = 32, cols_len = 0, cols_pos, next_cols_len = 0;
		
		if (!cols || !next_cols) { PRINT_ERROR("Xpress Huffman Compression Error: malloc failed\n"); free(cols); free(next_cols); return false; }

		msort(syms = syms_by_count, temp, symbol_counts, 0, len); // sort by the counts

		// Start at the lowest value row, adding new collection
		for (j = 0; j < 0xF; ++j)
		{
			cols_pos = 0;
			pos = 0;

			// All but the last one/none get added to collections
			while ((cols_len-cols_pos + len-pos) > 1)
			{
				if (cols_cap == next_cols_len)
				{
					cols_cap <<= 1;

					temp_cols = (collection*)realloc(cols,      cols_cap*sizeof(collection));
					if (temp_cols == NULL) { PRINT_ERROR("Xpress Huffman Compression Error: realloc failed\n"); free(cols); free(next_cols); return false; }
					cols      = temp_cols;

					temp_cols = (collection*)realloc(next_cols, cols_cap*sizeof(collection));
					if (temp_cols == NULL) { PRINT_ERROR("Xpress Huffman Compression Error: realloc failed\n"); free(cols); free(next_cols); return false; }
					next_cols = temp_cols;
				}
				memset(next_cols+next_cols_len, 0, sizeof(collection));
				for (i = 0; i < 2; ++i) // hopefully unrolled...
				{
					if (pos >= len || (cols_pos < cols_len && cols[cols_pos].count < symbol_counts[syms[pos]]))
					{
						// Add cols[cols_pos]
						next_cols[next_cols_len].count += cols[cols_pos].count;
						for (s = 0; s < SYMBOLS; ++s)
							next_cols[next_cols_len].symbols[s] += cols[cols_pos].symbols[s];
						++cols_pos;
					}
					else
					{
						// Add syms[pos]
						next_cols[next_cols_len].count += symbol_counts[syms[pos]];
						++next_cols[next_cols_len].symbols[syms[pos]];
						++pos;
					}
				}
				++next_cols_len;
			}
			
			// Left over gets dropped
			if (cols_pos < cols_len)
				for (s = 0; s < SYMBOLS; ++s)
					huffman_lens[s] -= cols[cols_pos].symbols[s];
			else if (pos < len)
				--huffman_lens[syms[pos]];

			// Move the next_collections to the current collections
			temp_cols = cols; cols = next_cols; next_cols = temp_cols;
			cols_len = next_cols_len;
			next_cols_len = 0;
		}
		free(cols);
		free(next_cols);


		////////// Create Huffman codes from lengths //////////
		msort(syms = syms_by_len, temp, huffman_lens, 0, len); // Sort by the code lengths
		for (i = 1; i < len; ++i)
		{
			// Code is previous code +1 with added zeroes for increased code length
			huffman_codes[syms[i]] = (huffman_codes[syms[i-1]] + 1) << (huffman_lens[syms[i]] - huffman_lens[syms[i-1]]);
		}
	}
	return true;
}
static size_t xh_encode(const_bytes in, size_t in_len, bytes out, size_t out_len, uint16_t codes[], byte lens[])
{
	uint_fast16_t i, i2;
	ptrdiff_t rem = (ptrdiff_t)in_len;
	uint32_t mask;
	const_bytes end;
	OutputBitstream bstr(out+HALF_SYMBOLS, out_len-HALF_SYMBOLS);

	// Write the Huffman prefix codes as lengths
	for (i = 0, i2 = 0; i < HALF_SYMBOLS; ++i, i2+=2) { out[i] = (lens[i2+1] << 4) | lens[i2]; }

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
				off &= OffsetMasks[O];
				sym = (uint_fast16_t)((O << 4) | MIN(0xF, len) | 0x100);
				if (!bstr.WriteBits(codes[sym], lens[sym]))					{ break; }
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
				if (!bstr.WriteBits(codes[*in], lens[*in]))					{ break; }
				++in; --rem;
			}
		}
		if (rem < 0)
			break;
		if (rem < i) { i = (byte)rem; }

		// Write the remaining literal symbols
		for (end = in+i; in != end && bstr.WriteBits(codes[*in], lens[*in]); ++in);
		if (in != end)														{ break; }
		rem -= i;
	}

	// Write end of stream symbol and return insufficient buffer or the compressed size
	if (rem > 0)
	{
		PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	bstr.Finish(); // make sure that the write stream is finished writing
	return bstr.Position()+HALF_SYMBOLS;
}
static size_t xpress_huff_compress_chunk(const_bytes in, int32_t* in_len, const_bytes in_end, bytes out, size_t out_len, bytes buf, Dictionary* d)  // 6.5 kb stack
{
	size_t buf_len;
	uint32_t symbol_counts[SYMBOLS]; // 4*512 = 2 kb
	uint16_t huffman_codes[SYMBOLS]; // 1 kb
	byte     huffman_lens [SYMBOLS]; // 0.5 kb

	if (out_len < MIN_DATA)
	{
		PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	if (*in_len == 0) // implies end_of_stream
	{
		memset(out, 0, MIN_DATA);
		out[STREAM_END>>1] = STREAM_END_LEN_1;
		return MIN_DATA;
	}

	////////// Perform the initial LZ77 compression //////////
	if ((buf_len = xh_lz77_compress(in, in_len, in_end, buf, symbol_counts, d)) == 0) { return 0; } // errno already set

	////////// Create the Huffman codes/lens //////////
	if (!xh_create_codes(symbol_counts, huffman_codes, huffman_lens)) { return 0; } // errno already set, 3 kb stack
	
	////////// Write compressed data //////////
	return xh_encode(buf, buf_len, out, out_len, huffman_codes, huffman_lens);
}
size_t xpress_huff_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	size_t done, total = 0;
	const const_bytes in_end = in+in_len;
	int32_t chunk_size;
	bytes buf;
	Dictionary d(in, in_end);

	if (in_len == 0) { return 0; }
	buf = (bytes)malloc((in_len > CHUNK_SIZE) ? 73739 : (in_len / 32 * 36 + 36 + 4 + 7)); // for every 32 bytes in "in" we need up to 36 bytes in the temp buffer + maybe an extra uint32 length symbol + up to 7 for the EOS
	if (buf == NULL) { return 0;  } // errno already set

	// Go through each chunk except the last
	while (in_len > CHUNK_SIZE)
	{
		// Compress a chunk
		chunk_size = CHUNK_SIZE;
		if ((done = xpress_huff_compress_chunk(in, &chunk_size, in_end, out, out_len, buf, &d)) == 0) { free(buf); return 0; } // errno already set

		// Update all the positions and lengths
		in      += chunk_size;
		in_len  -= chunk_size;
		out     += done;
		out_len -= done;
		total   += done;
	}

	// Do the last chunk
	chunk_size = (int32_t)in_len;
	if ((done = xpress_huff_compress_chunk(in, &chunk_size, in_end, out, out_len, buf, &d)) == 0) { total = 0; }
	total += done;

	// Cleanup
	free(buf);

	// Return the total number of compressed bytes
	return total;
}


////////////////////////////// Decompression Functions /////////////////////////////////////////////
typedef struct _XH_NODE			// A node in a Huffman prefix code tree
{
	uint16_t symbol;			// The symbol for the leaf or INVALID for internal nodes
	struct _XH_NODE* child[2];	// The node’s two children, or NULL if those children don't exist
} XH_NODE;
inline static uint_fast16_t xh_add_leaf(XH_NODE n[], uint_fast16_t npos, uint32_t code, byte len, uint16_t sym)
{
	XH_NODE *node = n;
	while (--len)
	{
		uint32_t i = (code >> len) & 1;
		if (node->child[i] == NULL)
			(node->child[i] = &n[npos++])->symbol = INVALID;
		node = node->child[i];
	}
	(node->child[code & 1] = &n[npos++])->symbol = sym;
	return npos;
}
inline static void xh_build_prefix_code_tree(const_bytes in, XH_NODE n[]) // 2.5 kb stack
{
	uint16_t syms[SYMBOLS], temp[SYMBOLS]; // 2*2*512 = 2 kb
	byte lens[SYMBOLS], nbits = 1; // 512 = 0.5 kb
	uint_fast16_t i, i2, len = 0, npos = 1;
	uint32_t code = 0;

	memset(n, 0, 1023*sizeof(XH_NODE));

	for (i = 0, i2 = 0; i < HALF_SYMBOLS; ++i)
	{
		if ((lens[i2] = (in[i] & 0xF)) > 0) { syms[len++] = (uint16_t)i2; } ++i2;
		if ((lens[i2] = (in[i] >> 4))  > 0) { syms[len++] = (uint16_t)i2; } ++i2;
	}

	msort(syms, temp, lens, 0, len);

	n[0].symbol = INVALID;
	for (i = 0; i < len; ++i)
	{
		code <<= (lens[syms[i]] - nbits);
		npos = xh_add_leaf(n, npos, code++, nbits = lens[syms[i]], syms[i]);
	}
}
inline static uint_fast16_t xh_decode_symbol(InputBitstream* bstr, XH_NODE* n)
{
	do {
		byte bit = bstr->ReadBit();
		if (bit > 1) return INVALID;
		n = n->child[bit];
	} while (n->symbol == INVALID);
	return n->symbol;
}
static bool xpress_huff_decompress_chunk(const_bytes in, size_t in_len, size_t* in_pos, bytes out, size_t out_len, size_t* out_pos, const const_bytes out_origin, bool* end_of_stream)
{
	size_t i = 0;
	XH_NODE codes[2*SYMBOLS-1]; // the maximum number of nodes in the Huffman code tree is 2*SYMBOLS-1 = 1023, overall this is ~10kb or ~20kb (64-bit)
	InputBitstream bstr(in+HALF_SYMBOLS, in_len-HALF_SYMBOLS);

	if (in_len < MIN_DATA)
	{
		if (in_len) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); errno = E_INVALID_DATA; }
		return false;
	}

	xh_build_prefix_code_tree(in, codes);
	
	do
	{
		uint_fast16_t sym = xh_decode_symbol(&bstr, codes);
		if (sym == INVALID)													{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); errno = E_INVALID_DATA; return false; }
		else if (sym == STREAM_END && bstr.MaskIsZero())					{ *end_of_stream = true; break; }
		else if (sym < 0x100)
		{
			if (i == out_len)												{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return false; }
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
				if (bstr.RemainingBytes() < 1)								{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read extra byte for length\n"); errno = E_INVALID_DATA; return false; }
				else if ((len = bstr.ReadRawByte()) == 0xFF)
				{
					if (bstr.RemainingBytes() < 2)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read two bytes for length\n"); errno = E_INVALID_DATA; return false; }
					if ((len = bstr.ReadRawUInt16()) == 0)
					{
						if (bstr.RemainingBytes() < 4)						{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read four bytes for length\n"); errno = E_INVALID_DATA; return false; }
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
	*in_pos = bstr.Position()+HALF_SYMBOLS;
	*out_pos = i;
	return true;
}
size_t xpress_huff_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes out_start = out;
	size_t in_pos = 0, out_pos = 0;
	bool end_of_stream = false;
	do
	{
		if (!xpress_huff_decompress_chunk(in, in_len, &in_pos, out, out_len, &out_pos, out_start, &end_of_stream)) { return 0; } // errno already set
		in  += in_pos;  in_len  -= in_pos;
		out += out_pos; out_len -= out_pos;
	} while (!end_of_stream);
	return out-out_start;
}
