#include "stdafx.h"
#include "xpress_huff.h"

#include "Dictionary.h"
#include "Bitstream.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#pragma optimize("t", on)


////////////////////////////// General Definitions and Functions ///////////////////////////////////
#define MAX_LEN			0xFFFF // Technically supports 0x010002, but that is a very rare situation, doesn't really help, and to support it would require a major overhaul to the dictionary system
#define MAX_OFFSET		0xFFFF
#define MAX_BYTE		0x100
#define STREAM_END		0x100

#define STREAM_END_LEN_1	1
//#define STREAM_END_LEN_1	1<<4 // if STREAM_END&1

#define SYMBOLS			0x200
#define HALF_SYMBOLS	0x100
#define INVALID			0xFFFF

#define MIN_DATA		HALF_SYMBOLS + 4 // the 512 Huffman lens + 2 uint16s for minimal bitstream
#define CHUNK_SIZE		0x10000

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

// Merge-sorts syms[l, r) using conditions[syms[x]]
// Use merge-sort so that it is stable, keeping symbols in increasing order
#ifdef __cplusplus
template<typename T> // T is either uint32_t or byte
static void msort(uint16_t* syms, uint16_t* temp, T* conditions, uint_fast16_t l, uint_fast16_t r)
{
	uint_fast16_t len = r - l;
	if (len <= 1) return;
	
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
#else
#error Need to implement non-template version of msort for uint32_t and byte
static void msort_u8(uint16_t* syms, uint16_t* temp, byte* conditions, uint_fast16_t l, uint_fast16_t r)
{
}
static void msort_u32(uint16_t* syms, uint16_t* temp, uint32_t* conditions, uint_fast16_t l, uint_fast16_t r)
{
}
#define msort(syms, temp, conditions, l, r) \
	if (sizeof(conditions[0]) == 8) { msort_u8(syms, temp, conditions, l, r); } \
	else if (sizeof(conditions[0]) == 32) { msort_u32(syms, temp, conditions, l, r); } \
	else { byte type_size_not_impl[0]; }
#endif

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
inline static byte highbit(uint_fast16_t x) { uint_fast16_t y = x >> 8; return y ? 8 + Log2Table[y] : Log2Table[x]; } // returns 0x0 - 0xF
static size_t xh_lz77_compress(const_bytes in, uint32_t in_len, const_bytes in_end, bytes out, uint32_t symbol_counts[], Dictionary* d, Dictionary* d2)
{
	uint32_t rem = in_len;
	uint32_t mask;
	const const_bytes in_orig = in, out_orig = out;
	const_bytes end;
	bytes frag;
	byte i;

	if (!Dictionary_Reset(d)) { return 0; } // errno already set
	memset(symbol_counts, 0, SYMBOLS*sizeof(uint32_t));

	////////// Count the symbols and write the initial LZ77 compressed data //////////
	// A uint32 mask holds the status of each subsequent byte (0 for literal, 1 for offset / length)
	// Literals are stored using a single byte for their value
	// Offset / length pairs are stored in the following manner:
	//   Offset: a uint16
	//   Length: for length-3:
	//     < 0xFF, a byte
	//     >=0xFF, a byte of 0xFF then a uint16
	// The number of bytes between uint32 masks >=32 and <=160 (5*32)
	while (rem)
	{
		// Go through each bit
		for (i = 0, mask = 0, frag = out+4; i < 32 && rem; ++i)
		{
			uint_fast16_t len, off;
			mask >>= 1;
			if ((len = Dictionary_Find(d, d2, in, rem, d2 ? (in-MAX_OFFSET) : in_orig, &off)) > 0)
			{
				// Add new entries
				for (end = in+len; in < end && Dictionary_Add(d, in, in_end-in); ++in);
				if (in != end) { return 0; } // errno already set
				rem -= len;

				// Write offset / length
				*(uint16_t*)frag = (uint16_t)off;
				frag += 2;
				len -= 3;
				if (len >= 0xFF) { *frag = 0xFF; *(uint16_t*)(frag+1) = (uint16_t)len; frag += 3; }
				else             { *frag = (byte)len; ++frag; }
				mask |= 0x80000000; // set the highest bit

				// Create a symbol from the offset and length
				++symbol_counts[(highbit(off) << 4) | MIN(0xF, len) | 0x100];
			}
			else
			{
				// Add new entry and write the literal value (which is the symbol)
				if (!Dictionary_Add(d, in, in_end-in)) { return 0; } // errno already set
				--rem;
				++symbol_counts[*frag = *in];
				++frag;
				++in;
			}
		}
		if (!rem)
		{
			mask >>= (32-i); // finish moving the value over
			if (in_orig+in_len == in_end)
			{
				// Add the end of stream symbol
				if (i == 32)
				{
					// Need to add a new mask since the old one is full with just one bit set
					*(uint32_t*)frag = 1;
					frag += 4;
				}
				else
				{
					// Add to the old mask
					mask |= 1 << i; // set the highest bit
				}
				memset(frag, 0, 3);
				frag += 3;
				++symbol_counts[STREAM_END];
			}
		}
		// Save mask, advance output to the end of the fragment
		*(uint32_t*)out = mask;
		out = frag;
	}

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
	size_t rem = in_len;
	uint32_t mask;
	const_bytes end;
	OutputBitstream bstr;

	// Write the Huffman prefix codes as lengths
	for (i = 0, i2 = 0; i < HALF_SYMBOLS; ++i, i2+=2) { out[i] = (lens[i2+1] << 4) | lens[i2]; }

	// Write the encoded compressed data
	// This involves parsing the LZ77 compressed data and re-writing it with the Huffman code
	BSWriteInit(&bstr, out+HALF_SYMBOLS, out_len-HALF_SYMBOLS);
	while (rem)
	{
		// Handle a fragment
		// Bit mask tells us how to handle the next 32 symbols, go through each bit
		for (i = 32, mask = *(uint32_t*)in, in += 4, rem -= 4; mask && rem; --i, mask >>= 1)
		{
			if (mask & 1) // offset / length symbol
			{
				uint_fast16_t len, off, sym;
				byte O;

				// Get the LZ77 offset and length
				off = *(uint16_t*)in;
				len = in[2];
				in += 3; rem -= 3;
				if (len == 0xFF)
				{
					len = *(uint16_t*)in;
					in += 2; rem -= 2;
				}

				// Write the Huffman code then extra offset bits and length bytes
				O = highbit(off);
				// len is already -= 3
				off &= OffsetMasks[O];
				sym = (uint_fast16_t)((O << 4) | MIN(0xF, len) | 0x100);
				if (!BSWriteBits(&bstr, codes[sym], lens[sym]))					{ break; }
				if (len >= 0x10E)
				{	// For 0xFF+0xF <= len <= MAX_LEN, write 3 bytes: 0xFF then uint16 with the length
					if (!BSWriteByte(&bstr, 0xFF) || !BSWriteByte(&bstr, (byte)(len & 0xFF)) || !BSWriteByte(&bstr, (byte)(len >> 8))) { break; }
				}
				else if (len >= 0xF && !BSWriteByte(&bstr, (byte)(len - 0xF)))	{ break; }
				if (!BSWriteBits(&bstr, off, O))								{ break; }
			}
			else
			{
				// Write the literal symbol
				if (!BSWriteBits(&bstr, codes[*in], lens[*in]))					{ break; }
				++in; --rem;
			}
		}
		if (rem < i) { i = (byte)rem; }

		// Write the remaining literal symbols
		for (end = in+i; in != end && BSWriteBits(&bstr, codes[*in], lens[*in]); ++in);
		if (in != end)															{ break; }
		rem -= i;
	}

	// Write end of stream symbol and return insufficient buffer or the compressed size
	if (rem)
	{
		PRINT_ERROR("Xpress Huffman Compression Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	BSWriteFinish(&bstr); // make sure that the write stream is finished writing
	return bstr.index+HALF_SYMBOLS;
}
static size_t xpress_huff_compress_chunk(const_bytes in, uint32_t in_len, const_bytes in_end, bytes out, size_t out_len, bytes buf, Dictionary* d, Dictionary* d2)  // 6.5 kb stack
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
	if (in_len == 0) // implies end_of_stream
	{
		memset(out, 0, MIN_DATA);
		out[STREAM_END>>1] = STREAM_END_LEN_1;
		return MIN_DATA;
	}

	////////// Perform the initial LZ77 compression //////////
	if ((buf_len = xh_lz77_compress(in, in_len, in_end, buf, symbol_counts, d, d2)) == 0) { return 0; } // errno already set

	////////// Create the Huffman codes/lens //////////
	if (!xh_create_codes(symbol_counts, huffman_codes, huffman_lens)) { return 0; } // errno already set, 3 kb stack
	
	////////// Write compressed data //////////
	return xh_encode(buf, buf_len, out, out_len, huffman_codes, huffman_lens);
}
size_t xpress_huff_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	size_t done, total = 0;
	const const_bytes in_end = in+in_len;
	bytes buf;
	Dictionary* d, *d2 = NULL, *temp_d;

	if (in_len == 0) { return 0; }
	if ((d = Dictionary_Create()) == NULL) { return 0; } // errno already set
	buf = (bytes)malloc((in_len > CHUNK_SIZE) ? (in_len / 32 * 36 + 36 + 7) : 73735); // for every 32 bytes in in we need up to 36 bytes in the temp buffer + up to 7 for the EOS
	if (buf == NULL) { Dictionary_Destroy(d); return 0;  } // errno already set

	// Go through each chunk except the last
	while (in_len > CHUNK_SIZE)
	{
		// Compress a chunk
		if ((done = xpress_huff_compress_chunk(in, CHUNK_SIZE, in_end, out, out_len, buf, d, d2)) == 0) { Dictionary_Destroy(d); Dictionary_Destroy(d2); free(buf); return 0; } // errno already set

		// Update all the positions and lengths
		in      += CHUNK_SIZE;
		in_len  -= CHUNK_SIZE;
		out     += done;
		out_len -= done;
		total   += done;

		// Swap dictionaries
		if (d2 == NULL && (d2 = Dictionary_Create()) == NULL) { Dictionary_Destroy(d); free(buf); return 0; }
		temp_d = d; d = d2; d2 = temp_d;
	}

	// Do the last chunk
	if ((done = xpress_huff_compress_chunk(in, (uint32_t)in_len, in_end, out, out_len, buf, d, d2)) == 0) { total = 0; }
	total += done;

	// Cleanup
	Dictionary_Destroy(d);
	if (d2) { Dictionary_Destroy(d2); }
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
		byte bit = BSReadBit(bstr);
		if (bit > 1) return INVALID;
		n = n->child[bit];
	} while (n->symbol == INVALID);
	return n->symbol;
}
static bool xpress_huff_decompress_chunk(const_bytes in, size_t in_len, size_t* in_pos, bytes out, size_t out_len, size_t* out_pos, const const_bytes out_origin, bool* end_of_stream)
{
	size_t i = 0;
	XH_NODE codes[2*SYMBOLS-1]; // the maximum number of nodes in the Huffman code tree is 2*SYMBOLS-1 = 1023, overall this is ~10kb or ~20kb (64-bit)
	InputBitstream bstr;

	if (in_len < MIN_DATA)
	{
		if (in_len) { PRINT_ERROR("Xpress Huffman Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); errno = E_INVALID_DATA; }
		return false;
	}

	xh_build_prefix_code_tree(in, codes);
	BSReadInit(&bstr, in+HALF_SYMBOLS, in_len-HALF_SYMBOLS);
	
	do
	{
		uint_fast16_t sym = xh_decode_symbol(&bstr, codes);
		if (sym == INVALID)													{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read enough bits for symbol\n"); errno = E_INVALID_DATA; return false; }
		else if (sym == STREAM_END && (bstr.mask>>(32-bstr.bits)) == 0)		{ *end_of_stream = true; break; }
		else if (sym < MAX_BYTE)
		{
			if (i == out_len)												{ PRINT_ERROR("Xpress Huffman Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return false; }
			out[i++] = (byte)sym;
		}
		else
		{
			uint32_t len = sym & 0xF, off = BSPeek(&bstr, (byte)(sym = ((sym>>4) & 0xF)));
#ifdef PRINT_ERRORS
			if (off == 0xFFFFFFFF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read %u bits for offset\n", sym); errno = E_INVALID_DATA; return false; }
			if ((out+i-(off+=1<<sym)) < out_origin)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out+i, off, out_origin); errno = E_INVALID_DATA; return false; }
#else
			if (off == 0xFFFFFFFF || (out+i-(off+=1<<sym)) < out_origin)	{ errno = E_INVALID_DATA; return false; }
#endif
			if (len == 0xF)
			{
				if (bstr.index >= bstr.len)									{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read extra byte for length\n"); errno = E_INVALID_DATA; return false; }
				if ((len = bstr.data.in[bstr.index++]) == 0xFF)
				{
					if (bstr.index + 2 > bstr.len)							{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read two bytes for length\n"); errno = E_INVALID_DATA; return false; }
					len = GET_UINT16(bstr.data.in+bstr.index);
					bstr.index += 2;
					if (len == 0)
					{
						if (bstr.index + 4 > bstr.len)						{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Unable to read four bytes for length\n"); errno = E_INVALID_DATA; return false; }
						len = GET_UINT32(bstr.data.in+bstr.index);
						bstr.index += 4;
					}
					if (len < 0xF)											{ PRINT_ERROR("Xpress Huffman Decompression Error: Invalid data: Invalid length specified\n"); errno = E_INVALID_DATA; return false; }
					len -= 0xF;
				}
				len += 0xF;
			}
			len += 3;
			BSSkip(&bstr, (byte)sym);

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
	} while (i < CHUNK_SIZE || (bstr.mask>>(32-bstr.bits)) != 0); /* end of chunk, not stream */
	*in_pos = bstr.index+HALF_SYMBOLS;
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
