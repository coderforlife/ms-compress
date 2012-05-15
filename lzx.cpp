#include "stdafx.h"
#include "lzx.h"

#include "Bitstream.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#pragma optimize("t", on)

#define VERBATIM_BLOCK			1
#define ALIGNED_OFFSET_BLOCK	2
#define UNCOMPRESSED_BLOCK		3

#define NUM_HUFFMAN_BITS		16

#define TRANSLATION_SIZE		12000000

#define kNumTableBits			9
#define kNumRepDistances		3
#define kNumAlignBits			3
#define kNumBitsForPreTreeLevel	4

#define kNumLenSlots			8
#define kMatchMinLen			2
#define kNumLenSymbols			249
#define kNumPosSlots			30
#define kNumPosLenSlots			kNumPosSlots * kNumLenSlots

#define kAlignTableSize			1 << kNumAlignBits
#define kMainTableSize			256 + kNumPosLenSlots
#define kLevelTableSize			20
#define kMaxTableSize			kMainTableSize

#define kLevelSymbolZeros			17
#define kLevelSymbolZerosStartValue	4
#define kLevelSymbolZerosNumBits	4

#define kLevelSymbolZerosBig			18
#define kLevelSymbolZerosBigStartValue	kLevelSymbolZerosStartValue + (1 << kLevelSymbolZerosNumBits)
#define kLevelSymbolZerosBigNumBits		5

#define kLevelSymbolSame			19
#define kLevelSymbolSameStartValue	4
#define kLevelSymbolSameNumBits		1

#define kNumLinearPosSlotBits		17
#define kNumPowerPosSlots			0x26

#ifdef __cplusplus

template <uint32_t NumSymbols>
class Decoder {
	uint32_t limits[NUM_HUFFMAN_BITS + 1];		// m_Limits[i] = value limit for symbols with length = i
	uint32_t positions[NUM_HUFFMAN_BITS + 1];	// m_Positions[i] = index in m_Symbols[] of first symbol with length = i
	uint32_t symbols[NumSymbols];
	byte lengths[1 << kNumTableBits];			// Table oh length for short codes.
public:
	bool SetCodeLengths(const byte *codeLengths) {
		int lenCounts[NUM_HUFFMAN_BITS + 1];
		uint32_t tmpPositions[NUM_HUFFMAN_BITS + 1];
		memset(lenCounts+1, 0, NUM_HUFFMAN_BITS*sizeof(int));
		uint32_t symbol;
		for (symbol = 0; symbol < NumSymbols; symbol++) {
			int len = codeLengths[symbol];
			if (len > NUM_HUFFMAN_BITS)
				return false;
			lenCounts[len]++;
			this->symbols[symbol] = 0xFFFFFFFF;
		}
		lenCounts[0] = 0;
		this->positions[0] = this->limits[0] = 0;
		uint32_t startPos = 0;
		uint32_t index = 0;
		const uint32_t kMaxValue = (1 << NUM_HUFFMAN_BITS);
		int i;
		for (i = 1; i <= NUM_HUFFMAN_BITS; i++) {
			startPos += lenCounts[i] << (NUM_HUFFMAN_BITS - i);
			if (startPos > kMaxValue)
				return false;
			this->limits[i] = (i == NUM_HUFFMAN_BITS) ? kMaxValue : startPos;
			this->positions[i] = this->positions[i - 1] + lenCounts[i - 1];
			tmpPositions[i] = this->positions[i];
			if(i <= kNumTableBits) {
				uint32_t limit = (this->limits[i] >> (NUM_HUFFMAN_BITS - kNumTableBits));
				for (; index < limit; index++)
					this->lengths[index] = (byte)i;
			}
		}
		for (symbol = 0; symbol < NumSymbols; symbol++) {
			int len = codeLengths[symbol];
			if (len != 0)
				this->symbols[tmpPositions[len]++] = symbol;
		}
		return true;
	}
	uint32_t DecodeSymbol(InputBitstream* bits) {
		int numBits;
		uint32_t value = BSPeek(bits, NUM_HUFFMAN_BITS);
		if (value < this->limits[kNumTableBits])
			numBits = this->lengths[value >> (NUM_HUFFMAN_BITS - kNumTableBits)];
		else
			for (numBits = kNumTableBits + 1; value >= this->limits[numBits]; numBits++);
		BSSkip(bits, numBits);
		uint32_t index = this->positions[numBits] + ((value - this->limits[numBits - 1]) >> (NUM_HUFFMAN_BITS - numBits));
		if (index >= NumSymbols) // throw CDecoderException(); // test it
			return 0xFFFFFFFF;
		return this->symbols[index];
	}
};

class LZX {
public:
	InputBitstream Bits;

	Decoder<kMainTableSize> MainDecoder;
	Decoder<kNumLenSymbols> LenDecoder;
	Decoder<kAlignTableSize> AlignDecoder;
	Decoder<kLevelTableSize> LevelDecoder;

	uint32_t RepDistances[kNumRepDistances];

	int Type;
	uint32_t UncompressedSize;
	bool IsAlignType;

	public: LZX(const byte *in, size_t len)
	{
		BSReadInit(&this->Bits, in, len);
		this->Type = BSReadBits(&this->Bits, 3);
		this->UncompressedSize = BSReadBit(&this->Bits) ? 0x8000 : BSReadBits(&this->Bits, 16);
		this->IsAlignType = (this->Type == ALIGNED_OFFSET_BLOCK);
		memset(this->RepDistances, 0, sizeof(this->RepDistances));
	}
	public: ~LZX() { /*delete this->Bits;*/ }

	public: size_t DecompressTo(byte *out) {
		if (this->Type == UNCOMPRESSED_BLOCK) {
			Translate(CopyUncompressedBlock(out));
			return this->UncompressedSize;
		} else if (this->Type != VERBATIM_BLOCK && this->Type != ALIGNED_OFFSET_BLOCK) {
			return 0;
		}
	
		byte newLevels[kMaxTableSize];

		if (this->IsAlignType) {
			for (unsigned i = 0; i < kAlignTableSize; i++)
				newLevels[i] = (byte)BSReadBits(&this->Bits, kNumAlignBits);
			if (!this->AlignDecoder.SetCodeLengths(newLevels)) {
				// error!
				return 0;
			}
		}

		ReadTable(newLevels, 256);
		ReadTable(newLevels + 256, kNumPosLenSlots);
		if (!this->MainDecoder.SetCodeLengths(newLevels)) {
			// error!
			return 0;
		}
	
		ReadTable(newLevels, kNumLenSymbols);
		if (!this->LenDecoder.SetCodeLengths(newLevels)) {
			// error!
			return 0;
		}

		if (!DecompressIt(out)) return 0;

		Translate(out);
		return this->UncompressedSize;
	}

	private: byte *CopyUncompressedBlock(byte *out) {
		// Align to 16-bit boundary
		BSSkip(&this->Bits, 32 - this->Bits.bits); //16 - this->Bits->GetBitPosition());

		// Get R0, R1, and R2
		//TODO: clear out the mask
		/*uint32_t ReadUInt32() {
			if (this->bitPos != 0) {
				// error! not aligned
			}
			this->bitPos = 32;
			return ((this->value >> 16) & 0xFFFF) | ((this->value << 16) & 0xFFFF0000);
		}*/
		//this->RepDistances[0] = this->Bits->ReadUInt32() - 1;
		//this->RepDistances[0] = (this->Bits.data.in[this->Bits.index] | (this->Bits.data.in[this->Bits.index+1] << 8) | (this->Bits.data.in[this->Bits.index+2] << 16) | (this->Bits.data.in[this->Bits.index+3] << 24)) - 1;
		if (this->Bits.bits != 32)
		{
			// error! not aligned
		}
		this->RepDistances[0] = ((this->Bits.mask >> 16) & 0xFFFF) | ((this->Bits.mask << 16) & 0xFFFF0000); // TODO: the swap might not be right
		this->Bits.index += 4;
		this->Bits.bits = 0; // TODO: allow the Bits to recover itself after the uncompressed block is done, probably be calling BSSkip(&this->Bits, 0) twice
		for (unsigned i = 1; i < kNumRepDistances; i++) {
			uint32_t rep = 0;
			for (unsigned j = 0; j < 4; j++)
				rep |= (uint32_t)this->Bits.data.in[this->Bits.index++] << (8 * j);
			this->RepDistances[i] = rep - 1;
		}
	
		// Copy the uncompressed data
		memcpy(out, this->Bits.data.in+this->Bits.index, this->UncompressedSize);
		this->Bits.index += this->UncompressedSize;

		// skip byte if uncompressed size is odd
		if (this->UncompressedSize & 1)
			++this->Bits.index;

		return out;
	}

	//private: bool ReadTable(byte *lastLevels, byte *newLevels, uint32_t numSymbols) {
	private: bool ReadTable(byte *newLevels, uint32_t numSymbols) {
		byte levelLevels[kLevelTableSize];
		uint32_t i;
		for (i = 0; i < kLevelTableSize; i++)
			levelLevels[i] = (byte)BSReadBits(&this->Bits, kNumBitsForPreTreeLevel);
		this->LevelDecoder.SetCodeLengths(levelLevels);
		unsigned num = 0;
		byte symbol = 0;
		for (i = 0; i < numSymbols;) {
			if (num != 0) {
				//lastLevels[i] = newLevels[i] = symbol;
				newLevels[i] = symbol;
				i++;
				num--;
				continue;
			}
			uint32_t number = this->LevelDecoder.DecodeSymbol(&this->Bits);
			if (number == kLevelSymbolZeros) {
				num = kLevelSymbolZerosStartValue + (unsigned)BSReadBits(&this->Bits, kLevelSymbolZerosNumBits);
				symbol = 0;
			} else if (number == kLevelSymbolZerosBig) {
				num = kLevelSymbolZerosBigStartValue + (unsigned)BSReadBits(&this->Bits, kLevelSymbolZerosBigNumBits);
				symbol = 0;
			} else if (number == kLevelSymbolSame || number <= NUM_HUFFMAN_BITS) {
				if (number <= NUM_HUFFMAN_BITS)
					num = 1;
				else {
					num = kLevelSymbolSameStartValue + (unsigned)BSReadBit(&this->Bits); // kLevelSymbolSameNumBits
					number = this->LevelDecoder.DecodeSymbol(&this->Bits);
					if (number > NUM_HUFFMAN_BITS)
						return false;
				}
				//symbol = byte((17 + lastLevels[i] - number) % (NUM_HUFFMAN_BITS + 1));
				symbol = (byte)((number==0) ? 0 : (17 - number)); //byte((17 - number) % (NUM_HUFFMAN_BITS + 1));
			} else
				return false;
		}
		return true;
	}

	private: byte *DecompressIt(byte *out) {
		size_t pos = 0;
		while (pos < this->UncompressedSize) {
			uint32_t number = this->MainDecoder.DecodeSymbol(&this->Bits);
			if (number < 256) {
				out[pos++] = (byte)number;
			} else {
				uint32_t posLenSlot = number - 256;
				if (posLenSlot >= kNumPosLenSlots)
					return NULL;
				uint32_t posSlot = posLenSlot >> 3; //posLenSlot / kNumLenSlots;
				uint32_t lenSlot = posLenSlot & 0x7; //posLenSlot % kNumLenSlots;
				uint32_t len = kMatchMinLen + lenSlot;
				if (lenSlot == kNumLenSlots - 1) {
					uint32_t lenTemp = this->LenDecoder.DecodeSymbol(&this->Bits);
					if (lenTemp >= kNumLenSymbols)
						return NULL;
					len += lenTemp;
				}
				if (posSlot < kNumRepDistances) {
					uint32_t distance = this->RepDistances[posSlot];
					this->RepDistances[posSlot] = this->RepDistances[0];
					this->RepDistances[0] = distance;
				} else {
					uint32_t distance;
					unsigned numDirectBits;
					if (posSlot < kNumPowerPosSlots) {
						numDirectBits = (unsigned)(posSlot >> 1) - 1;
						distance = ((2 | (posSlot & 1)) << numDirectBits);
					} else {
						numDirectBits = kNumLinearPosSlotBits;
						distance = ((posSlot - 0x22) << kNumLinearPosSlotBits);
					}
					if (this->IsAlignType && numDirectBits >= kNumAlignBits) {
						distance += (BSReadBits(&this->Bits, numDirectBits - kNumAlignBits) << kNumAlignBits);
						uint32_t alignTemp = this->AlignDecoder.DecodeSymbol(&this->Bits);
						if (alignTemp >= kAlignTableSize)
							return NULL;
						distance += alignTemp;
					} else
						distance += BSReadBits(&this->Bits, numDirectBits);
					this->RepDistances[2] = this->RepDistances[1];
					this->RepDistances[1] = this->RepDistances[0];
					this->RepDistances[0] = distance - kNumRepDistances;
				}

				if (len > this->UncompressedSize - pos)
					return NULL;

				uint32_t dist = this->RepDistances[0]+1;
				do {
					out[pos] = out[pos - dist];
					++pos;
				} while (--len != 0);
			}
		}
		return out;
	}

	private: void Translate(byte *data) {
		if (this->UncompressedSize >= 6) // && m_ProcessedSize < (1 << 30)
		{
			uint32_t numBytes = this->UncompressedSize - 6;
			for (uint32_t i = 0; i < numBytes; ) {
				if (data[i++] == 0xE8) {
					int32_t absValue = 0;
					int j;
					for (j = 0; j < 4; j++)
						absValue += (uint32_t)data[i + j] << (j * 8);
					int32_t pos = (int32_t)(i - 1); // possibly need to add the total number of bytes processed up to this point (m_ProcessedSize)
					if (absValue >= -pos && absValue < TRANSLATION_SIZE) {
						uint32_t offset = (absValue >= 0) ? absValue - pos : absValue + TRANSLATION_SIZE;
						for (j = 0; j < 4; j++) {
							data[i + j] = (byte)(offset & 0xFF);
							offset >>= 8;
						}
					}
					i += 4;
				}
			}
		}
	}
};

size_t lzx_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len) {
	LZX* lzx = new LZX(in, in_len);
	return lzx->DecompressTo(out);
}

#else

size_t lzx_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	PRINT_ERROR("LZX Decompression: Not implemented in C\n");
	errno = E_ILLEGAL_FORMAT;
	return 0;
}

#endif

size_t lzx_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	PRINT_ERROR("LZX Compression: Not implemented\n");
	errno = E_ILLEGAL_FORMAT;
	return 0;
}
