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
// The dictionary system used for LZNT1 compression.
// Most of the compression time is spent in the dictionary - particularly Find (54%) and Add (37%)

// Implementation designed for being extremely fast at the expense of memory
// usage. The base memory usage is 512 KB (or 768 KB on 64-bit systems). More
// memory is always allocated but only as much as needed. Larger sized chunks
// will consume more memory. For a series of 4 KB chunks, the extra consumed
// memory is around 20-80 KB. For a series of 64 KB chunks, it is 200-800 KB.

// This implementation is ~30x faster than the original 576 KB fixed-size dictionary!

#ifndef LZNT1_DICTIONARY_H
#define LZNT1_DICTIONARY_H
#include "compression-api.h"

class LZNT1Dictionary // 512+ KB (768+ KB on 64-bit systems)
{
private:
	// An entry within the dictionary, using a dynamically resized array of positions
	struct Entry // 6+ bytes (10+ bytes on 64-bit systems)
	{
		const_bytes* pos;
		uint16_t cap;
		inline void add(const_bytes data, uint16_t& size)
		{
			if (size >= this->cap)
			{
				const_bytes *temp = (const_bytes*)realloc((bytes*)this->pos, (this->cap=(this->cap?((this->cap==0x8000)?0xFFFF:(this->cap<<1)):8))*sizeof(const_bytes));
				if (temp == NULL)
				{
					// TODO: throw memory error
				}
				this->pos = temp;
			}
			this->pos[size++] = data;
		}
	};

	// The dictionary
#ifdef LARGE_STACK
	Entry entries[0x100*0x100]; //Entry entries[0x100][0x100]; // 384/640 KB
	uint16_t sizes[0x100*0x100]; //uint16_t sizes[0x100][0x100]; // 128 KB
#else
	Entry *entries;
	uint16_t *sizes;
#endif

public:
	inline LZNT1Dictionary()
	{
#ifndef LARGE_STACK
		this->entries = (Entry*)malloc(0x100*0x100*sizeof(Entry));
		this->sizes = (uint16_t*)malloc(0x100*0x100*sizeof(uint16_t));
#endif
		// need to set pos to NULL and cap to 0
		memset(this->entries, 0, 0x100*0x100*sizeof(Entry));
	}

	inline ~LZNT1Dictionary()
	{
		uint32_t idx;
		for (idx = 0; idx < 0x100*0x100; ++idx)
			free(this->entries[idx].pos);
#ifndef LARGE_STACK
		free(this->entries);
		free(this->sizes);
#endif
	}

	// Resets a dictionary, ready to start a new chunk
	// This should also be called before any Add/Find
	inline void Reset()
	{
		memset(this->sizes, 0, 0x100*0x100*sizeof(uint16_t));
	}
	
	// Adds data to the dictionary, which will be used as a starting point during future finds
	// Max length is how many bytes can be read from data, regardless of the end of the chunk
	inline void Add(const_bytes data, const size_t max_len)
	{
		if (max_len >= 2)
		{
			const uint_fast16_t idx = data[0] << 8 | data[1];
			this->entries[idx].add(data, this->sizes[idx]);
		}
	}
	inline void Add(const_bytes data, size_t len, const size_t max_len)
	{
		if (len > max_len - 2)
			len = max_len - 2;
		byte x, y = data[0];
		for (const_bytes end = data + len; data < end; ++data)
		{
			x = y; y = data[1];
			this->entries[x << 8 | y].add(data, this->sizes[x << 8 | y]);
		}
	}
	
WARNINGS_PUSH()
WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()

	// Finds the best symbol in the dictionary(ies) for the data
	// The second dictionary may be NULL for independent chunks, or the dictionary for the previous chunk is overlap can occur
	// Returns the length of the string found, or 0 if nothing of length >= 3 was found
	// offset is set to the offset from the current position to the string
	inline uint_fast16_t Find(const_bytes data, const uint_fast16_t max_len, const_bytes search, uint_fast16_t* offset) const
	{
		if (max_len >= 3 && data-search > 0)
		{
			const byte x = data[0], y = data[1];
			const uint_fast16_t idx = data[0] << 8 | data[1];
			const Entry* e = &this->entries[idx];
			const uint16_t size = this->sizes[idx];
			if (size) // a match is possible
			{
				const byte z = data[2];
				uint_fast16_t l = 0, o;
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
							--ep;

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
						uint_fast16_t i;
						for (i = 3; i < max_len && data[i] == *s; i++, s++);
						if (i > l) { o = (uint_fast16_t)(data-ss); l = i; if (l == max_len) { break; } }
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
