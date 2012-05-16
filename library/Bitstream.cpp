#include "Bitstream.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif


// Reading functions:
void BSReadInit(InputBitstream* bstr, const_bytes in, size_t len)
{
	assert(in);
	assert(4 < len);
	bstr->data.in = in;
	bstr->index = 4;
	bstr->len = len;
	bstr->mask = (GET_UINT16(in) << 16) | GET_UINT16(in+2);
	bstr->bits = 32;
	bstr->pntr[0] = NULL;
	bstr->pntr[1] = NULL;
}
uint32_t BSPeek(const InputBitstream* bstr, byte n) { return (n > bstr->bits) ? 0xFFFFFFFF : ((n == 0) ? 0 : (bstr->mask >> (32 - n))); }
void BSSkip(InputBitstream* bstr, byte n)
{
	bstr->mask <<= n;
	bstr->bits -= n;
	if (bstr->bits < 16 && bstr->index + 2 <= bstr->len)
	{
		bstr->mask |= GET_UINT16(bstr->data.in + bstr->index) << (16 - bstr->bits);
		bstr->bits |= 0x10; //bstr->bits += 16;
		bstr->index += 2;
	}
}
byte BSReadBit(InputBitstream* bstr) { byte x = 0xFF; if (bstr->bits) { x = (byte)(bstr->mask >> 31); BSSkip(bstr, 1); } return x; }
uint32_t BSReadBits(InputBitstream* bstr, byte n) { uint32_t x = BSPeek(bstr, n); if (x != 0xFFFFFFFF) { BSSkip(bstr, n); } return x; }


// Writing functions:
void BSWriteInit(OutputBitstream* bstr, bytes out, size_t len)
{
	assert(out);
	assert(4 <= len);
	bstr->data.out = out;
	bstr->index = 4;
	bstr->len = len;
	bstr->mask = 0;
	bstr->bits = 0;
	bstr->pntr[0] = (uint16_t*)out;
	bstr->pntr[1] = (uint16_t*)(out+2);
}
bool BSWriteBits(OutputBitstream* bstr, uint32_t b, byte n)
{
	bstr->mask |= b << (32 - (bstr->bits += n));
	if (bstr->bits > 16)
	{
		if (bstr->pntr[1] == NULL) return false; // only 16 bits can fit into pntr[0]!
		SET_UINT16(bstr->pntr[0], bstr->mask >> 16);
		bstr->mask <<= 16;
		bstr->bits &= 0xF; //bstr->bits -= 16;
		bstr->pntr[0] = bstr->pntr[1];
		if (bstr->index + 2 > bstr->len)
		{
			// No more uint16s are available, however we can still write 16 more bits to pntr[0]
			bstr->pntr[1] = NULL;
		}
		else
		{
			bstr->pntr[1] = (uint16_t*)(bstr->data.out+bstr->index);
			bstr->index += 2;
		}
	}
	return true;
}
bool BSWriteByte(OutputBitstream* bstr, byte b)
{
	if (bstr->index >= bstr->len) return false;
	bstr->data.out[bstr->index++] = b;
	return true;
}
void BSWriteFinish(OutputBitstream* bstr)
{
	SET_UINT16(bstr->pntr[0], bstr->mask >> 16); // if !bits then mask is 0 anyways
	if (bstr->pntr[1]) *bstr->pntr[1] = 0;
}
