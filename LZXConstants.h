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

#ifndef LZX_CONSTANTS_H
#define LZX_CONSTANTS_H

#include "compression-api.h"

const unsigned kNumHuffmanBits = 16;
const uint32_t kNumRepDistances = 3;

const uint32_t kNumLenSlots = 8;
const uint32_t kMatchMinLen = 2;
const uint32_t kNumLenSymbols = 249;
const uint32_t kMatchMaxLen = kMatchMinLen + (kNumLenSlots - 1) + kNumLenSymbols - 1;

const unsigned kNumAlignBits = 3;
const uint32_t kAlignTableSize = 1 << kNumAlignBits;

const uint32_t kNumPosSlots = 50;
const uint32_t kNumPosLenSlots = kNumPosSlots * kNumLenSlots;

const uint32_t kMainTableSize = 256 + kNumPosLenSlots;
const uint32_t kLevelTableSize = 20;
const uint32_t kMaxTableSize = kMainTableSize;

const unsigned kNumBlockTypeBits = 3;
const unsigned kBlockTypeVerbatim = 1;
const unsigned kBlockTypeAligned = 2;
const unsigned kBlockTypeUncompressed = 3;

const unsigned kUncompressedBlockSizeNumBits = 24;

const unsigned kNumBitsForPreTreeLevel = 4;

const unsigned kLevelSymbolZeros = 17;
const unsigned kLevelSymbolZerosBig = 18;
const unsigned kLevelSymbolSame = 19;

const unsigned kLevelSymbolZerosStartValue = 4;
const unsigned kLevelSymbolZerosNumBits = 4;

const unsigned kLevelSymbolZerosBigStartValue = kLevelSymbolZerosStartValue + (1 << kLevelSymbolZerosNumBits);
const unsigned kLevelSymbolZerosBigNumBits = 5;

const unsigned kLevelSymbolSameNumBits = 1;
const unsigned kLevelSymbolSameStartValue = 4;

const unsigned kNumBitsForAlignLevel = 3;
  
const unsigned kNumDictionaryBitsMin = 15;
const unsigned kNumDictionaryBitsMax = 21;
const uint32_t kDictionarySizeMax = (1 << kNumDictionaryBitsMax);

const unsigned kNumLinearPosSlotBits = 17;
const uint32_t kNumPowerPosSlots = 0x26;

const byte kMinTranslationLength = 10; // not allowed in last 6 and is 5 long
const int kWIMTranslationSize = 12000000;

static inline uint32_t GetNumPosSlots(unsigned int numDictBits)
{
	if (numDictBits < kNumDictionaryBitsMin || numDictBits > kNumDictionaryBitsMax)	{ return 0; }
	if (numDictBits < 20) { return 30 + (numDictBits - 15) * 2; }
	else                  { return (numDictBits == 20) ? 42: 50; }
}

#endif
