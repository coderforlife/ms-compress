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
// The dictionary system used for LZX compression (WIM style).
//
// TODO: ? Most of the compression time is spent in the dictionary - particularly Find and Add.

#ifndef LZX_DICTIONARY_WIM_H
#define LZX_DICTIONARY_WIM_H
#include "compression-api.h"

#include "LZXConstants.h"

class LZXDictionaryWIM
{
private:
	static const unsigned int MaxHash = 0x8000;

	const_bytes start, end, end2;
#ifdef LARGE_STACK
	const_bytes table[MaxHash];
	const_bytes window[0x8000];
#else
	const_bytes* table;
	const_bytes* window;
#endif

	inline static uint_fast16_t Hash(const const_bytes x) { return (x[0] | (x[1] << 8)) & (MaxHash - 1); }
	inline uint32_t WindowPos(const_bytes x) const { return (uint32_t)(x - this->start); }
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
	inline LZXDictionaryWIM(const const_bytes start, uint32_t in_len) : start(start), end(start + in_len), end2(end - 2)
	{
#ifndef LARGE_STACK
		this->table  = (const_bytes*)malloc(MaxHash*sizeof(const_bytes));
		this->window = (const_bytes*)malloc(in_len *sizeof(const_bytes));
#endif
		memset(this->table,  0, MaxHash*sizeof(const_bytes));
		memset(this->window, 0, in_len *sizeof(const_bytes));
	}
#ifndef LARGE_STACK
	inline ~LZXDictionaryWIM()
	{
		free(this->table);
		free(this->window);
	}
#endif
	
	inline void Add(const_bytes data)
	{
		if (data < this->end2)
		{
			const uint32_t hash = Hash(data);
			this->window[WindowPos(data)] = this->table[hash];
			this->table[hash] = data;
		}
	}

	inline void Add(const_bytes data, size_t len)
	{
		uint32_t pos = WindowPos(data);
		const const_bytes end = ((data + len) < this->end2) ? data + len : this->end2;
		while (data < end)
		{
			const uint32_t hash = Hash(data);
			this->window[pos++] = this->table[hash];
			this->table[hash] = data++;
		}
	}

	inline uint32_t Find(const const_bytes data, uint32_t* offset) const
	{
		const const_bytes end = ((data + 257) >= this->end) ? this->end : data + 257; // if overflow or past end use the end, otherwise go MaxLength away
		const const_bytes xend = data - (0x8000 - 3), end4 = end - 4;
		const_bytes x;
		uint32_t len = 1;
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
