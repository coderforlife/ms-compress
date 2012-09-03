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

#ifndef HUFFMAN_DECODER
#define HUFFMAN_DECODER

#include "compression-api.h"
#include "Bitstream.h"

#define INVALID_SYMBOL 0xFFFF

template <byte kNumBitsMax, uint16_t NumSymbols>
class HuffmanDecoder
{
	CASSERT(kNumBitsMax <= 16);

private:
	static const int kNumTableBits = 9;

	uint32_t limits   [kNumBitsMax + 1];     // m_Limits[i] = value limit for symbols with length = i
	uint32_t positions[kNumBitsMax + 1];     // m_Positions[i] = index in m_Symbols[] of first symbol with length = i
	uint16_t symbols  [NumSymbols];
	byte     lengths  [1 << kNumTableBits]; // Table oh length for short codes.

public:
	inline bool SetCodeLengths(const const_bytes codeLengths)
	{
		memset(symbols, INVALID_SYMBOL, sizeof(symbols));

		// Get all length counts
		uint_fast16_t lenCounts[kNumBitsMax + 1];
		memset(lenCounts+1, 0, kNumBitsMax*sizeof(uint_fast16_t));
		for (uint_fast16_t symbol = 0; symbol < NumSymbols; ++symbol)
		{
			byte len = codeLengths[symbol];
			if (len > kNumBitsMax) { return false; }
			++lenCounts[len];
		}
		lenCounts[0] = 0;

		// Get positions and limits
		uint32_t tmpPositions[kNumBitsMax + 1];
		this->positions[0] = this->limits[0] = 0;
		uint32_t startPos = 0, index = 0;
		const uint32_t kMaxValue = (1 << kNumBitsMax);
		for (byte i = 1; i <= kNumBitsMax; i++)
		{
			startPos += lenCounts[i] << (kNumBitsMax - i);
			if (startPos > kMaxValue) { return false; }
			this->limits[i] = (i == kNumBitsMax) ? kMaxValue : startPos;
			this->positions[i] = this->positions[i - 1] + lenCounts[i - 1];
			tmpPositions[i] = this->positions[i];
			if (i <= kNumTableBits)
			{
				uint32_t limit = this->limits[i] >> (kNumBitsMax - kNumTableBits);
				for (; index < limit; index++)
					this->lengths[index] = i;
			}
		}

		// Get symbols
		for (uint_fast16_t symbol = 0; symbol < NumSymbols; symbol++)
		{
			int len = codeLengths[symbol];
			if (len != 0)
				this->symbols[tmpPositions[len]++] = (uint16_t)symbol;
		}

		return true;
	}

	inline uint_fast16_t DecodeSymbol(InputBitstream *bits) const
	{
		byte numBits, remBits = bits->RemainingBits();
		uint32_t value = (kNumBitsMax > remBits) ? (bits->Peek(remBits) << (kNumBitsMax - remBits)) : bits->Peek(kNumBitsMax);
		if (value < this->limits[kNumTableBits])
			numBits = this->lengths[value >> (kNumBitsMax - kNumTableBits)];
		else
			for (numBits = kNumTableBits + 1; value >= this->limits[numBits]; numBits++);
		bits->Skip(numBits);
		uint32_t index = this->positions[numBits] + ((value - this->limits[numBits - 1]) >> (kNumBitsMax - numBits));
		return (index >= NumSymbols) ? INVALID_SYMBOL : this->symbols[index];
	}
};

// The Huffman Decoder I originally wrote
// Does not use kNumBitsMax and does not perform data checks during SetCodeLengths (always returns true)
// It requires msort() from HuffmanEncoder to work.
//
// The 7-zip one is about twice as fast.
//
//template <int kNumBitsMax, uint32_t NumSymbols>
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
//	inline void add_leaf(uint_fast16_t& npos, uint32_t code, byte len, uint16_t sym)
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
//	inline bool SetCodeLengths(const bytes codeLengths) // 2 kb stack
//	{
//		uint_fast16_t len = 0;
//		uint16_t syms[NumSymbols], temp[NumSymbols]; // 2*2*512 = 2 kb
//		for (uint_fast16_t i = 0; i < NumSymbols; ++i) { if (codeLengths[i] > 0) { syms[len++] = (uint16_t)i; } }
//
//		memset(n, 0, (2*NumSymbols-1)*sizeof(Node));
//		msort(syms, temp, codeLengths, 0, len);
//
//		byte nbits = 1;
//		uint_fast16_t npos = 1;
//		uint32_t code = 0;
//		this->n[0].symbol = INVALID_SYMBOL;
//		for (uint_fast16_t i = 0; i < len; ++i)
//		{
//			code <<= (codeLengths[syms[i]] - nbits);
//			add_leaf(npos, code++, nbits = codeLengths[syms[i]], syms[i]);
//		}
//
//		return true;
//	}
//
//	inline uint_fast16_t DecodeSymbol(InputBitstream *bits)
//	{
//		Node *n = this->n;
//		do {
//			byte bit = bits->ReadBit();
//			if (bit > 1) { return INVALID_SYMBOL; }
//			n = n->child[bit];
//		} while (n->symbol == INVALID_SYMBOL);
//		return n->symbol;
//	}
//};

#endif
