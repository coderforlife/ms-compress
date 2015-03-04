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

#ifdef MSCOMP_WITH_LZNT1

#include "../include/lznt1.h"

#define CHUNK_SIZE 0x1000 // to be compatible with all known forms of Windows

struct _mscomp_internal_state
{ // 8,214 - 8,230 bytes (+padding)
	bool end_of_stream;
	byte in[CHUNK_SIZE+2];
	size_t in_needed, in_avail;
	byte out[CHUNK_SIZE];
	size_t out_pos, out_avail;
};


/////////////////// Decompression Functions ///////////////////////////////////
static MSCompStatus lznt1_decompress_chunk(const_bytes in, const const_bytes in_end, bytes out, const const_bytes out_end, size_t* _out_len)
{
	const const_bytes                  in_endx  = in_end -0x11; // 1 + 8 * 2 from the end
	const const_bytes out_start = out, out_endx = out_end-0x58; // 8 * (3 + 8) from the end
	byte flags, flagged;
	
	uint_fast16_t pow2 = 0x10, mask = 0xFFF, shift = 12;
	const_bytes pow2_target = out_start + 0x10;
	uint_fast16_t len, off;

	// Most of the decompression happens here
	// Very few bounds checks are done but we can only go to near the end and not the end
	while (LIKELY(in < in_endx && out < out_endx))
	{
		// Handle a fragment
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (flagged)  // Offset/length symbol
			{
				// Offset/length symbol
				while (UNLIKELY(out > pow2_target)) { pow2 <<= 1; pow2_target = out_start + pow2; mask >>= 1; --shift; } // Update the current power of two available bytes
				uint16_t sym = GET_UINT16(in);
				off = (sym>>shift)+1;
				len = (sym&mask)+3;
				in += 2;
				const_bytes o = out-off;
				if (UNLIKELY(o < out_start)) { /*SET_ERROR(stream, "LZNT1 Decompression Error: Invalid data: Illegal offset (%p-%u < %p)", out, off, out_start);*/ return MSCOMP_DATA_ERROR; }

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
								//if (out > out_end) { printf("A %p %p %zu\n", out, out_end, out_end-out_start); return MSCOMP_BUF_ERROR; }
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
							//if (out > out_end) { printf("B %p %p %zu\n", out, out_end, out_end-out_start); return MSCOMP_BUF_ERROR; }
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
		} while (LIKELY(flags));
	}
	
	// Slower decompression but with full bounds checking
	while (LIKELY(in < in_end))
	{
		// Handle a fragment
		flagged = (flags = *in++) & 0x01;
		flags = (flags >> 1) | 0x80;
		do
		{
			if (in == in_end) { *_out_len = out - out_start; return MSCOMP_OK; }
			else if (flagged) // Offset/length symbol
			{
				// Offset/length symbol
				if (UNLIKELY(in + 2 > in_end)) { /*SET_ERROR(stream, "LZNT1 Decompression Error: Invalid data: Unable to read 2 bytes for offset/length");*/ return MSCOMP_DATA_ERROR; }
				while (UNLIKELY(out > pow2_target)) { pow2 <<= 1; pow2_target = out_start + pow2; mask >>= 1; --shift; } // Update the current power of two available bytes
				{
					uint16_t sym = GET_UINT16(in);
					off = (sym>>shift)+1;
					len = (sym&mask)+3;
				}
				in += 2;
				if (UNLIKELY(out - off < out_start)) { /*SET_ERROR(stream, "LZNT1 Decompression Error: Invalid data: Illegal offset (%p-%u < %p)", out, off, out_start);*/ return MSCOMP_DATA_ERROR; }
				//if (out + len > out_end) { printf("C %p %p %zu\n", out + len, out_end, out_end-out_start); return MSCOMP_BUF_ERROR; }

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
			//else if (out == out_end) { printf("D %p %p %zu\n", out, out_end, out_end-out_start); return MSCOMP_BUF_ERROR; }
			else { *out++ = *in++; } // Copy byte directly
			flagged = flags & 0x01;
			flags >>= 1;
		} while (LIKELY(flags));
	}

	if (UNLIKELY(in != in_end)) { /*SET_ERROR(stream, "LZNT1 Decompression Error: Invalid data: Unable to read byte for flags");*/ return MSCOMP_DATA_ERROR; }
	*_out_len = out - out_start;
	return MSCOMP_OK;
}
MSCompStatus lznt1_decompress_chunk_read(mscomp_stream* const stream, const_bytes const in, size_t* const in_len)
{
	mscomp_internal_state *state = stream->state;

	// Read chunk header
	const uint16_t header = GET_UINT16(in);
	if (UNLIKELY(header == 0))
	{
		if (UNLIKELY((stream->in_avail+state->in_avail) != 2)) { SET_ERROR(stream, "LZNT1 Decompression Warning: End-of-stream found with data left"); return MSCOMP_DATA_ERROR; }
		*in_len = 2;
		state->end_of_stream = true;
		return MSCOMP_OK;
	}
	const uint_fast16_t in_size = (header & 0x0FFF)+3; // +3 includes +2 for header
	if (in_size > *in_len)
	{
		// We don't have the entire compressed chunk yet
		memcpy(state->in, in, *in_len);
		state->in_needed = in_size - *in_len;
		state->in_avail  = *in_len;
		return MSCOMP_OK;
	}
	*in_len = in_size;

	// Flags:
	//   Highest bit (0x8) means compressed
	// The other bits are always 011 (0x3) and have unknown meaning:
	//   The last two bits are possibly uncompressed chunk size (512, 1024, 2048, or 4096)
	//   However in NT 3.51, NT 4 SP1, XP SP2, Win 7 SP1 the actual chunk size is always 4096 and the unknown flags are always 011 (0x3)
	// The online description says it must always be 3.
	if (UNLIKELY((header & 0x7000) != 0x3000)) { SET_ERROR(stream, "LZNT1 Decompression Error: Invalid header signature: %x", (unsigned int)((header >> 12) & 0x7)); return MSCOMP_DATA_ERROR; }
	if (header & 0x8000) // read compressed chunk
	{
		if (stream->out_avail < CHUNK_SIZE)
		{
			// buffer decompression
			size_t out_size;
			MSCompStatus status = lznt1_decompress_chunk(in+2, in+in_size, state->out, state->out+CHUNK_SIZE, &out_size);
			if (UNLIKELY(status != MSCOMP_OK))
			{
#ifdef MSCOMP_WITH_ERROR_MESSAGES
				if (UNLIKELY(status == MSCOMP_BUF_ERROR)) { SET_ERROR(stream, "LZNT1 Decompression Error: Uncompressed chunk size exceeds expected size"); }
#endif
				return MSCOMP_DATA_ERROR;
			}
			const size_t copy = MIN(out_size, stream->out_avail);
			memcpy(stream->out, state->out, copy);
			state->out_pos   = copy;
			state->out_avail = out_size - copy;
			ADVANCE_OUT(stream, copy);
		}
		else
		{
			// direct decompress
			size_t out_size;
			MSCompStatus status = lznt1_decompress_chunk(in+2, in+in_size, stream->out, stream->out+CHUNK_SIZE, &out_size);
			if (UNLIKELY(status != MSCOMP_OK))
			{
#ifdef MSCOMP_WITH_ERROR_MESSAGES
				if (UNLIKELY(status == MSCOMP_BUF_ERROR)) { SET_ERROR(stream, "LZNT1 Decompression Error: Uncompressed chunk size exceeds expected size"); }
#endif
				return MSCOMP_DATA_ERROR;
			}
			ADVANCE_OUT(stream, out_size);
		}
	}
	else // read uncompressed chunk
	{
		const size_t out_size = in_size-2;
		if (stream->out_avail < out_size)
		{
			// chunk is longer than the available output space
			memcpy(stream->out, in + 2, stream->out_avail);
			memcpy(state->out, in + 2 + stream->out_avail, out_size - stream->out_avail);
			state->out_pos   = 0;
			state->out_avail = out_size - stream->out_avail;
			ADVANCE_OUT_TO_END(stream);
		}
		else
		{
			// direct copy
			memcpy(stream->out, in + 2, out_size);
			ADVANCE_OUT(stream, out_size);
		}
	}

	return MSCOMP_OK;
}
MSCompStatus lznt1_inflate_init(mscomp_stream* stream)
{
	INIT_STREAM(stream, false, MSCOMP_LZNT1);

	mscomp_internal_state* state = (mscomp_internal_state*)malloc(sizeof(mscomp_internal_state));
	if (UNLIKELY(state == NULL)) { SET_ERROR(stream, "LZNT1 Decompression Error: Unable to allocate buffer memory"); return MSCOMP_MEM_ERROR; }
	state->end_of_stream = false;
	state->in_needed = 0;
	state->in_avail  = 0;
	state->out_pos   = 0;
	state->out_avail = 0;

	stream->state = state;
	return MSCOMP_OK;
}
MSCompStatus lznt1_inflate(mscomp_stream* stream)
{
	CHECK_STREAM_PLUS(stream, false, MSCOMP_LZNT1, stream->state == NULL);

	mscomp_internal_state *state = stream->state;

	DUMP_OUT(state, stream);
	if (UNLIKELY(state->end_of_stream))
	{
		if (UNLIKELY(stream->in_avail || state->in_avail)) { SET_ERROR(stream, "LZNT1 Decompression Warning: End-of-stream found with data left"); return MSCOMP_DATA_ERROR; }
		return MSCOMP_STREAM_END;
	}

	APPEND_IN(state, stream,
		if (state->in_needed) { return MSCOMP_OK; }
		size_t in_len = state->in_avail;
		MSCompStatus status = lznt1_decompress_chunk_read(stream, state->in, &in_len);
		if (UNLIKELY(in_len != state->in_avail)) { SET_ERROR(stream, "LZNT1 Decompression Error: Inconsistent stream state"); return MSCOMP_ARG_ERROR; }
		else if (UNLIKELY(status != MSCOMP_OK)) { return status; }
		else if (UNLIKELY(state->end_of_stream)) { return MSCOMP_STREAM_END; }
		else if (state->in_needed) { continue; } // we only had enough to read the header this time
	);
	
	// Decompress full chunks while there is room in the output buffer
	while (stream->out_avail && stream->in_avail >= 2)
	{
		size_t in_size = stream->in_avail;
		MSCompStatus status = lznt1_decompress_chunk_read(stream, stream->in, &in_size);
		if (UNLIKELY(status != MSCOMP_OK)) { return status; }
		ADVANCE_IN(stream, in_size);
		if (UNLIKELY(state->end_of_stream)) { return MSCOMP_STREAM_END; }
	}

	if (stream->out_avail && stream->in_avail)
	{
		ALWAYS(stream->in_avail == 1);
		state->in[0] = stream->in[0];
		state->in_needed = 1; // we need 1 more just to determine how many more we actually need
		state->in_avail  = 1;
		stream->in += 1; stream->in_total += 1; stream->in_avail = 0;
	}

	return UNLIKELY(!stream->in_avail && !state->in_avail && !state->out_avail) ? MSCOMP_POSSIBLE_STREAM_END : MSCOMP_OK;
}
MSCompStatus lznt1_inflate_end(mscomp_stream* stream)
{
	CHECK_STREAM_PLUS(stream, false, MSCOMP_LZNT1, stream->state == NULL);

	mscomp_internal_state* state = stream->state;

	MSCompStatus status = MSCOMP_OK;
	if (UNLIKELY(stream->in_avail || state->in_avail || state->out_avail)) { SET_ERROR(stream, "LZNT1 Decompression Error: End prematurely called"); status = MSCOMP_DATA_ERROR; }

	free(state);
	stream->state = NULL;

	return status;
}
#ifdef NOT_OPTIMAL___MSCOMP_WITH_OPT_DECOMPRESS // NOTE: the "optimized" version is actually slower!
MSCompStatus lznt1_decompress(const_bytes in, size_t in_len, bytes out, size_t* _out_len)
{
	const size_t out_len = *_out_len;
	const const_bytes in_end  = in  + in_len-1;
	const const_bytes out_end = out + out_len, out_start = out;

	// Go through every chunk
	while (in < in_end && out < out_end)
	{
		// Read chunk header
		const uint16_t header = GET_UINT16(in);
		if (UNLIKELY(header == 0)) { *_out_len = out - out_start; return MSCOMP_OK; }
		const uint_fast16_t in_size = (header & 0x0FFF)+1;
		if (UNLIKELY(in+in_size >= in_end)) { return MSCOMP_DATA_ERROR; }
		in += 2;

		// Flags:
		//   Highest bit (0x8) means compressed
		// The other bits are always 011 (0x3) and have unknown meaning:
		//   The last two bits are possibly uncompressed chunk size (512, 1024, 2048, or 4096)
		//   However in NT 3.51, NT 4 SP1, XP SP2, Win 7 SP1 the actual chunk size is always 4096 and the unknown flags are always 011 (0x3)

		size_t out_size;
		if (header & 0x8000) // read compressed chunk
		{
			MSCompStatus err = lznt1_decompress_chunk(in, in+in_size, out, out_end, &out_size);
			if (err != MSCOMP_OK) { return err; }
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
	if (in < in_end) { return MSCOMP_BUF_ERROR; }
	*_out_len = out - out_start;
	return MSCOMP_OK;
}
#else
ALL_AT_ONCE_WRAPPER_DECOMPRESS(lznt1)
#endif

#endif