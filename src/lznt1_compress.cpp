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
#include "../include/mscomp/LZNT1Dictionary.h"

#define CHUNK_SIZE 0x1000 // to be compatible with all known forms of Windows

size_t lznt1_max_compressed_size(size_t in_len) { return in_len + 3 + 2 * ((in_len + CHUNK_SIZE - 1) / CHUNK_SIZE); }

struct _mscomp_internal_state
{ // 8,220 - 8,240 bytes (+padding)
  // could reduce padding by 4 bytes by moving out up to right after in
	bool finished; // means fully finished
	LZNT1Dictionary* d;
	byte in[CHUNK_SIZE+2];
	size_t in_needed, in_avail;
	byte out[CHUNK_SIZE+2];
	size_t out_pos, out_avail;
};


/////////////////// Compression Functions /////////////////////////////////////
FORCE_INLINE static uint_fast16_t lznt1_compress_chunk(const_bytes in, uint_fast16_t in_len, bytes out, size_t out_len, LZNT1Dictionary* d)
{
	uint_fast16_t in_pos = 0, out_pos = 0, rem = in_len, pow2 = 0x10, mask3 = 0x1002, shift = 12, len, off, end;
	uint16_t sym;
	byte i, pos, bits, bytes[16]; // if all are special, then it will fill 16 bytes
	d->Reset();

	while (LIKELY(out_pos < out_len && rem))
	{
		// Go through each bit
		for (i = 0, pos = 0, bits = 0; i < 8 && out_pos < out_len && rem; ++i)
		{
			bits >>= 1;

			while (pow2 < in_pos) { pow2 <<= 1; mask3 = (mask3>>1)+1; --shift; }

			if ((len = d->Find(in+in_pos, MIN(rem, mask3), in, &off)) > 0)
			{
				// And new entries
				if (UNLIKELY(!d->Add(in+in_pos, len, rem))) { return 0; }
				in_pos += len; rem -= len;

WARNINGS_PUSH()
WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()
				// Write symbol that is a combination of offset and length
				sym = (uint16_t)(((off-1) << shift) | (len-3));
WARNINGS_POP()

				SET_UINT16(bytes+pos, sym);
				pos += 2;
				bits |= 0x80; // set the highest bit
			}
			else
			{
				// And new entry
				if (UNLIKELY(!d->Add(in+in_pos, rem--))) { return 0; }

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
static bool lznt1_compress_chunk_write(mscomp_stream* stream, const_bytes in, uint_fast16_t in_len)
{
	mscomp_internal_state* state = stream->state;
	uint_fast16_t out_size, flags;
	uint16_t header;
	bool out_buffering = stream->out_avail < in_len+2u;
	bytes out = out_buffering ? state->out : stream->out;

	// Compress the chunk
	out_size = lznt1_compress_chunk(in, in_len, out+2, in_len, state->d);
	if (UNLIKELY(out_size == 0)) { return false; }
	else if (out_size < in_len) // chunk is compressed
	{
		flags = 0xB000;
	}
	else // chunk is uncompressed
	{
		out_size = in_len;
		flags = 0x3000;
		memcpy(out+2, in, in_len); // Note: this does mean that stream->out_avail bytes will be double-copied
	}

	// Save header
	header = (uint16_t)(flags | (out_size-1));
	SET_UINT16(out, header);

	// Advanced output stream (while potentially copy some data from buffers)
	out_size += 2;
	if (out_buffering)
	{
		const size_t copy = MIN(out_size, stream->out_avail);
		memcpy(stream->out, state->out, copy);
		ADVANCE_OUT(stream, copy);
		state->out_pos   = copy;
		state->out_avail = out_size - copy;
	}
	else // not buffered at all
	{
		ADVANCE_OUT(stream, out_size);
	}

	return true;
}
MSCompStatus lznt1_deflate_init(mscomp_stream* stream)
{
	INIT_STREAM(stream, true, MSCOMP_LZNT1);

	mscomp_internal_state* state = (mscomp_internal_state*)malloc(sizeof(mscomp_internal_state));
	if (UNLIKELY(state == NULL)) { SET_ERROR(stream, "LZNT1 Compression Error: Unable to allocate buffer memory"); return MSCOMP_MEM_ERROR; }
	state->finished  = false;
	state->in_needed = 0;
	state->in_avail  = 0;
	state->out_pos   = 0;
	state->out_avail = 0;

	void* d = malloc(sizeof(LZNT1Dictionary));
	if (UNLIKELY(d == NULL)) { free(state); SET_ERROR(stream, "LZNT1 Compression Error: Unable to allocate dictionary memory"); return MSCOMP_MEM_ERROR; }
	state->d = new (d) LZNT1Dictionary();

	stream->state = state;
	return MSCOMP_OK;
}
MSCompStatus lznt1_deflate(mscomp_stream* stream, MSCompFlush flush)
{
	CHECK_STREAM_PLUS(stream, true, MSCOMP_LZNT1, stream->state == NULL || stream->state->finished);

	mscomp_internal_state *state = stream->state;

	DUMP_OUT(state, stream);
	APPEND_IN(state, stream,
		if (flush != MSCOMP_NO_FLUSH && state->in_needed) { return MSCOMP_OK; } /* not enough to compress yet */
		if (UNLIKELY(!lznt1_compress_chunk_write(stream, state->in, (uint_fast16_t)state->in_avail)))
		{
			SET_ERROR(stream, "LZNT1 Compression Error: Unable to allocate dictionary memory");
			return MSCOMP_MEM_ERROR;
		});

	// Compress full chunks while there is room in the output buffer
	while (stream->out_avail && stream->in_avail >= CHUNK_SIZE)
	{
		if (UNLIKELY(!lznt1_compress_chunk_write(stream, stream->in, CHUNK_SIZE)))
		{
			SET_ERROR(stream, "LZNT1 Compression Error: Unable to allocate dictionary memory");
			return MSCOMP_MEM_ERROR;
		}
		ADVANCE_IN(stream, CHUNK_SIZE);
	}

	// Make sure we have depleted input data or output room
	if (stream->out_avail && stream->in_avail)
	{
		ALWAYS(0 < stream->in_avail && stream->in_avail < CHUNK_SIZE);
		if (flush != MSCOMP_NO_FLUSH)
		{
			// Compress a partial chunk
			if (UNLIKELY(!lznt1_compress_chunk_write(stream, stream->in, (uint_fast16_t)stream->in_avail)))
			{
				SET_ERROR(stream, "LZNT1 Compression Error: Unable to allocate dictionary memory");
				return MSCOMP_MEM_ERROR;
			}
		}
		else
		{
			memcpy(state->in, stream->in, stream->in_avail);
			state->in_avail  = stream->in_avail;
			state->in_needed = CHUNK_SIZE - state->in_avail;
		}
		ADVANCE_IN_TO_END(stream);
	}

	if (flush == MSCOMP_FINISH && !state->out_avail)
	{
		state->finished = true;
		// https://msdn.microsoft.com/library/jj679084.aspx: If an End_of_buffer terminal is added, the
		// size of the final compressed data is considered not to include the size of the End_of_buffer terminal.
		if (stream->out_avail >= 2) { stream->out[0] = stream->out[1] = 0; }
		return MSCOMP_STREAM_END;
	}
	return MSCOMP_OK;
}
MSCompStatus lznt1_deflate_end(mscomp_stream* stream)
{
	CHECK_STREAM_PLUS(stream, true, MSCOMP_LZNT1, stream->state == NULL);

	mscomp_internal_state *state = stream->state;

	MSCompStatus status = MSCOMP_OK;
	if (UNLIKELY(!state->finished || stream->in_avail || state->in_avail || state->out_avail)) { SET_ERROR(stream, "LZNT1 Compression Error: End prematurely called"); status = MSCOMP_DATA_ERROR; }

	// Cleanup
	state->d->~LZNT1Dictionary();
	free(state->d);
	free(state);
	stream->state = NULL;

	return status;
}
#ifdef MSCOMP_WITH_OPT_COMPRESS
MSCompStatus lznt1_compress(const_bytes in, size_t in_len, bytes out, size_t* _out_len)
{
	const size_t out_len = *_out_len;
	size_t out_pos = 0, in_pos = 0;
	uint_fast16_t in_size, out_size, flags;
	uint16_t header;
	LZNT1Dictionary d; // requires 512-768 KB of stack space

	while (out_pos < out_len-1 && in_pos < in_len)
	{
		// Compress the next chunk
		in_size = (uint_fast16_t)MIN(in_len-in_pos, 0x1000);
		out_size = lznt1_compress_chunk(in+in_pos, in_size, out+out_pos+2, out_len-out_pos-2, &d);
		if (UNLIKELY(out_size == 0)) { return MSCOMP_MEM_ERROR; }
		else if (out_size < in_size) // chunk is compressed
		{
			flags = 0xB000;
		}
		else // chunk is uncompressed
		{
			if (UNLIKELY(out_pos+2+in_size > out_len)) { return MSCOMP_BUF_ERROR; }
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
	if (UNLIKELY(in_pos < in_len)) { return MSCOMP_BUF_ERROR; }
	// https://msdn.microsoft.com/library/jj679084.aspx: If an End_of_buffer terminal is added, the
	// size of the final compressed data is considered not to include the size of the End_of_buffer terminal.
	if (out_len-out_pos >= 2) { out[out_pos] = out[out_pos+1] = 0; }
	*_out_len = out_pos;
	return MSCOMP_OK;
}
#else
ALL_AT_ONCE_WRAPPER_COMPRESS(lznt1)
#endif

#endif
