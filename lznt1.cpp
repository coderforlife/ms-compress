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


#include "lznt1.h"

#include "LZNT1Dictionary.h"

// Get the minimum of 2
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


#ifdef COMPRESSION_API_EXPORT
size_t lznt1_max_compressed_size(size_t in_len) { return in_len + 3 + 2 * ((in_len + 4095) / 4096); }
#endif


/////////////////// Compression Functions /////////////////////////////////////
static uint_fast16_t lznt1_compress_chunk(const_bytes in, uint_fast16_t in_len, bytes out, size_t out_len, LZNT1Dictionary* d)
{
	uint_fast16_t in_pos = 0, out_pos = 0, rem = in_len, pow2 = 0x10, mask3 = 0x1002, shift = 12, len, off, end;
	uint16_t sym;
	byte i, pos, bits, bytes[16]; // if all are special, then it will fill 16 bytes
	d->Reset();

	while (out_pos < out_len && rem)
	{
		// Go through each bit
		for (i = 0, pos = 0, bits = 0; i < 8 && out_pos < out_len && rem; ++i)
		{
			bits >>= 1;

			while (pow2 < in_pos) { pow2 <<= 1; mask3 = (mask3>>1)+1; --shift; }

			if ((len = d->Find(in+in_pos, MIN(rem, mask3), in, &off)) > 0)
			{
				// And new entries
				d->Add(in+in_pos, len, rem);
				in_pos += len; rem -= len;

				// Write symbol that is a combination of offset and length
				sym = (uint16_t)(((off-1) << shift) | (len-3));
				SET_UINT16(bytes+pos, sym);
				pos += 2;
				bits |= 0x80; // set the highest bit
			}
			else
			{
				// And new entry
				d->Add(in+in_pos, rem--);

				// Copy directly
				bytes[pos++] = in[in_pos++];
			}
		}
		end = out_pos+1+pos;
		if (end >= in_len || end > out_len)  { return in_len; } // should be uncompressed or insufficient buffer
		out[out_pos] = (bits >> (8-i)); // finish moving the value over
		memcpy(out+out_pos+1, bytes, pos);
		out_pos += 1+pos;
	}

	// Return insufficient buffer or the compressed size
	return rem ? in_len : out_pos;
}
size_t lznt1_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	size_t out_pos = 0, in_pos = 0;
	uint_fast16_t in_size, out_size, flags;
	uint16_t header;
	LZNT1Dictionary d;

	while (out_pos < out_len-1 && in_pos < in_len)
	{
		// Compress the next chunk
		in_size = (uint_fast16_t)MIN(in_len-in_pos, 0x1000);
		out_size = lznt1_compress_chunk(in+in_pos, in_size, out+out_pos+2, out_len-out_pos-2, &d);
		if (out_size < in_size) // chunk is compressed
		{
			flags = 0xB000;
		}
		else // chunk is uncompressed
		{
			if (out_pos+2+in_size > out_len) { break; }
			out_size = in_size;
			flags = 0x3000;
			memcpy(out+out_pos+2, in+in_pos, out_size);
		}

		// Save header
		header = (uint16_t)(flags | (out_size-1));
		SET_UINT16(out+out_pos, header);

		// Increment positions
		out_pos += out_size+2;
		in_pos  += in_size;
	}
	
	// Return insufficient buffer or the compressed size
	if (in_pos < in_len)
	{
		PRINT_ERROR("LZNT1 Compression Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	// Clear up to 2 bytes for end-of-stream (not reported in size, not necessary, but could help)
	memset(out+out_pos, 0, MIN(out_len-out_pos, 2));
	return out_pos;
}


/////////////////// Decompression Functions ///////////////////////////////////
static size_t lznt1_decompress_chunk(const_bytes in, const const_bytes in_end, bytes out, const const_bytes out_end)
{
	const const_bytes                  in_endx  = in_end -0x11; // 1 + 8 * 2 from the end
	const const_bytes out_start = out, out_endx = out_end-0x58; // 8 * (3 + 8) from the end
	byte flags, flagged;
	
	uint_fast16_t pow2 = 0x10, mask = 0xFFF, shift = 12;
	const_bytes pow2_target = out_start + 0x10;
	uint_fast16_t len, off;

	// Most of the decompression happens here
	// Very few bounds checks are done but we can only go to near the end and not the end
	while (in < in_endx && out < out_endx)
	{
		// Handle a fragment
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (flagged)  // Offset/length symbol
			{
				uint16_t sym;
				const_bytes o;
				
				// Offset/length symbol
				while (out > pow2_target) { pow2 <<= 1; pow2_target = out_start + pow2; mask >>= 1; --shift; } // Update the current power of two available bytes
				sym = GET_UINT16(in);
				off = (sym>>shift)+1;
				len = (sym&mask)+3;
				in += 2;
				if ((o = out-off) < out_start) { PRINT_ERROR("LZNT1 Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out, off, out_start); errno = E_INVALID_DATA; return 0; }

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
								if (out > out_end) { PRINT_ERROR("LZNT1 Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
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
						if ((const_bytes)out32 >= out_endx)
						{
							if (out > out_end) { PRINT_ERROR("LZNT1 Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
							out = (bytes)out32;
							goto CHECKED_COPY;
						}
						out32[0] = o32[0];
						out32[1] = o32[1];
						out32[2] = o32[2];
						out32[3] = o32[3];
					}
				}
			}
			else { *out++ = *in++; } // Copy byte directly
			flagged = flags & 0x01;
			flags >>= 1;
		} while (flags);
	}
	
	// Slower decompression but with full bounds checking
	while (in < in_end)
	{
		// Handle a fragment
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (in == in_end) { return out - out_start; }
			else if (flagged) // Offset/length symbol
			{
				uint16_t sym;
								
				// Offset/length symbol
				if (in + 2 > in_end) { PRINT_ERROR("LZNT1 Decompression Error: Invalid data: Unable to read 2 bytes for offset/length\n"); errno = E_INVALID_DATA; return 0; }
				while (out > pow2_target) { pow2 <<= 1; pow2_target = out_start + pow2; mask >>= 1; --shift; } // Update the current power of two available bytes
				sym = GET_UINT16(in);
				off = (sym>>shift)+1;
				len = (sym&mask)+3;
				in += 2;
				if (out - off < out_start) { PRINT_ERROR("LZNT1 Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out, off, out_start); errno = E_INVALID_DATA; return 0; }
				if (out + len > out_end) { PRINT_ERROR("LZNT1 Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

				// Copy bytes
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
			else if (out == out_end) { PRINT_ERROR("LZNT1 Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
			else { *out++ = *in++; } // Copy byte directly
			flagged = flags & 0x01;
			flags >>= 1;
		} while (flags);
	}

	if (in == in_end) { return out - out_start; }
	PRINT_ERROR("LZNT1 Decompression Error: Invalid data: Unable to read byte for flags\n");
	errno = E_INVALID_DATA;
	return 0;
}
size_t lznt1_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes in_end  = in  + in_len-1;
	const const_bytes out_end = out + out_len, out_start = out;

	// Go through every chunk
	while (in < in_end && out < out_end)
	{
		size_t out_size;
		uint_fast16_t in_size;
		uint16_t header;

		// Read chunk header
		if ((header = GET_UINT16(in)) == 0)
		{
#ifdef PRINT_WARNINGS
			if (in <= in_end) { PRINT_WARNING("LZNT1 Decompression Warning: Possible premature end (%p < %p)\n", in, in_end+1); }
#endif
			return out - out_start;
		}
		in_size = (header & 0x0FFF)+1;
		if (in+in_size >= in_end)
		{
			PRINT_ERROR("LZNT1 Decompression Error: Invalid data: Compressed chunk length is longer than available data (%p > %p)\n", in+2+in_size, in_end);
			errno = E_INVALID_DATA;
			return 0;
		}
		in += 2;

		// Flags:
		//   Highest bit (0x8) means compressed
		// The other bits are always 011 (0x3) and have unknown meaning:
		//   The last two bits are possibly uncompressed chunk size (512, 1024, 2048, or 4096)
		//   However in NT 3.51, NT 4 SP1, XP SP2, Win 7 SP1 the actual chunk size is always 4096 and the unknown flags are always 011 (0x3)

#ifdef PRINT_WARNINGS
		if ((header & 0x7000) != 0x3000) { PRINT_WARNING("LZNT1 Decompression Warning: Unknown flags used: %x\n", (unsigned int)((header >> 12) & 0x7)); }
#endif

		if (header & 0x8000) // read compressed chunk
		{
			out_size = lznt1_decompress_chunk(in, in+in_size, out, out_end);
			if (out_size == 0) { break; }
		}
		else // read uncompressed chunk
		{
			out_size = in_size;
			if (out + out_size > out_end) { break; } // chunk is longer than the available space
			memcpy(out, in, out_size);
		}
		out += out_size;
		in += in_size;
	}

	// Return insufficient buffer or uncompressed size
	if (in < in_end)
	{
		PRINT_ERROR("LZNT1 Decompression Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	return out - out_start;
}


/////////////////// Decompression Dry-run Functions ////////////////////////////
static size_t lznt1_decompress_chunk_dry_run(const_bytes in, const const_bytes in_end)
{
	const const_bytes in_endx = in_end-0x11; // 1 + 8 * 2 from the end
	size_t out = 0;
	byte flags, flagged;
	uint_fast16_t pow2 = 0x10, mask = 0xFFF, shift = 12;

	// Faster decompression, minimal bounds checking
	while (in < in_endx)
	{
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (flagged)
			{
				uint16_t sym;
				while (out > pow2) { pow2 <<= 1; mask >>= 1; --shift; }
				sym = GET_UINT16(in);
				if (out < ((sym>>shift)+1u)) { errno = E_INVALID_DATA; return 0; }
				out += (sym&mask)+3;
				in += 2;
			}
			else { ++out; ++in; }
			flagged = flags & 0x01;
			flags >>= 1;
		} while (flags);
	}
	
	// Slower decompression but with full bounds checking
	while (in < in_end)
	{
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (in == in_end) { return out; }
			else if (flagged)
			{
				uint16_t sym;
				if (in + 2 > in_end) { errno = E_INVALID_DATA; return 0; }
				while (out > pow2) { pow2 <<= 1; mask >>= 1; --shift; }
				sym = GET_UINT16(in);
				if (out < ((sym>>shift)+1u)) { errno = E_INVALID_DATA; return 0; }
				out += (sym&mask)+3;
				in += 2;
			}
			else { ++out; ++in; }
			flagged = flags & 0x01;
			flags >>= 1;
		} while (flags);
	}

	if (in != in_end) { errno = E_INVALID_DATA; return 0; }
	return out;
}
size_t lznt1_uncompressed_size(const_bytes in, size_t in_len)
{
	size_t out = 0;
	const const_bytes in_end = in+in_len-1;
	uint16_t header;
	uint_fast16_t in_size;

	// Go through every chunk
	while (in < in_end)
	{
		if ((header = GET_UINT16(in)) == 0) { return out; }
		in_size = (header & 0x0FFF)+1;
		if (in+in_size >= in_end) { errno = E_INVALID_DATA; return 0; }
		if (header & 0x8000)
		{
			size_t out_size = lznt1_decompress_chunk_dry_run(in+2, in+2+in_size);
			if (out_size == 0) { return 0; }
			out += out_size;
		}
		else { out += in_size; }
		in += 2 + in_size;
	}

	return out;
}
