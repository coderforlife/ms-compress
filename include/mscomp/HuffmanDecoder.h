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

// Adapted from 7-zip. See http://www.7-zip.org/.

#ifndef MSCOMP_HUFFMAN_DECODER
#define MSCOMP_HUFFMAN_DECODER

#include "internal.h"
#include "Bitstream.h"

#define INVALID_SYMBOL 0xFFFF

template <byte NumBitsMax, uint16_t NumSymbols> // for NumBitsMax = 15 and NumSymbols = 0x200 this takes 1.625 kb (+~0.15 during SetCodeLengths)
class HuffmanDecoder
{
	CASSERT(NumBitsMax <= 16 && NumBitsMax > 2);

private:
	static const int NumTableBits = (NumBitsMax+1)/2 + 1; // = 9 for a NumBitsMax of 15 and 16
	// TODO: increasing the value gives speedups but requires more space, eventually great increases in memory provide little increase in speed

	uint_fast16_t lims[NumBitsMax + 1]; // lims[i] = value limit for syms with length = i
	uint_fast16_t poss[NumBitsMax + 1]; // poss[i] = index in syms[] of first symbol with length = i
	uint16_t syms[NumSymbols];
	byte     lens[1 << NumTableBits]; // table for short codes

public:
	INLINE bool SetCodeLengths(const const_byte code_lengths[NumSymbols])
	{
		memset(this->syms, INVALID_SYMBOL, sizeof(this->syms));

		// Get all length counts
		uint_fast16_t cnts[NumBitsMax + 1];
		memset(cnts+1, 0, NumBitsMax*sizeof(uint_fast16_t));
		for (uint_fast16_t s = 0; s < NumSymbols; ++s)
		{
			const byte len = code_lengths[s];
			//if (UNLIKELY(len > NumBitsMax)) { return false; } // TODO: Not needed for XPRESS Huffman, need to check if LZX needs it
			ALWAYS(len <= NumBitsMax);
			++cnts[len];
		}
		cnts[0] = 0;

		// Get limits and lengths
		const uint_fast16_t MaxValue = (1 << NumBitsMax);
		uint_fast16_t last = 0, index = 0;
		this->lims[0] = 0; this->lims[NumBitsMax] = MaxValue;
		for (uint_fast8_t len = 1; len <= NumTableBits; ++len)
		{
			this->lims[len] = (last += (cnts[len] << (NumBitsMax - len)));
			const uint_fast16_t limit = this->lims[len] >> (NumBitsMax - NumTableBits);
			memset(this->lens+index, len, limit-index); index = limit;
		}
		for (uint_fast8_t len = NumTableBits + 1; len < NumBitsMax; ++len)
		{
			this->lims[len] = (last += (cnts[len] << (NumBitsMax - len)));
		}
		if (UNLIKELY(last + cnts[NumBitsMax] > MaxValue)) { return false; }

		// Get positions
		this->poss[0] = 0;
		for (uint_fast8_t len = 1; len <= NumBitsMax; ++len) { this->poss[len] = this->poss[len-1] + cnts[len-1]; }

		// Get symbols
		memcpy(cnts, this->poss, sizeof(uint_fast16_t)*(NumBitsMax+1));
		for (uint16_t s = 0; s < NumSymbols; ++s)
		{
			int len = code_lengths[s];
			if (len) { this->syms[cnts[len]++] = s; }
		}

		return true;
	}

	INLINE uint_fast16_t DecodeSymbol(InputBitstream *bits) const
	{
		uint_fast8_t n, r = bits->AvailableBits();
		const uint32_t x = UNLIKELY(r < NumBitsMax) ? (bits->Peek(r) << (NumBitsMax - r)) : bits->Peek(NumBitsMax);
		if (LIKELY(x < this->lims[NumTableBits])) { n = this->lens[x >> (NumBitsMax - NumTableBits)]; }
		else { for (n = NumTableBits + 1; x >= this->lims[n]; ++n); }
		bits->Skip(n);
		uint32_t s = this->poss[n] + ((x - this->lims[n-1]) >> (NumBitsMax-n));
		return UNLIKELY(s >= NumSymbols) ? INVALID_SYMBOL : this->syms[s]; // TODO: can this ever happen? does removing it make things faster?
	}
	
	INLINE uint_fast16_t DecodeSymbolFast(InputBitstream *bits) const
	{
		uint_fast8_t n;
		const uint32_t x = bits->Peek(NumBitsMax);
		if (LIKELY(x < this->lims[NumTableBits])) { n = this->lens[x >> (NumBitsMax - NumTableBits)]; }
		else { for (n = NumTableBits + 1; x >= this->lims[n]; ++n); }
		bits->Skip_Fast(n);
		uint32_t s = this->poss[n] + ((x - this->lims[n-1]) >> (NumBitsMax-n));
		return UNLIKELY(s >= NumSymbols) ? INVALID_SYMBOL : this->syms[s]; // TODO: can this ever happen? does removing it make things faster?
	}
};

//// The Huffman Decoder I originally wrote
//// Does not use NumBitsMax and does not perform data checks during SetCodeLengths (always returns true)
//// The 7-zip one is about twice as fast.
//
//#include "sorting.h"
//template <int NumBitsMax, uint32_t NumSymbols>
//class HuffmanDecoder
//{
//	// the maximum number of nodes in the Huffman code tree [for Xpress Huffman] is 2*SYMBOLS-1 = 1023, overall this is ~10kb or ~20kb (64-bit)
//
//private:
//	struct Node
//	{
//		uint16_t symbol; // The symbol for the leaf or INVALID_SYMBOL for internal nodes
//		Node* child[2];  // The node’s two children, or NULL if those children don't exist
//	};
//	Node n[2*NumSymbols-1];
//
//	INLINE void add_leaf(uint_fast16_t& npos, uint32_t code, byte len, uint16_t sym)
//	{
//		Node *node = this->n;
//		while (--len)
//		{
//			uint32_t i = (code >> len) & 1;
//			if (node->child[i] == NULL)
//				(node->child[i] = &this->n[npos++])->symbol = INVALID_SYMBOL;
//			node = node->child[i];
//		}
//		(node->child[code & 1] = &this->n[npos++])->symbol = sym;
//	}
//
//public:
//	INLINE bool SetCodeLengths(const bytes code_lengths) // 2 kb stack
//	{
//		uint_fast16_t len = 0;
//		uint16_t syms[NumSymbols], temp[NumSymbols]; // 2*2*512 = 2 kb
//		for (uint_fast16_t i = 0; i < NumSymbols; ++i) { if (code_lengths[i] > 0) { syms[len++] = (uint16_t)i; } }
//
//		memset(n, 0, (2*NumSymbols-1)*sizeof(Node));
//		merge_sort(syms, temp, code_lengths, len);
//
//		byte nbits = 1;
//		uint_fast16_t npos = 1;
//		uint32_t code = 0;
//		this->n[0].symbol = INVALID_SYMBOL;
//		for (uint_fast16_t i = 0; i < len; ++i)
//		{
//			code <<= (code_lengths[syms[i]] - nbits);
//			add_leaf(npos, code++, nbits = code_lengths[syms[i]], syms[i]);
//		}
//
//		return true;
//	}
//
//	INLINE uint_fast16_t DecodeSymbol(InputBitstream *bits)
//	{
//		Node *n = this->n;
//		do {
//			n = n->child[bits->ReadBit()];
//		} while (n->symbol == INVALID_SYMBOL);
//		return n->symbol;
//	}
//
//	INLINE uint_fast16_t DecodeSymbolFast(InputBitstream *bits)
//	{
//		Node *n = this->n;
//		do {
//			n = n->child[bits->ReadBit_Fast()];
//		} while (n->symbol == INVALID_SYMBOL);
//		return n->symbol;
//	}
//};

#endif
