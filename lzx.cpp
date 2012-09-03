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

// Decompression code is adapted from 7-zip. See http://www.7-zip.org/.

#include "lzx.h"

#include "LZXCompressionCore.h"
#include "LZXDecompressionCore.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

#ifdef COMPRESSION_API_EXPORT
size_t lzx_wim_max_compressed_size(size_t in_len) { return in_len + 20; }
#endif

static LZXSettings lzx_wim_settings;

size_t lzx_wim_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	return in_len > 0x8000 ? 0 : lzx_compress_core(in, in_len, out, out_len, &lzx_wim_settings);
}
size_t lzx_wim_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	return lzx_decompress_core(in, in_len, out, out_len, &lzx_wim_settings);
}
size_t lzx_wim_uncompressed_size(const_bytes in, size_t in_len)
{
	if (!in_len) { return 0; }
	byte type = *in & 0x7;
	return (type == kBlockTypeVerbatim || type == kBlockTypeAligned || type == kBlockTypeUncompressed) ?
		((*in & 0x8) ? 0x8000 :
		(in_len < 3 ? 0 : (((in[2] << 12) | (in[1] << 4) | (*in >> 4)) & 0xFFFF))) : 0;
}



#ifdef COMPRESSION_API_EXPORT
size_t lzx_cab_max_compressed_size(size_t in_len, unsigned int numDictBits) { return in_len + 4 + ((in_len + 0x7FFF) >> 11); } // TODO: use numDictBits
#endif

size_t lzx_cab_compress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits)
{
	LZXSettings settings(numDictBits);
	return (settings.NumPosLenSlots == 0) ? 0 : lzx_compress_core(in, in_len, out, out_len, &settings);
}
size_t lzx_cab_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits)
{
	LZXSettings settings(numDictBits);
	return (settings.NumPosLenSlots == 0) ? 0 : lzx_decompress_core(in, in_len, out, out_len, &settings);
}
size_t lzx_cab_uncompressed_size(const_bytes in, size_t in_len, unsigned int numDictBits)
{
	LZXSettings settings(numDictBits);
	return (settings.NumPosLenSlots == 0) ? 0 : lzx_decompress_dry_run_core(in, in_len, &settings);
}
