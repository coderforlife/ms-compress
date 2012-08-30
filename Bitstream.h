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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4512) // warning C4512: assignment operator could not be generated
#endif

class Bitstream
{
protected:
	size_t index;	    // The current position of the stream
	const size_t len;	// The length of the stream
	uint32_t mask;		// The next bits to be read/written in the bitstream
	byte bits;			// The number of bits in mask that are valid
	inline Bitstream(size_t len, uint32_t mask, byte bits) : len(len), index(4), mask(mask), bits(bits) { assert(4 < len); }
public:
	inline size_t Position() { return this->index; }
};

// Reading functions:
class InputBitstream : public Bitstream
{
private:
	const const_bytes in;
public:
	inline InputBitstream(const_bytes in, size_t len) : Bitstream(len, (GET_UINT16(in) << 16) | GET_UINT16(in+2), 32), in(in) { assert(in); }
	inline uint32_t Peek(byte n) const { return (n > this->bits) ? 0xFFFFFFFF : ((n == 0) ? 0 : (this->mask >> (32 - n))); }
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
	inline byte ReadBit() { byte x = 0xFF; if (this->bits) { x = (byte)(this->mask >> 31); this->Skip(1); } return x; }
	inline uint32_t ReadBits(byte n) { uint32_t x = this->Peek(n); if (x != 0xFFFFFFFF) { this->Skip(n); } return x; }

	inline size_t RemainingBytes() { return this->len - this->index; }
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
	inline bool WriteBits(uint32_t b, byte n)
	{
		{
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
	}
	inline bool WriteRawByte(byte x)       { if (this->index + 1 > this->len) { return false; } this->out[this->index++] = x; return true; }
	inline bool WriteRawUInt16(uint16_t x) { if (this->index + 2 > this->len) { return false; } SET_UINT16(this->out + this->index, x); this->index += 2; return true; }
	inline bool WriteRawUInt32(uint32_t x) { if (this->index + 4 > this->len) { return false; } SET_UINT32(this->out + this->index, x); this->index += 4; return true; }
	inline void Finish()
	{
		SET_UINT16(this->pntr[0], this->mask >> 16); // if !bits then mask is 0 anyways
		if (this->pntr[1]) *this->pntr[1] = 0;
	}
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
