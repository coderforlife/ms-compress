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
// It reads uint16s for bits and 16 bits can be reliably read at a time.
// These are designed for speed and perform few checks. The burden of checking is on the caller.
// See the functions for assumptions they make that should be checked by the caller (asserts check
// these in the functions as well). Note that this->bits is >= 16 unless near the very end of the
// stream.

#ifndef MSCOMP_BITSTREAM_H
#define MSCOMP_BITSTREAM_H
#include "internal.h"

WARNINGS_PUSH()
WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED()

// Reading functions:
class InputBitstream 
{
private:
	const_bytes in;
	const const_bytes in_end;
	uint32_t mask;		// The next bits to be read/written in the bitstream
	uint_fast8_t bits;	// The number of bits in mask that are valid
public:
	// Create an input bitstream
	//   Assumption: in != NULL && in_end - in >= 4
	INLINE InputBitstream(const_bytes in, const const_bytes in_end) : in(in+4), in_end(in_end), mask((GET_UINT16(in) << 16) | GET_UINT16(in+2)), bits(32) { assert(in); assert(in_end - in >= 4); }
	
	///// Basic Properties /////
	FORCE_INLINE const_bytes RawStream() { return this->in; }
	FORCE_INLINE uint_fast8_t AvailableBits() const { return this->bits; }
	// Get the remaining number of bytes, including any whole bytes retrievable from the pre-read bits
	FORCE_INLINE size_t RemainingBytes() const { return this->in_end - this->in + this->bits / 8; } // rounds down, but bits is typically between 16 and 32, adding 2 to 4 bytes
	// Get the remaining number of raw bytes (disregards pre-read bits)
	FORCE_INLINE size_t RemainingRawBytes() const { return this->in_end - this->in; }

	///// Peeking Functions /////
	// Peek at the next n bits of the stream
	//   Assumption: n <= 16 && n <= this->bits
	FORCE_INLINE uint32_t Peek(const uint_fast8_t n) const { ASSERT_ALWAYS(n <= 16); assert(n <= this->bits); return (this->mask >> 16) >> (16 - n); } // we can't do a single shift because if n is 0 then a shift by 32 is undefined
	// Check if all pre-read bits are 0, essentially Peek(RemainingBits()) == 0 (except that RemainingBits can be larger than 16, which Peek does not allow)
	// If there are 0 pre-read bits, returns true
	FORCE_INLINE bool MaskIsZero() const { return this->bits == 0 || (this->mask>>(32-this->bits)) == 0; }
	
	///// Skipping Functions /////
	// Skip the next n bits of the stream
	//   Assumption: n <= 16 && n <= this->bits
	INLINE void Skip(const uint_fast8_t n)
	{
		ASSERT_ALWAYS(n <= 16); ASSERT_ALWAYS(n <= this->bits);
		this->mask <<= n;
		this->bits -= n;
		if (this->bits < 16 && this->in + 2 <= this->in_end)
		{
			this->mask |= GET_UINT16(this->in) << (16 - this->bits);
			this->bits |= 0x10; //this->bits += 16;
			this->in += 2;
		}
	}
	// Skip the next n bits of the stream without bounds checks
	//   Assumption: n <= 16
	INLINE void Skip_Fast(const uint_fast8_t n)
	{
		ASSERT_ALWAYS(n <= 16);
		this->mask <<= n;
		this->bits -= n;
		if (this->bits < 16)
		{
			this->mask |= GET_UINT16(this->in) << (16 - this->bits);
			this->bits |= 0x10; //this->bits += 16;
			this->in += 2;
		}
	}
	
	///// Reading Functions /////
	// Read the next bit of the stream (essentially Peek(1); Skip(1))
	//   Assumption: 1 <= this->bits
	INLINE bool ReadBit() { assert(this->bits); const uint32_t x = this->mask; this->Skip(1); return (x & 0x80000000) != 0; }
	// Read the next n bits of the stream, where n <= 16 (essentially Peek(n); Skip(n))
	//   Assumption: n <= 16 && n <= this->bits
	INLINE uint32_t ReadBits(const uint_fast8_t n) { ASSERT_ALWAYS(n <= 16); const uint32_t x = (this->mask >> 16) >> (16 - n); this->Skip(n); return x; }
	// Read the next n bits of the stream, where n > 16 (essentially ReadBits called twice)
	//   Assumption: n > 16 && this->bits >= 16 && (this->RemainingRawBytes() >= 2 || this->bits == 32)
	INLINE uint32_t ReadManyBits(const uint_fast8_t n)
	{
		ASSERT_ALWAYS(n > 16); ASSERT_ALWAYS(this->bits >= 16); ASSERT_ALWAYS(this->in + 2 <= this->in_end || this->bits == 32);
		uint32_t x = (this->mask >> (32 - n)) & 0xFFFF0000; this->Skip(n-16); x |= this->mask >> 16; this->Skip(16); return x;
	}
	// Read the next 32 bits of the stream (essentially ReadManyBits(32))
	//   Assumption: this->bits >= 16 && (this->RemainingRawBytes() >= 2 || this->bits == 32)
	INLINE uint32_t ReadUInt32()
	{
		ASSERT_ALWAYS(this->bits >= 16); ASSERT_ALWAYS(this->in + 2 <= this->in_end || this->bits == 32);
		uint32_t x = this->mask & 0xFFFF0000; this->Skip(16); x |= this->mask >> 16; this->Skip(16); return x;
	}
	
	///// Fast Reading Functions /////
	// Equivalent to the reading functions but do not do bounds checks
	INLINE bool ReadBit_Fast() { const uint32_t x = this->mask; this->Skip_Fast(1); return (x & 0x80000000) != 0; }
	// Read the next n bits of the stream, where n <= 16 (essentially Peek(n); Skip_Fast(n))
	//   Assumption: n <= 16
	INLINE uint32_t ReadBits_Fast(const uint_fast8_t n) { ASSERT_ALWAYS(n <= 16); const uint32_t x = (this->mask >> 16) >> (16 - n); this->Skip_Fast(n); return x; }
	// Read the next n bits of the stream, where n > 16 (essentially ReadBits called twice)
	//   Assumption: n > 16
	INLINE uint32_t ReadManyBits_Fast(const uint_fast8_t n)
	{
		ASSERT_ALWAYS(n > 16);
		uint32_t x = (this->mask >> (32 - n)) & 0xFFFF0000; this->Skip_Fast(n-16); x |= this->mask >> 16; this->Skip_Fast(16); return x;
	}
	// Read the next 32 bits of the stream (essentially ReadManyBits(32))
	INLINE uint32_t ReadUInt32_Fast()
	{
		uint32_t x = this->mask & 0xFFFF0000; this->Skip_Fast(16); x |= this->mask >> 16; this->Skip_Fast(16); return x;
	}
	
	///// Raw Reading Functions /////
	// These never do bounds checking
	// Get the next raw byte (from the underlying stream, not the pre-read bits)
	//   Assumption: this->RemainingRawBytes() >= 1
	INLINE byte     ReadRawByte()   { assert(this->in + 1 <= this->in_end); return *this->in++; }
	// Get the next raw uint16 (from the underlying stream, not the pre-read bits)
	//   Assumption: this->RemainingRawBytes() >= 2
	INLINE uint16_t ReadRawUInt16() { assert(this->in + 2 <= this->in_end); const uint16_t x = GET_UINT16(this->in); this->in += 2; return x; }
	// Get the next raw uint32 (from the underlying stream, not the pre-read bits)
	//   Assumption: this->RemainingRawBytes() >= 4
	INLINE uint32_t ReadRawUInt32() { assert(this->in + 4 <= this->in_end); const uint32_t x = GET_UINT32(this->in); this->in += 4; return x; }
	

	// Gets a 16-bit aligned byte stream from the underlying stream.
	// Any pre-read bits are returned (in multiples of 16 bits)
	// This chunk of data is skipped, and the bit stream is restarted after it
	INLINE const_bytes Get16BitAlignedByteStream(const size_t nBytes)
	{
		const_bytes str = this->in;
		     if (UNLIKELY(this->bits == 32)) { str -= 4; }
		else if (LIKELY  (this->bits >= 16)) { str -= 2; }
		if (str + nBytes > this->in_end) { return NULL; }
		this->bits = 0;
		this->in = str + nBytes + (nBytes & 1); // make sure the end is also 16-bit aligned
		this->Skip(0);
		return str;
	}
};

class BitstreamX
{
protected:
	size_t index;	    // The current position of the stream
	const size_t len;	// The length of the stream
	uint32_t mask;		// The next bits to be read/written in the bitstream
	uint_fast8_t bits;	// The number of bits in mask that are valid
	INLINE BitstreamX(const size_t len, const uint32_t mask, const uint_fast8_t bits) : index(4), len(len), mask(mask), bits(bits) { assert(len >= 4); }
public:
	INLINE size_t RawPosition() { return this->index; }
	INLINE uint_fast8_t RemainingBits() { return this->bits; }
};


// Writing functions:
class OutputBitstream : public BitstreamX
{
private:
	bytes out;
	uint16_t* pntr[2];	// the uint16's to write the data in mask to when there are enough bits
public:
	INLINE OutputBitstream(bytes out, size_t len) : BitstreamX(len, 0, 0), out(out)
	{
		assert(out);
		this->pntr[0] = (uint16_t*)out;
		this->pntr[1] = (uint16_t*)(out+2);
	}
	INLINE bool WriteBit(byte b) { return this->WriteBits(b, 1); }
	INLINE bool WriteBits(uint32_t b, byte n)
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
	INLINE bool WriteManyBits(uint32_t x, byte n) { assert(n > 16); return this->WriteBits(x >> 16, n - 16) && this->WriteBits(x & 0xFFFF, 16); }
	INLINE bool WriteUInt32(uint32_t x) { return this->WriteBits(x >> 16, 16) && this->WriteBits(x & 0xFFFF, 16); }

	INLINE bytes Get16BitAlignedByteStream(size_t nBytes)
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

	INLINE bool WriteRawByte(byte x)       { if (this->index + 1 > this->len) { return false; } this->out[this->index++] = x; return true; }
	INLINE bool WriteRawUInt16(uint16_t x) { if (this->index + 2 > this->len) { return false; } SET_UINT16(this->out + this->index, x); this->index += 2; return true; }
	INLINE bool WriteRawUInt32(uint32_t x) { if (this->index + 4 > this->len) { return false; } SET_UINT32(this->out + this->index, x); this->index += 4; return true; }
	INLINE bool Flush() { return !this->bits || this->WriteBits(0, 16 - this->bits); } // aligns to a 16 bit boundary
	INLINE size_t Finish()
	{
		SET_UINT16(this->pntr[0], this->mask >> 16); // if !bits then mask is 0 anyways
		if (this->pntr[1]) *this->pntr[1] = 0;
		return this->index;
	}
};

WARNINGS_POP()

#endif
