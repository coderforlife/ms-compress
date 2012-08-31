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

#ifndef LZX_DECOMPRESSION_CORE_H
#define LZX_DECOMPRESSION_CORE_H
#include "compression-api.h"

#include "LZXConstants.h"

size_t lzx_decompress_core(const_bytes in, size_t in_len, bytes out, size_t out_len, bool wimMode, uint32_t numPosLenSlots);
size_t lzx_decompress_dry_run_core(const_bytes in, size_t in_len, uint32_t numPosLenSlots);

#endif
