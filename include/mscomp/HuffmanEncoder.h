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
#include "Bitstream.h"

template <byte NumBitsMax, uint16_t NumSymbols>
class HuffmanEncoder
{
private:
	uint16_t codes[NumSymbols];
	byte lens[NumSymbols];

#define HEAP_PUSH(x)                         \
{                                            \
	heap[++heap_len] = x;                    \
	uint_fast16_t j = heap_len;              \
	while (weights[x] < weights[heap[j>>1]]) \
	{                                        \
		heap[j] = heap[j>>1]; j >>= 1;       \
	}                                        \
	heap[j] = x;                             \
}

#define HEAP_POP()                                  \
{                                                   \
	uint_fast16_t i = 1, t = heap[1] = heap[heap_len--]; \
	for (;;)                                        \
	{                                               \
		uint_fast16_t j = i << 1;                   \
		if (j > heap_len) { break; }                \
		if (j < heap_len && weights[heap[j+1]] < weights[heap[j]]) { ++j; } \
		if (weights[t] < weights[heap[j]]) { break; } \
		heap[i] = heap[j];                          \
		i = j;                                      \
	}                                               \
	heap[i] = t;                                    \
}

public:
	INLINE const const_bytes CreateCodes(uint32_t symbol_counts[NumSymbols]) // 17 kb stack (for NumSymbols == 0x200)
	{
		memset(this->codes, 0, sizeof(this->codes));

		// Compute the initial weights (the weight is in the upper 24 bits, the depth (initially 0) is in the lower 8 bits
		uint32_t weights[NumSymbols * 2]; // weights of nodes
		weights[0] = 0;
		for (uint_fast16_t i = 0; i < NumSymbols; i++) { weights[i+1] = (symbol_counts[i] == 0 ? 1 : symbol_counts[i]) << 8; }

		for (;;)
		{
			// Build the initial heap
			uint_fast16_t heap_len = 0, heap[NumSymbols + 2] = { 0 }; // heap of symbols, 1 to heap_len
			for (uint_fast16_t i = 1; i <= NumSymbols; ++i) { HEAP_PUSH(i); }

			// Build the tree (its a bottom-up tree)
			uint_fast16_t n_nodes = NumSymbols, parents[NumSymbols * 2]; // parents of nodes, 1 to n_nodes
			memset(parents, 0, sizeof(parents));
			while (heap_len > 1)
			{
				uint_fast16_t n1 = heap[1]; HEAP_POP();
				uint_fast16_t n2 = heap[1]; HEAP_POP();
				parents[n1] = parents[n2] = ++n_nodes;
				weights[n_nodes] = ((weights[n1]&0xffffff00)+(weights[n2]&0xffffff00)) | (1 + MAX((weights[n1]&0x000000ff),(weights[n2]&0x000000ff)));
				HEAP_PUSH(n_nodes);
			}

			// Create the actual length codes
			bool too_long = false;
			for (uint_fast16_t i = 1; i <= NumSymbols; i++)
			{
				byte j = 0;
				uint_fast16_t k = i;
				while (parents[k] > 0) { k = parents[k]; ++j; }
				this->lens[i-1] = j;
				if (j > NumBitsMax) { too_long = true; }
			}

			// If we had codes that were too long then we need to make all the weights smaller
			if (!too_long) { break; }
			for (uint_fast16_t i = 1; i <= NumSymbols; ++i)
			{
				weights[i] = (1 + (weights[i] >> 9)) << 8;
			}
		}

		// Compute the values of the codes
		uint_fast16_t min = this->lens[0], max = min;
		for (uint_fast16_t i = 1; i < NumSymbols; ++i)
		{
			if (this->lens[i] > max) { max = this->lens[i]; }
			else if (this->lens[i] < min) { min = this->lens[i]; }
		}
		uint16_t code = 0;
		for (uint_fast16_t n = min; n <= max; ++n)
		{
			for (uint_fast16_t i = 0; i < NumSymbols; ++i)
			{
				if (this->lens[i] == n) { this->codes[i] = code++; }
			}
			code <<= 1;
		}

		// Done!
		return this->lens;
	}

	INLINE bool EncodeSymbol(uint_fast16_t sym, OutputBitstream *bits) const { return bits->WriteBits(this->codes[sym], this->lens[sym]); }
};

#undef HEAP_PUSH
#undef HEAP_POP

#endif
