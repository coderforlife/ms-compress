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


////////////////////////////// Bitstreams //////////////////////////////////////////////////////////
// A bitstream that allows either reading or writing, but not both at the same time.
// It reads uint16s for bits and 16 bits can be reliably read at a time

#ifndef BITSTREAM_H
#define BITSTREAM_H
#include "compression-api.h"

WARNINGS_PUSH()
WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED()

class Bitstream
{
protected:
	size_t index;	    // The current position of the stream
	const size_t len;	// The length of the stream
	uint32_t mask;		// The next bits to be read/written in the bitstream
	byte bits;			// The number of bits in mask that are valid
	inline Bitstream(size_t len, uint32_t mask, byte bits) : index(4), len(len), mask(mask), bits(bits) { assert(4 <= len); }
public:
	inline size_t RawPosition() { return this->index; }
	inline byte RemainingBits() { return this->bits; }
};

// Reading functions:
class InputBitstream : public Bitstream
{
private:
	const const_bytes in;
public:
	inline InputBitstream(const_bytes in, size_t len) : Bitstream(len, (GET_UINT16(in) << 16) | GET_UINT16(in+2), 32), in(in) { assert(in); }
	inline uint32_t Peek(byte n) const { assert(n <= 16); return (n > this->bits) ? 0xFFFFFFFF : ((n == 0) ? 0 : (this->mask >> (32 - n))); }
	inline void Skip(byte n)
	{
		this->mask <<= n;
		this->bits -= n;
		if (this->bits < 16 && this->index + 2 <= this->len)
		{
			this->mask |= GET_UINT16(this->in + this->index) << (16 - this->bits);
			this->bits |= 0x10; //this->bits += 16;
			this->index += 2;
		}
	}
	inline bool ReadBit() { byte x = 0xFF; if (this->bits) { x = (byte)(this->mask >> 31); this->Skip(1); } return x != 0; }
	inline uint32_t ReadBits(byte n) { assert(n <= 16); uint32_t x = this->Peek(n); if (x != 0xFFFFFFFF) { this->Skip(n); } return x; }
	inline uint32_t ReadManyBits(byte n) { assert(n > 16); return (this->ReadBits(n - 16) << 16) | this->ReadBits(16); }
	inline uint32_t ReadUInt32() { return (this->ReadBits(16) << 16) | this->ReadBits(16); }

	inline const_bytes Get16BitAlignedByteStream(size_t nBytes)
	{
		size_t start = this->index;
		     if (this->bits == 32) { start -= 4; }
		else if (this->bits >= 16) { start -= 2; }
		if (start + nBytes > this->len) { return NULL; }
		this->bits = 0;
		this->index = start + nBytes + (nBytes & 1); // make sure the end is also 16-bit aligned
		this->Skip(0);
		return this->in + start;
	}

	inline size_t RemainingBytes() { return this->len - this->index + this->bits / 8; } // rounds down, but bits is typically between 16 and 32, adding 2 to 4 bytes
	inline size_t RemainingRawBytes() { return this->len - this->index; }

	// Assume that you have already checked for necessary room
	inline byte     ReadRawByte()   { assert(this->index + 1 <= this->len); return this->in[this->index++]; }
	inline uint16_t ReadRawUInt16() { assert(this->index + 2 <= this->len); uint16_t x = GET_UINT16(this->in+this->index); this->index += 2; return x; }
	inline uint32_t ReadRawUInt32() { assert(this->index + 4 <= this->len); uint32_t x = GET_UINT32(this->in+this->index); this->index += 4; return x; }

	inline bool MaskIsZero() { return (this->mask>>(32-this->bits)) == 0; }
};

// Writing functions:
class OutputBitstream : public Bitstream
{
private:
	bytes out;
	uint16_t* pntr[2];	// the uint16's to write the data in mask to when there are enough bits
public:
	inline OutputBitstream(bytes out, size_t len) : Bitstream(len, 0, 0), out(out)
	{
		assert(out);
		this->pntr[0] = (uint16_t*)out;
		this->pntr[1] = (uint16_t*)(out+2);
	}
	inline bool WriteBit(byte b) { return this->WriteBits(b, 1); }
	inline bool WriteBits(uint32_t b, byte n)
	{
		assert(n <= 16);
		this->mask |= b << (32 - (this->bits += n));
		if (this->bits > 16)
		{
			if (this->pntr[1] == NULL) return false; // only 16 bits can fit into pntr[0]!
			SET_UINT16(this->pntr[0], this->mask >> 16);
			this->mask <<= 16;
			this->bits &= 0xF; //this->bits -= 16;
			this->pntr[0] = this->pntr[1];
			if (this->index + 2 > this->len)
			{
				// No more uint16s are available, however we can still write 16 more bits to pntr[0]
				this->pntr[1] = NULL;
			}
			else
			{
				this->pntr[1] = (uint16_t*)(this->out+this->index);
				this->index += 2;
			}
		}
		return true;
	}
	inline bool WriteManyBits(uint32_t x, byte n) { assert(n > 16); return this->WriteBits(x >> 16, n - 16) && this->WriteBits(x & 0xFFFF, 16); }
	inline bool WriteUInt32(uint32_t x) { return this->WriteBits(x >> 16, 16) && this->WriteBits(x & 0xFFFF, 16); }

	inline bytes Get16BitAlignedByteStream(size_t nBytes)
	{
		// Flush, aligning to 16 bit boundary [ add 1 - 16 bits ]
		this->WriteBit(0);
		this->WriteBits(0, 16 - this->bits);
		// now (this->bits == 16)
		SET_UINT16(this->pntr[0], this->mask >> 16);
		if (this->index + nBytes - 2 > this->len) { return NULL; }
		this->bits = 0;
		bytes out = this->out + this->index - 2;
		this->index += 2 + nBytes + (nBytes & 1);
		// TODO: some additional checks for going over the end should be done here
		// TODO: actually this is probably the end with no more reading
		this->pntr[0] = (uint16_t*)(this->out + this->index - 4);
		this->pntr[1] = (uint16_t*)(this->out + this->index - 2);
		return out;
	}

	inline bool WriteRawByte(byte x)       { if (this->index + 1 > this->len) { return false; } this->out[this->index++] = x; return true; }
	inline bool WriteRawUInt16(uint16_t x) { if (this->index + 2 > this->len) { return false; } SET_UINT16(this->out + this->index, x); this->index += 2; return true; }
	inline bool WriteRawUInt32(uint32_t x) { if (this->index + 4 > this->len) { return false; } SET_UINT32(this->out + this->index, x); this->index += 4; return true; }
	inline bool Flush() { return !this->bits || this->WriteBits(0, 16 - this->bits); } // aligns to a 16 bit boundary
	inline size_t Finish()
	{
		SET_UINT16(this->pntr[0], this->mask >> 16); // if !bits then mask is 0 anyways
		if (this->pntr[1]) *this->pntr[1] = 0;
		return this->index;
	}
};

WARNINGS_POP()

#endif
