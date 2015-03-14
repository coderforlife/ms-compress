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

#ifndef MSCOMP_HUFFMAN_ENCODER
#define MSCOMP_HUFFMAN_ENCODER

#include "internal.h"
#include "sorting.h"
#include "Bitstream.h"

#define INVALID_SYMBOL 0xFFFF

template <byte NumBitsMax, uint16_t NumSymbols>
class HuffmanEncoder
{
private:
	uint16_t codes[NumSymbols];
	byte lens[NumSymbols];

public:
	INLINE const const_bytes CreateCodes(uint32_t symbol_counts[NumSymbols]) // 519 kb stack
	{
		memset(this->codes, 0, sizeof(this->codes));
		memset(this->lens,  0, sizeof(this->lens));

		// Fill the syms_by_count, syms_by_length, and huffman_lens with the symbols that were found
		uint16_t syms_by_count[NumSymbols], syms_by_len[NumSymbols], temp[NumSymbols]; // 3*2*512 = 3 kb
		uint_fast16_t len = 0;
		for (uint_fast16_t i = 0; i < NumSymbols; ++i) { if (symbol_counts[i]) { syms_by_count[len] = (uint16_t)i; syms_by_len[len++] = (uint16_t)i; this->lens[i] = NumBitsMax; } }

		////////// Get the Huffman lengths //////////
		merge_sort(syms_by_count, temp, symbol_counts, len); // sort by the counts
		if (UNLIKELY(len == 1))
		{
			this->lens[syms_by_count[0]] = 1; // never going to happen, but the code below would probably assign a length of 0 which is not right
		}
		else
		{
			///// Package-Merge Algorithm /////
			typedef struct _collection // 516 bytes each
			{
				byte symbols[NumSymbols];
				uint_fast16_t count;
			} collection;
			collection _cols[512], *cols = _cols, _next_cols[512], *next_cols = _next_cols; // 2*516*512 = 516 kb
			uint_fast16_t cols_len = 0, next_cols_len = 0;

			// Start at the lowest value row, adding new collection
			for (uint_fast16_t j = 0; j < NumBitsMax; ++j)
			{
				uint_fast16_t cols_pos = 0, pos = 0;

				// All but the last one/none get added to collections
				while ((cols_len-cols_pos + len-pos) > 1)
				{
					memset(next_cols+next_cols_len, 0, sizeof(collection));
					for (uint_fast16_t i = 0; i < 2; ++i) // hopefully unrolled...
					{
						if (pos >= len || (cols_pos < cols_len && cols[cols_pos].count < symbol_counts[syms_by_count[pos]]))
						{
							// Add cols[cols_pos]
							next_cols[next_cols_len].count += cols[cols_pos].count;
							for (uint_fast16_t s = 0; s < NumSymbols; ++s)
							{
								next_cols[next_cols_len].symbols[s] += cols[cols_pos].symbols[s];
							}
							++cols_pos;
						}
						else
						{
							// Add syms[pos]
							next_cols[next_cols_len].count += symbol_counts[syms_by_count[pos]];
							++next_cols[next_cols_len].symbols[syms_by_count[pos]];
							++pos;
						}
					}
					++next_cols_len;
				}
			
				// Left over gets dropped
				if (cols_pos < cols_len)
				{
					const byte* const syms = cols[cols_pos].symbols;
					for (uint_fast16_t i = 0; i < NumSymbols; ++i) { this->lens[i] -= syms[i]; }
				}
				else if (pos < len)
				{
					--this->lens[syms_by_count[pos]];
				}

				// Move the next_collections to the current collections
				collection* temp = cols; cols = next_cols; next_cols = temp;
				cols_len = next_cols_len;
				next_cols_len = 0;
			}


			////////// Create Huffman codes from lengths //////////
			merge_sort(syms_by_len, temp, this->lens, len); // Sort by the code lengths
			for (uint_fast16_t i = 1; i < len; ++i)
			{
				// Code is previous code +1 with added zeroes for increased code length
				this->codes[syms_by_len[i]] = (this->codes[syms_by_len[i-1]] + 1) << (this->lens[syms_by_len[i]] - this->lens[syms_by_len[i-1]]);
			}
		}


		return this->lens;
	}

	INLINE bool EncodeSymbol(uint_fast16_t sym, OutputBitstream *bits) const { return bits->WriteBits(this->codes[sym], this->lens[sym]); }
};

#endif
