#include "stdafx.h"
#include "xpress.h"

#define MIN_DATA	5
#define MAX_BYTE	0x100	// +1
#define MAX_OFFSET	0x2000
#define MAX_OFF_DBL	(MAX_OFFSET*2)
#define MAX_LENGTH	UINT32_MAX
#define MAX_HASH	0x8000	// +1

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2
#define MAX(a, b) (((a) > (b)) ? (a) : (b)) // Get the maximum of 2


////////////////////////////// Compression Dictionary //////////////////////////////////////////////
inline static uint32_t GetMatchLength(const_bytes a, const_bytes b, const const_bytes end)
{
	// like memcmp but tells you the length of the match and optimized
	// assumptions: a < b < end
	const const_bytes b_start = b, end_4 = end-4;
	byte a0, b0;
	while (b < end_4 && *((uint32_t*)a) == *((uint32_t*)b))
	{
		a += 4;
		b += 4;
	}
	do
	{
		a0 = *a++;
		b0 = *b++;
	} while (b < end && a0 == b0);
	return (uint_fast16_t)(b - b_start - 1);
}

// Tabulation Hashing
// Will hash 3 bytes at a time to a number from 0 to 0x7FFF (inclusive)
static uint16_t xpress_hashes[3][MAX_BYTE];
static bool xpress_hashes_initialized = false;
// Initializes the tabulation data using a linear congruential generator
// The values are essentially random and evenly distributed
template<uint32_t SEED, uint32_t A, uint32_t C, uint32_t M, uint32_t S>
static void xpress_hashes_init_lcg()
{
	uint32_t hash = SEED;
	int i;
	for (i = 0; i < 3; ++i)
	{
		uint16_t* table = xpress_hashes[i];
		int j;
		for (j = 0; j < MAX_BYTE; ++j)
		{
			if (M)	hash = (hash * A + C) % M; // if M == 0, then M is actually max int, no need to do any division
			else	hash = (hash * A + C);
			table[j] = (hash >> S) & (MAX_HASH - 1);
		}
	}
	xpress_hashes_initialized = true;
}
// Initialize using the LCG with constants from GLIBC rand() and a "random" seed
inline static void xpress_hashes_init() { xpress_hashes_init_lcg<0x001A5BA5u, 0x41C64E6Du, 12345u, 1ull << 32, 16>(); }
inline static uint_fast16_t xpress_hash(const const_bytes x) { return xpress_hashes[0][x[0]] ^ xpress_hashes[1][x[1]] ^ xpress_hashes[2][x[2]]; }

typedef struct _XpressLzDictionary // 192 kb (on 32-bit) or 384 kb (on 64-bit)
{
	const_bytes start, end, end3;
	const_bytes table[MAX_HASH];
	const_bytes window[MAX_OFF_DBL];
} Dictionary;
inline static void Dictionary_Init(Dictionary* d, const const_bytes start, const const_bytes end)
{
	if (!xpress_hashes_initialized) { xpress_hashes_init(); }
	d->start = start;
	d->end = end;
	d->end3 = end - 3;
	memset(d->table, 0, sizeof(d->table));
}
#define WINDOW_POS(x) (((x)-d->start)&(MAX_OFF_DBL-1))
inline static const_bytes Dictionary_Fill(Dictionary* d, const_bytes data)
{
	uint_fast16_t pos = WINDOW_POS(data); // either 0x0000 or 0x2000
	const const_bytes end = MIN(data + MAX_OFFSET, d->end3);
	while (data < end)
	{
		const uint_fast16_t hash = xpress_hash(data);
		d->window[pos++] = d->table[hash];
		d->table[hash] = data++;
	}
	return end;
}
inline static uint32_t Dictionary_Find(const Dictionary* d, const const_bytes data, uint_fast16_t* offset)
{
	const const_bytes xend = data - MAX_OFFSET;
#if PNTR_BITS <= 32
	const const_bytes end = d->end; // with 32-bits and less the data + MAX_LENGTH will always overflow
#else
	const const_bytes end = MIN(data + MAX_LENGTH, d->end);
#endif
	const_bytes x;
	uint32_t len = 2;
	for (x = d->window[WINDOW_POS(data)]; x >= xend; x = d->window[WINDOW_POS(x)])
	{
		const uint32_t l = GetMatchLength(x, data, end);
		if (l > len)
		{
			*offset = (uint_fast16_t)(data - x);
			len = l;
		}
	}
	return len;
}


////////////////////////////// Compression Functions ///////////////////////////////////////////////
size_t xpress_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes                  in_end  = in +in_len,  in_end3  = in_end  - 3;
	const const_bytes out_start = out, out_end = out+out_len, out_end1 = out_end - 1;
	const_bytes filled_to = in;

	uint32_t flags = 0, *out_flags = (uint32_t*)out;
	byte flag_count;
	byte* half_byte = NULL;
	
	Dictionary d;

	if (in_len == 0) { return 0; }
	if (out_len < MIN_DATA) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	Dictionary_Init(&d, in, in_end); // initialize the dictionary
	out += 4;		// skip four for flags
	*out++ = *in++;	// copy the first byte
	flag_count = 1;

	while (in < in_end3 && out < out_end1)
	{
		uint32_t len;
		uint_fast16_t off;
		if (filled_to <= in) { filled_to = Dictionary_Fill(&d, filled_to); }
		if ((len = Dictionary_Find(&d, in, &off)) < 3) { *out++ = *in++; flags <<= 1; } // Copy byte
		else // Match found
		{
			in += len;
			len -= 3;
			SET_UINT16(out, ((off-1) << 3) | MIN(len, 7));
			out += 2;
			if (len >= 0x7)
			{
				len -= 0x7;
				if (half_byte)
				{
					*half_byte |= MIN(len, 0xF) << 4;
					half_byte = NULL;
				}
				else
				{
					if (out >= out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
					*(half_byte=out++) = MIN(len, 0xF) & 0xF;
				}
				if (len >= 0xF)
				{
					len -= 0xF;
					if (out >= out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
					*out++ = (byte)MIN(len, 0xFF);
					if (len >= 0xFF)
					{
						len += 0xF+0x7;
						if (len <= 0xFFFF)
						{
							if (out + 2 > out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
							SET_UINT16(out, len); out += 2;
						}
						else
						{
							if (out + 6 > out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
							SET_UINT16(out, 0);
							SET_UINT32(out+2, len);
							out += 6;
						}
					}
				}
			}
			flags = (flags << 1) | 1;
		}
		if (++flag_count == 32)
		{
			SET_UINT32(out_flags, flags);
			flag_count = 0;
			if (out + 4 > out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
			out_flags = (uint32_t*)out;
			out += 4;
		}
	}
	while (in < in_end && out < out_end)
	{
		*out++ = *in++;
		flags <<= 1;
		if (++flag_count == 32)
		{
			SET_UINT32(out_flags, flags);
			flag_count = 0;
			if (out + 4 > out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
			out_flags = (uint32_t*)out;
			out += 4;
		}
	}
	flags = (flags << (32 - flag_count)) | ((1 << (32 - flag_count)) - 1); // finish shifting over and set all unused bytes to 1
	SET_UINT32(out_flags, flags);
	if (in != in_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	return out - out_start;
}


////////////////////////////// Decompression Functions /////////////////////////////////////////////
size_t xpress_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes                  in_end  = in +in_len,  in_endx  = in_end -0x054; // 4 + 32 * (2 + 0.5) from the end, or maybe 4 + 32 * (2 + 0.5 + 1 + 2 + 4) = 0x134
	const const_bytes out_start = out, out_end = out+out_len, out_endx = out_end-0x160; // 32 * 11 from the end
	const_byte* half_byte = NULL;
	uint32_t flags, flagged, len;
	uint_fast16_t off;

	if (in_len < MIN_DATA)
	{
		if (in_len) { PRINT_ERROR("Xpress Decompression Error: Invalid Data: Less than %d input bytes\n", MIN_DATA); errno = E_INVALID_DATA; }
		return 0;
	}

	// Most of the decompression happens here
	// Very few bounds checks are done but we can only go to near the end and not the end
	while (in < in_endx && out < out_endx)
	{
		// Handle a fragment
		flagged = (flags = GET_UINT32(in)) & 0x80000000;
		flags = (flags << 1) | 1;
		in += 4;
		do
		{
			if (flagged) // Either: offset/length symbol, end of flags, or end of stream (only happens in full-check version)
			{
				uint16_t sym;
				const_bytes o;

				// Offset/length symbol
				sym = GET_UINT16(in);
				off = (sym >> 3) + 1;
				in += 2;
				if ((len = sym & 0x7) == 0x7)
				{
					if (half_byte) { len = *half_byte >> 4; half_byte = NULL; }
					else           { len = *in & 0xF;       half_byte = in++; }
					if (len == 0xF)
					{
						if (in + 7 > in_endx)
						{
							goto CHECKED_LENGTH;
						}
						else if ((len = *(in++)) == 0xFF)
						{
							len = GET_UINT16(in);
							in += 2;
							if (len == 0) { len = GET_UINT32(in); in += 4; }
							if (len < 0xF+0x7) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Invalid length specified\n"); errno = E_INVALID_DATA; return 0; }
							len -= 0xF+0x7;
						}
						len += 0xF;
					}
					len += 0x7;
				}
				len += 0x3;

				if ((o = out-off) < out_start) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out, off, out_start); errno = E_INVALID_DATA; return 0; }

				// Write up to 3 bytes for close offsets so that we have >=4 bytes to read in all cases
				switch (off)
				{
				case 1: out[0] = out[1] = out[2] = o[0];     out += 3; len -= 3; break;
				case 2: out[0] = o[0]; out[1] = o[1];        out += 2; len -= 2; break;
				case 3: out[0]=o[0];out[1]=o[1];out[2]=o[2]; out += 3; len -= 3; break;
				}
				if (len)
				{
					// Write 8 bytes in groups of 4 (since we have >=4 bytes that can be read)
					uint32_t* out32 = (uint32_t*)out, *o32 = (uint32_t*)o;
					out += len;
					out32[0] = o32[0];
					out32[1] = o32[1];
					if (len > 8)
					{
						out32 += 2; o32 += 2; len -= 8;

						// Repeatedly write 16 bytes
						while (len > 16) 
						{
							if ((const_bytes)out32 >= out_endx)
							{
								if (out > out_end) { PRINT_ERROR("Xpress Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
								out = (bytes)out32;
								goto CHECKED_COPY;
							}
							out32[0] = o32[0];
							out32[1] = o32[1];
							out32[2] = o32[2];
							out32[3] = o32[3];
							out32 += 4; o32 += 4; len -= 16;
						}
						// Last 16 bytes
						out32[0] = o32[0];
						out32[1] = o32[1];
						out32[2] = o32[2];
						out32[3] = o32[3];
					}
				}
				flagged = flags & 0x80000000;
				flags <<= 1;
			}
			else
			{
				// Copy bytes directly
					 if ((flagged = (flags & 0x80000000)) != 0) { *out++ = *in++; flags <<= 1; } // Copy 1 byte directly
				else if ((flagged = (flags & 0x40000000)) != 0) { *(uint16_t*)out = *(uint16_t*)in; out += 2; in += 2; flags <<= 2; } // Copy 2 bytes directly
				else     { byte n = (flags & 0x20000000) ? 3 : 4; *(uint32_t*)out = *(uint32_t*)in; out += n; in += n; flagged = (flags & 0x30000000); flags <<= n; } // Copy 3 or 4 bytes directly
			}
		} while (flags);
	}

	// Slower decompression but with full bounds checking
	while (in + 4 <= in_end)
	{
		// Handle a fragment
		flagged = (flags = GET_UINT32(in)) & 0x80000000;
		flags = (flags << 1) | 1;
		in += 4;
		do
		{
			if (in == in_end)
			{
				if (!flagged) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read a byte\n"); errno = E_INVALID_DATA; return 0; }
				else { return out - out_start; }
			}
			else if (flagged) // Either: offset/length symbol, end of flags, or end of stream (checked above)
			{
				uint16_t sym;

				// Offset/length symbol
				if (in + 2 > in_end) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read 2 bytes for offset/length\n"); errno = E_INVALID_DATA; return 0; }
				sym = GET_UINT16(in);
				off = (sym >> 3) + 1;
				in += 2;
				if ((len = sym & 0x7) == 0x7)
				{
					if (half_byte) { len = *half_byte >> 4; half_byte = NULL; }
					else
					{
						if (in == in_end) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read a half-byte for length\n"); errno = E_INVALID_DATA; return 0; }
						len = *in & 0xF;
						half_byte = in++;
					}
					if (len == 0xF)
					{
CHECKED_LENGTH:			if (in == in_end) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read a byte for length\n"); errno = E_INVALID_DATA; return 0; }
						if ((len = *(in++)) == 0xFF)
						{
							if (in + 2 > in_end) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read two bytes for length\n"); errno = E_INVALID_DATA; return 0; }
							len = GET_UINT16(in);
							in += 2;
							if (len == 0)
							{
								if (in + 4 > in_end) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read four bytes for length\n"); errno = E_INVALID_DATA; return 0; }
								len = GET_UINT32(in);
								in += 4;
							}
							if (len < 0xF+0x7) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Invalid length specified\n"); errno = E_INVALID_DATA; return 0; }
							len -= 0xF+0x7;
						}
						len += 0xF;
					}
					len += 0x7;
				}
				len += 0x3;

				if (out - off < out_start) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out, off, out_start); errno = E_INVALID_DATA; return 0; }
				if (out + len > out_end)   { PRINT_ERROR("Xpress Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

				if (off == 1)
				{
					memset(out, out[-1], len);
					out += len;
				}
				else
				{
					const_bytes end;
CHECKED_COPY:		for (end = out + len; out < end; ++out) { *out = *(out-off); }
				}
			}
			else if (out == out_end) { PRINT_ERROR("Xpress Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
			else { *out++ = *in++; } // Copy byte directly
			flagged = flags & 0x80000000;
			flags <<= 1;
		} while (flags);
	}

	if (in == in_end) { return out - out_start; }
	PRINT_ERROR("Xpress Decompression Error: Invalid data: Unable to read 4 bytes for flags\n");
	errno = E_INVALID_DATA;
	return 0;
}
size_t xpress_uncompressed_size(const_bytes in, size_t in_len)
{
	const const_bytes in_end = in+in_len, in_endx = in_end-0x134; // 4 + 32 * (2 + 0.5 + 1 + 2 + 4) from the end
	size_t out = 0;
	const_byte* half_byte = NULL;
	uint32_t flags, flagged, len;

	if (in_len < MIN_DATA) { return 0; }

	// Faster decompression, minimal bounds checking
	while (in < in_endx)
	{
		flagged = (flags = GET_UINT32(in)) & 0x80000000;
		flags = (flags << 1) | 1;
		in += 4;
		do
		{
			if (flagged)
			{
				uint16_t sym = GET_UINT16(in);
				if (out < (uint16_t)((sym>>3)+1)) { errno = E_INVALID_DATA; return 0; }
				in += 2;
				if ((len = (sym & 0x7)) == 0x7)
				{
					if (half_byte) { len = *half_byte >> 4; half_byte = NULL; }
					else           { len = *in & 0xF;       half_byte = in++; }
					if (len == 0xF)
					{
						if ((len = *(in++)) == 0xFF)
						{
							len = GET_UINT16(in);
							in += 2;
							if (len == 0) { len = GET_UINT32(in); in += 4; }
							if (len < 0xF+0x7) { errno = E_INVALID_DATA; return 0; }
							len -= 0xF+0x7;
						}
						len += 0xF;
					}
					len += 0x7;
				}
				out += len + 3;
			}
			else { out++; in++; }
			flagged = flags & 0x80000000;
			flags <<= 1;
		} while (flags);
	}

	// Slower decompression but with full bounds checking
	while (in + 4 <= in_end)
	{
		flagged = (flags = GET_UINT32(in)) & 0x80000000;
		flags = (flags << 1) | 1;
		in += 4;
		do
		{
			if (in == in_end) { if (!flagged) { errno = E_INVALID_DATA; return 0; } else { return out; } }
			else if (flagged)
			{
				uint16_t sym = GET_UINT16(in);
				if (in + 2 > in_end) { errno = E_INVALID_DATA; return 0; }
				sym = GET_UINT16(in);
				if (out < (uint16_t)((sym>>3)+1)) { errno = E_INVALID_DATA; return 0; }
				in += 2;
				if ((len = sym & 0x7) == 0x7)
				{
					if (half_byte) { len = *half_byte >> 4; half_byte = NULL; }
					else
					{
						if (in == in_end) { errno = E_INVALID_DATA; return 0; }
						len = *in & 0xF;
						half_byte = in++;
					}
					if (len == 0xF)
					{
						if (in == in_end) { errno = E_INVALID_DATA; return 0; }
						if ((len = *(in++)) == 0xFF)
						{
							if (in + 2 > in_end) { errno = E_INVALID_DATA; return 0; }
							len = GET_UINT16(in);
							in += 2;
							if (len == 0)
							{
								if (in + 4 > in_end) { errno = E_INVALID_DATA; return 0; }
								len = GET_UINT32(in);
								in += 4;
							}
							if (len < 0xF+0x7) { errno = E_INVALID_DATA; return 0; }
							len -= 0xF+0x7;
						}
						len += 0xF;
					}
					len += 0x7;
				}
				out += len + 0x3;
			}
			else { out++; in++; }
			flagged = flags & 0x80000000;
			flags <<= 1;
		} while (flags);
	}
	
	return in == in_end ? out : 0;
}
