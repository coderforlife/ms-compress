////////////////////////////// Bitstreams //////////////////////////////////////////////////////////
// A bitstream that allows either reading or writing, but not both at the same time.
// It reads uint16s for bits and 16 bits can be reliably read at a time

#ifndef BITSTREAM_H
#define BITSTREAM_H
#include "compression-api.h"

struct _Bitstream
{
	union
	{
		const_bytes in;	// Reading only: The input byte array
		bytes out;		// Writing only: The output byte array
	} data;
	size_t index, len;	// The current position and length of the stream
	uint32_t mask;		// The next bits to be read/written in the bitstream
	byte bits;			// The number of bits in mask that are valid
	uint16_t* pntr[2];	// Writing only: the uint16's to write the data in mask to when there are enough bits
};
typedef struct _Bitstream InputBitstream;
typedef struct _Bitstream OutputBitstream;


// Reading functions:
void BSReadInit(InputBitstream* bstr, const_bytes in, size_t len);
uint32_t BSPeek(const InputBitstream* bstr, byte n);
void BSSkip(InputBitstream* bstr, byte n);
byte BSReadBit(InputBitstream* bstr);
uint32_t BSReadBits(InputBitstream* bstr, byte n);


// Writing functions:
void BSWriteInit(OutputBitstream* bstr, bytes out, size_t len);
bool BSWriteBits(OutputBitstream* bstr, uint32_t b, byte n);
bool BSWriteByte(OutputBitstream* bstr, byte b);
void BSWriteFinish(OutputBitstream* bstr);

#endif
