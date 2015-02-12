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

#ifdef MSCOMP_WITH_XPRESS

#define MIN_DATA	5

#include "../include/xpress.h"
#include "../include/mscomp/XpressDictionary.h"
//#include "../include/mscomp/XpressDictionaryStatic.h"

typedef XpressDictionary<0x2000> Dictionary;
//typedef XpressDictionaryStatic<0x2000> DictionaryStatic;

size_t xpress_max_compressed_size(size_t in_len) { return in_len + 4 + 4 * (in_len / 32); }

////////////////////////////// Compression Functions ///////////////////////////////////////////////
#define PRINT_ERROR(...) // TODO: remove
// TODO: streaming-compression not made yet
MSCompStatus xpress_deflate_init(mscomp_stream* stream)
{
	INIT_STREAM(stream, true, MSCOMP_XPRESS);
	return MSCOMP_MEM_ERROR;

	//mscomp_internal_state* state = (mscomp_internal_state*)malloc(sizeof(mscomp_internal_state));
	//if (UNLIKELY(state == NULL)) { SET_ERROR(stream, "XPRESS Compression Error: Unable to allocate buffer memory"); return MSCOMP_MEM_ERROR; }
	////state->finished  = false;
	////state->in_needed = 0;
	////state->in_avail  = 0;
	////state->out_pos   = 0;
	////state->out_avail = 0;

	//DictionaryStatic* d = (DictionaryStatic*)malloc(sizeof(DictionaryStatic));
	//if (UNLIKELY(d == NULL)) { free(state); SET_ERROR(stream, "XPRESS Compression Error: Unable to allocate dictionary memory"); return MSCOMP_MEM_ERROR; }
	//if (!(d = new (d) DictionaryStatic())->Initialized()) { free(state); d->~DictionaryStatic(); free(d); SET_ERROR(stream, "XPRESS Compression Error: Unable to allocate dictionary memory"); return MSCOMP_MEM_ERROR; }
	//state->d = d;

	//stream->state = state;
	//return MSCOMP_OK;
}
MSCompStatus xpress_deflate(mscomp_stream* stream, bool finish)
{
	// There will be one conceptual difference between the streaming and non-streaming versions.
	// The streaming version has to deal with the fact that the two partnered half-bytes might be
	// very far apart. To not take up too much memory, after 16kb (or more - up to 128kb?) of not
	// finding a partner for a half-byte the partner will be assumed to be 0x0 (forcing a length
	// of 10 the next time a length 10+ match is found). This adds at most 2 bytes to the output
	// for data that is already not compressing well (each time it occurs).
	return MSCOMP_ARG_ERROR;
}
MSCompStatus xpress_deflate_end(mscomp_stream* stream)
{
	CHECK_STREAM_PLUS(stream, true, MSCOMP_XPRESS, stream->state == NULL);

	MSCompStatus status = MSCOMP_OK;
	//if (UNLIKELY(!stream->state->finished || stream->in_avail || stream->state->in_avail || stream->state->out_avail)) { SET_ERROR(stream, "XPRESS Compression Error: End prematurely called"); status = MSCOMP_DATA_ERROR; }

	//// Cleanup
	//stream->state->d->~DictionaryStatic();
	//free(stream->state->d);
	//free(stream->state);
	//stream->state = NULL;

	return status;
}

MSCompStatus xpress_compress(const_bytes in, size_t in_len, bytes out, size_t* _out_len)
{
	const size_t out_len = *_out_len;
	const const_bytes                  in_end  = in +in_len,  in_end3  = in_end  - 3;
	const const_bytes out_start = out, out_end = out+out_len, out_end1 = out_end - 1;
	const_bytes filled_to = in;

	uint32_t flags = 0, *out_flags = (uint32_t*)out;
	byte flag_count;
	byte* half_byte = NULL;
	
	Dictionary d(in, in_end);

	if (in_len == 0)
	{
		if (UNLIKELY(out_len < 4)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
		SET_UINT32(out, 0xFFFFFFFF);
		*_out_len = 4;
		return MSCOMP_OK;
	}
	if (out_len < MIN_DATA) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
	if (!d.Initialized()) { return MSCOMP_MEM_ERROR; }

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
					if (out >= out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
					*(half_byte=out++) = (byte)(MIN(len, 0xF));
				}
				if (len >= 0xF)
				{
					len -= 0xF;
					if (UNLIKELY(out >= out_end)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
					*out++ = (byte)MIN(len, 0xFF);
					if (len >= 0xFF)
					{
						len += 0xF+0x7;
						if (len <= 0xFFFF)
						{
							if (UNLIKELY(out + 2 > out_end)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
							SET_UINT16(out, len);
							out += 2;
						}
						else
						{
							if (UNLIKELY(out + 6 > out_end)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
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
			if (UNLIKELY(out + 4 > out_end)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
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
			if (out + 4 > out_end) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
			out_flags = (uint32_t*)out;
			out += 4;
		}
	}
	// Finish shifting over flags and set all unused bytes to 1
	// Note: the shifting math does not effect flags at all when flag_count == 0, resulting in a copy of the previous flags so the proper value must be set manually
	// RTL produces improper output in this case as well, so the decompressor still must tolerate bad flags at the very end
	if (UNLIKELY(in != in_end)) { PRINT_ERROR("Xpress Compression Error: Insufficient buffer"); return MSCOMP_BUF_ERROR; }
	flags = flag_count ? (flags << (32 - flag_count)) | ((1 << (32 - flag_count)) - 1) : 0xFFFFFFFF;
	SET_UINT32(out_flags, flags);
	*_out_len = out - out_start;
	return MSCOMP_OK;
}

#endif
