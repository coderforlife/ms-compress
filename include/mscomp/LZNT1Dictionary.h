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


/////////////////// LZNT1 Dictionary //////////////////////////////////////////////////////////////
// The dictionary system used for LZNT1 compression that favors speed over memory usage.
// Most of the compression time is spent in the dictionary, particularly Find (54%) and Add (37%).
//
// The base memory usage is 512 KB (or 768 KB on 64-bit systems). More memory is always allocated
// but only as much as needed. Larger sized chunks will consume more memory. For a series of 4 KB
// chunks (what LZNT1 uses), the extra consumed memory averages about 40 KB (80 KB on 64-bit), but
// could theoretically grow to 4 MB (8 MB on 64-bit).
//
// This implementation is about twice as fast as the SA version but consumes about 12-20x as much
// memory on average (and up to 112-220x as much) and requires dynamic allocations.

#include "internal.h"
#ifdef MSCOMP_WITH_LZNT1_SA_DICT
#include "LZNT1Dictionary_SA.h"
#endif

#ifndef MSCOMP_LZNT1_DICTIONARY_H
#define MSCOMP_LZNT1_DICTIONARY_H

class LZNT1Dictionary // 512+ KB (768+ KB on 64-bit systems)
{
private:
	// An entry within the dictionary, using a dynamically resized array of positions
	struct Entry // 6+ bytes (10+ bytes on 64-bit systems)
	{
		const_bytes* pos;
		uint16_t cap;
		INLINE bool add(const_bytes data, uint16_t& size)
		{
			if (size >= this->cap)
			{
				const_bytes *temp = (const_bytes*)realloc((bytes*)this->pos, (this->cap=(this->cap?(this->cap<<1):8))*sizeof(const_bytes));
				if (UNLIKELY(temp == NULL)) { return false; }
				this->pos = temp;
			}
			this->pos[size++] = data;
			return true;
		}
	};

	// The dictionary
	Entry entries[0x100*0x100];  //Entry entries[0x100][0x100];  // 384/640 KB
	uint16_t sizes[0x100*0x100]; //uint16_t sizes[0x100][0x100]; // 128 KB

public:
	INLINE LZNT1Dictionary()
	{
		// need to set pos to NULL and cap to 0
		memset(this->entries, 0, 0x100*0x100*sizeof(Entry));
	}

	INLINE ~LZNT1Dictionary()
	{
		uint32_t idx;
		for (idx = 0; idx < 0x100*0x100; ++idx)
		{
			free(this->entries[idx].pos);
		}
	}

	// Resets a dictionary, ready to start a new chunk
	// This should also be called before any Add/Find
	INLINE void Reset()
	{
		memset(this->sizes, 0, 0x100*0x100*sizeof(uint16_t));
	}
	
	// Adds data to the dictionary, which will be used as a starting point during future finds
	// Max length is how many bytes can be read from data, regardless of the end of the chunk
	INLINE bool Add(const_bytes data, const size_t max_len)
	{
		if (LIKELY(max_len >= 2))
		{
			const uint_fast16_t idx = data[0] << 8 | data[1];
			return this->entries[idx].add(data, this->sizes[idx]);
		}
		return true;
	}
	INLINE bool Add(const_bytes data, size_t len, const size_t max_len)
	{
		if (len > max_len - 2)
		{
			len = max_len - 2;
		}
		byte x, y = data[0];
		for (const_bytes end = data + len; data < end; ++data)
		{
			x = y; y = data[1];
			if (UNLIKELY(!this->entries[x << 8 | y].add(data, this->sizes[x << 8 | y]))) { return false; }
		}
		return true;
	}
	
WARNINGS_PUSH()
WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()

	// Finds the best symbol in the dictionary for the data
	// Returns the length of the string found, or 0 if nothing of length >= 3 was found
	// offset is set to the offset from the current position to the string
	INLINE int_fast16_t Find(const_bytes data, const int_fast16_t max_len, const_bytes search, int_fast16_t* offset) const
	{
		if (LIKELY(max_len >= 3 && data-search > 0))
		{
			const byte x = data[0], y = data[1];
			const uint_fast16_t idx = data[0] << 8 | data[1];
			const Entry* e = &this->entries[idx];
			const uint16_t size = this->sizes[idx];
			if (size) // a match is possible
			{
				const byte z = data[2];
				int_fast16_t l = 0, o;
				int_fast32_t ep = size - 1; // need to support all uint16 values and <0

				// Try short repeats - this does not use the Dictionary at all
				if (x == z && y == data[-1])
				{
					if (x == y) // x == y == z == data[-1]
					{
						// Repeating the last byte
						o = 1;
						l = 3;
						while (l < max_len && data[l] == x)	{ ++l; }
						--ep;
						if (data-search > 1 && x == data[-2])
						{
							--ep;
						}

						// Found the best match, stop now
						if (l == max_len) { *offset = o; return l; }
					}
					else if (data-search > 1 && x == data[-2]) // x == z == data[-2], y == data[-1]
					{
						// Repeating the last two bytes
						o = 2;
						l = 3;
						while (l < max_len && data[l] == y)	{ ++l; if (l < max_len && data[l] == x) { ++l; } else break; }
						--ep;

						// Found the best match, stop now
						if (l == max_len) { *offset = o; return l; }
					}
				}

				// Do an exhaustive search (with the possible positions)
				for (; ep >= 0 && e->pos[ep] >= search; --ep)
				{
					const const_bytes ss = e->pos[ep];
					if (ss[2] == z)
					{
						const_bytes s = ss+3;
						int_fast16_t i;
						for (i = 3; i < max_len && data[i] == *s; i++, s++);
						if (i > l) { o = (int_fast16_t)(data-ss); l = i; if (l == max_len) { break; } }
					}
				}

				// Found a match, return it
				if (l >= 3)
				{
					*offset = o;
					return l;
				}
			}
		}

		// No match found, return 0
		return 0;
	}
};

WARNINGS_POP()

#endif
