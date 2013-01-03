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


#include "xpress.h"

#include "XpressDictionary.h"

#define MIN_DATA	5

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

typedef XpressDictionary<0x2000> Dictionary;

#ifdef COMPRESSION_API_EXPORT
size_t xpress_max_compressed_size(size_t in_len) { return in_len + 4 * ((in_len + 31) / 32); }
#endif


////////////////////////////// Compression Functions ///////////////////////////////////////////////
size_t xpress_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes                  in_end  = in +in_len,  in_end3  = in_end  - 3;
	const const_bytes out_start = out, out_end = out+out_len, out_end1 = out_end - 1;
	const_bytes filled_to = in;

	uint32_t flags = 0, *out_flags = (uint32_t*)out;
	byte flag_count;
	byte* half_byte = NULL;
	
	Dictionary d(in, in_end);

	if (in_len == 0) { return 0; }
	if (out_len < MIN_DATA) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	out += 4;		// skip four for flags
	*out++ = *in++;	// copy the first byte
	flag_count = 1;

	while (in < in_end3 && out < out_end1)
	{
		uint32_t len, off;
		if (filled_to <= in) { filled_to = d.Fill(filled_to); }
		if ((len = d.Find(in, &off)) < 3) { *out++ = *in++; flags <<= 1; } // Copy byte
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
					*(half_byte=out++) = (byte)(MIN(len, 0xF));
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
							SET_UINT16(out, len);
							out += 2;
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
	// Finish shifting over flags and set all unused bytes to 1
	// Note: the shifting math does not effect flags at all when flag_count == 0, resulting in a copy of the previous flags so the proper value must be set manually
	// RTL produces improper output in this case as well, so the decompressor still must tolerate bad flags at the very end
	flags = flag_count ? (flags << (32 - flag_count)) | ((1 << (32 - flag_count)) - 1) : 0xFFFFFFFF;
	SET_UINT32(out_flags, flags);
	if (in != in_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	return out - out_start;
}


////////////////////////////// Decompression Functions /////////////////////////////////////////////
size_t xpress_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	const const_bytes                  in_end  = in +in_len,  in_endx  = in_end -0x054; // 4 + 32 * (2 + 0.5) from the end, or maybe 4 + 32 * (2 + 0.5 + 1 + 2 + 4) = 0x134
	const const_bytes out_start = out, out_end = out+out_len, out_endx = out_end-0x160; // 32 * (3 + 8) from the end
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
		flags = GET_UINT32(in);
		flagged = flags & 0x80000000;
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
				in += 2;
				off = (sym >> 3) + 1;
				len = sym & 0x7;
				if (len == 0x7)
				{
					if (half_byte) { len = *half_byte >> 4; half_byte = NULL; }
					else           { len = *in & 0xF;       half_byte = in++; }
					if (len == 0xF)
					{
						if (in + 7 > in_endx) { goto CHECKED_LENGTH; }
						if ((len = *(in++)) == 0xFF)
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

				o = out-off;
				if (o < out_start) { PRINT_ERROR("Xpress Decompression Error: Invalid data: Illegal offset (%p-%u < %p)\n", out, off, out_start); errno = E_INVALID_DATA; return 0; }

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
					}
				}
				flagged = flags & 0x80000000;
				flags <<= 1;
			}
			else if ((flags & 0x80000000) != 0)  // Copy 1 byte directly
			{
				*out++ = *in++;
				flagged = 1;
				flags <<= 1;
			}
			else if ((flags & 0x40000000) != 0) // Copy 2 bytes directly
			{
				*(uint16_t*)out = *(uint16_t*)in;
				out += 2;
				in += 2;
				flagged = 1;
				flags <<= 2;
			}
			else // Copy 3 or 4 bytes directly
			{
				byte n = (flags & 0x20000000) ? 3 : 4;
				*(uint32_t*)out = *(uint32_t*)in;
				out += n;
				in += n;
				flagged = (flags & 0x30000000);
				flags <<= n;
			}
		} while (flags);
	}

	// Slower decompression but with full bounds checking
	while (in + 4 < in_end)
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

	if (in == in_end || in + 4 == in_end) { return out - out_start; }
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
	while (in + 4 < in_end)
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
	
	return (in == in_end || in + 4 == in_end) ? out : 0;
}
