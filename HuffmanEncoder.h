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

#ifndef HUFFMAN_ENCODER
#define HUFFMAN_ENCODER

#include "compression-api.h"
#include "Bitstream.h"

#define INVALID_SYMBOL 0xFFFF

template <byte kNumBitsMax, uint16_t NumSymbols>
class HuffmanEncoder
{
private:
	uint16_t codes[NumSymbols];
	byte lens[NumSymbols];

	// Merge-sorts syms[l, r) using conditions[syms[x]]
	// Use merge-sort so that it is stable, keeping symbols in increasing order
	template<typename T> // T is either uint32_t or byte
	static void msort(uint16_t* syms, uint16_t* temp, T* conditions, uint_fast16_t l, uint_fast16_t r)
	{
		uint_fast16_t len = r - l;
		if (len <= 1) { return; }
	
		// Not required to do these special in-place sorts, but is a bit more efficient
		else if (len == 2)
		{
			if (conditions[syms[l+1]] < conditions[syms[ l ]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; }
			return;
		}
		else if (len == 3)
		{
			if (conditions[syms[l+1]] < conditions[syms[ l ]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; }
			if (conditions[syms[l+2]] < conditions[syms[l+1]]) { uint16_t t = syms[l+2]; syms[l+2] = syms[l+1]; syms[l+1] = t;
				if (conditions[syms[l+1]]<conditions[syms[l]]) { uint16_t t = syms[l+1]; syms[l+1] = syms[ l ]; syms[ l ] = t; } }
			return;
		}
	
		// Merge-Sort
		else
		{
			uint_fast16_t m = l + (len >> 1), i = l, j = l, k = m;
		
			// Divide and Conquer
			msort(syms, temp, conditions, l, m);
			msort(syms, temp, conditions, m, r);
			memcpy(temp+l, syms+l, len*sizeof(uint16_t));
		
			// Merge
			while (j < m && k < r) syms[i++] = (conditions[temp[k]] < conditions[temp[j]]) ? temp[k++] : temp[j++]; // if == then does j which is from the lower half, keeping stable
				 if (j < m) memcpy(syms+i, temp+j, (m-j)*sizeof(uint16_t));
			else if (k < r) memcpy(syms+i, temp+k, (r-k)*sizeof(uint16_t));
		}
	}

public:
	inline const const_bytes CreateCodes(uint32_t symbol_counts[]) // 3 kb stack
	{
		uint16_t* syms, syms_by_count[NumSymbols], syms_by_len[NumSymbols], temp[NumSymbols]; // 3*2*512 = 3 kb
		uint_fast16_t i, j, len, pos, s;

		memset(this->codes, 0, NumSymbols*sizeof(uint16_t));
		memset(this->lens,  0, NumSymbols*sizeof(byte));

		// Fill the syms_by_count, syms_by_length, and huffman_lens with the symbols that were found
		for (i = 0, len = 0; i < NumSymbols; ++i) { if (symbol_counts[i]) { syms_by_count[len] = (uint16_t)i; syms_by_len[len++] = (uint16_t)i; this->lens[i] = kNumBitsMax; } }


		////////// Get the Huffman lengths //////////
		msort(syms = syms_by_count, temp, symbol_counts, 0, len); // sort by the counts
		if (len == 1)
		{
			this->lens[syms[0]] = 1; // never going to happen, but the code below would probably assign a length of 0 which is not right
		}
		else
		{
			///// Package-Merge Algorithm /////
			typedef struct _collection // 516 bytes each
			{
				uint_fast16_t count;
				byte symbols[NumSymbols];
			} collection;
			collection* cols = (collection*)malloc(32*sizeof(collection)), *next_cols = (collection*)malloc(32*sizeof(collection)), *temp_cols; // 32.25 kb initial allocation
			uint_fast16_t cols_cap = 32, cols_len = 0, cols_pos, next_cols_len = 0;
		
			if (!cols || !next_cols) { PRINT_ERROR("Xpress Huffman Compression Error: malloc failed\n"); free(cols); free(next_cols); return NULL; }

			// Start at the lowest value row, adding new collection
			for (j = 0; j < kNumBitsMax; ++j)
			{
				cols_pos = 0;
				pos = 0;

				// All but the last one/none get added to collections
				while ((cols_len-cols_pos + len-pos) > 1)
				{
					if (cols_cap == next_cols_len)
					{
						cols_cap <<= 1;

						temp_cols = (collection*)realloc(cols,      cols_cap*sizeof(collection));
						if (temp_cols == NULL) { PRINT_ERROR("Xpress Huffman Compression Error: realloc failed\n"); free(cols); free(next_cols); return NULL; }
						cols      = temp_cols;

						temp_cols = (collection*)realloc(next_cols, cols_cap*sizeof(collection));
						if (temp_cols == NULL) { PRINT_ERROR("Xpress Huffman Compression Error: realloc failed\n"); free(cols); free(next_cols); return NULL; }
						next_cols = temp_cols;
					}
					memset(next_cols+next_cols_len, 0, sizeof(collection));
					for (i = 0; i < 2; ++i) // hopefully unrolled...
					{
						if (pos >= len || (cols_pos < cols_len && cols[cols_pos].count < symbol_counts[syms[pos]]))
						{
							// Add cols[cols_pos]
							next_cols[next_cols_len].count += cols[cols_pos].count;
							for (s = 0; s < NumSymbols; ++s)
								next_cols[next_cols_len].symbols[s] += cols[cols_pos].symbols[s];
							++cols_pos;
						}
						else
						{
							// Add syms[pos]
							next_cols[next_cols_len].count += symbol_counts[syms[pos]];
							++next_cols[next_cols_len].symbols[syms[pos]];
							++pos;
						}
					}
					++next_cols_len;
				}
			
				// Left over gets dropped
				if (cols_pos < cols_len)
					for (s = 0; s < NumSymbols; ++s)
						this->lens[s] -= cols[cols_pos].symbols[s];
				else if (pos < len)
					--this->lens[syms[pos]];

				// Move the next_collections to the current collections
				temp_cols = cols; cols = next_cols; next_cols = temp_cols;
				cols_len = next_cols_len;
				next_cols_len = 0;
			}
			free(cols);
			free(next_cols);


			////////// Create Huffman codes from lengths //////////
			msort(syms = syms_by_len, temp, this->lens, 0, len); // Sort by the code lengths
			for (i = 1; i < len; ++i)
			{
				// Code is previous code +1 with added zeroes for increased code length
				this->codes[syms[i]] = (this->codes[syms[i-1]] + 1) << (this->lens[syms[i]] - this->lens[syms[i-1]]);
			}
		}


		return this->lens;
	}

	inline bool EncodeSymbol(uint_fast16_t sym, OutputBitstream *bits) const { return bits->WriteBits(this->codes[sym], this->lens[sym]); }
};

#endif
