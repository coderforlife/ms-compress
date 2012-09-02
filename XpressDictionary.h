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


/////////////////// Dictionary /////////////////////////////////////////////////
// The dictionary system used for Xpress compression.
//
// TODO: ? Most of the compression time is spent in the dictionary - particularly Find and Add.

#ifndef XPRESS_DICTIONARY_H
#define XPRESS_DICTIONARY_H
#include "compression-api.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

#include "LCG.h"

template<uint32_t MaxOffset, uint32_t ChunkSize = MaxOffset, uint32_t MaxHash = 0x8000>
class XpressDictionary // 192 kb (on 32-bit) or 384 kb (on 64-bit)
{
	//TODO: CASSERT(IS_POW2(ChunkSize));
	CASSERT(MaxOffset <= ChunkSize);

private:
	// Define a LCG-generate hash table
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4309) // warning C4309: 'specialization' : truncation of constant value
#endif
	typedef LCG<0x2a190348ul, 0x41C64E6Du, 12345u, 1ull << 32, 16, MaxHash> lcg;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	const_bytes start, end, end3;
#ifdef LARGE_STACK
	const_bytes table[MaxHash];
	const_bytes window[ChunkSize*2];
#else
	const_bytes* table;
	const_bytes* window;
#endif
	
	inline uint32_t WindowPos(const_bytes x) const { return (uint32_t)((x - this->start) & ((ChunkSize << 1) - 1)); } // { return (uint32_t)((x - this->start) % (2 * ChunkSize)); }
	inline static uint32_t GetMatchLength(const_bytes a, const_bytes b, const const_bytes end, const const_bytes end4)
	{
		// like memcmp but tells you the length of the match and optimized
		// assumptions: a < b < end, end4 = end - 4
		const const_bytes b_start = b;
		byte a0, b0;
		while (b < end4 && *((uint32_t*)a) == *((uint32_t*)b))
		{
			a += sizeof(uint32_t);
			b += sizeof(uint32_t);
		}
		do
		{
			a0 = *a++;
			b0 = *b++;
		} while (b < end && a0 == b0);
		return (uint32_t)(b - b_start - 1);
	}

public:
	XpressDictionary(const const_bytes start, const const_bytes end) : start(start), end(end), end3(end - 3)
	{
#ifndef LARGE_STACK
		this->table  = (const_bytes*)malloc(MaxHash    *sizeof(const_bytes));
		this->window = (const_bytes*)malloc(ChunkSize*2*sizeof(const_bytes));
#endif
		memset(this->table, 0, MaxHash*sizeof(const_bytes));
	}
#ifndef LARGE_STACK
	~XpressDictionary()
	{
		free(this->table);
		free(this->window);
	}
#endif

	const_bytes Fill(const_bytes data)
	{
		uint32_t pos = WindowPos(data); // either 0x00000 or 0x02000 / 0x10000
		const const_bytes end = ((data + ChunkSize) < this->end3) ? data + ChunkSize : this->end3;
		while (data < end)
		{
			const uint32_t hash = lcg::Hash(data);
			this->window[pos++] = this->table[hash];
			this->table[hash] = data++;
		}
		return end;
	}

	uint32_t Find(const const_bytes data, uint32_t* offset) const
	{
#if PNTR_BITS <= 32
		const const_bytes end = this->end; // with 32-bits and less the data + UINT32_MAX will always overflow
#else
		const const_bytes end = ((data + UINT32_MAX) < this->end) ? data + UINT32_MAX : this->end;
#endif
		const const_bytes xend = data - MaxOffset, end4 = end - 4;
		const_bytes x;
		uint32_t len = 2;
		for (x = this->window[WindowPos(data)]; x >= xend; x = this->window[WindowPos(x)])
		{
			const uint32_t l = GetMatchLength(x, data, end, end4);
			if (l > len)
			{
				*offset = (uint32_t)(data - x);
				len = l;
			}
		}
		return len;
	}
};

#endif
