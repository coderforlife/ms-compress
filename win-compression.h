#pragma once

////////// NT compression functions from ntdll.dll //////////

#ifdef _WIN32

#include "win-min.h"

///// Dynamic Function Loading /////
#define FUNC(r, n, ...) typedef r (__stdcall* FUNC_##n)(__VA_ARGS__); FUNC_##n n = NULL;
#define LOAD_FUNC(func, hmod) (func = (FUNC_##func)GetProcAddress(hmod, #func))


///// From <ntdef.h> /////

typedef __success(return >= 0) LONG NTSTATUS;


///// From <ntstatus.h> /////

// The success status codes 0 - 63 are reserved for wait completion status.
// FacilityCodes 0x5 - 0xF have been allocated by various drivers.
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) // ntsubauth

#ifndef STATUS_INVALID_PARAMETER
// MessageId: STATUS_INVALID_PARAMETER
// MessageText: An invalid parameter was passed to a service or function.
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL) // winnt
#endif

// MessageId: STATUS_BUFFER_TOO_SMALL
// MessageText: {Buffer Too Small}
// The buffer is too small to contain the entry. No information has been written to the buffer.
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)

// MessageId: STATUS_NOT_SUPPORTED
// MessageText: The request is not supported.
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBL)

// MessageId: STATUS_BUFFER_ALL_ZEROS
// MessageText: Specified buffer contains all zeros.
#define STATUS_BUFFER_ALL_ZEROS          ((NTSTATUS)0x00000117L)

// MessageId: STATUS_BAD_COMPRESSION_BUFFER
// MessageText: The specified buffer contains ill-formed data.
#define STATUS_BAD_COMPRESSION_BUFFER    ((NTSTATUS)0xC0000242L)

// MessageId: STATUS_UNSUPPORTED_COMPRESSION
// MessageText: The specified compression format is unsupported.
#define STATUS_UNSUPPORTED_COMPRESSION   ((NTSTATUS)0xC000025FL)


///// From <ntifs.h> /////

//
//
//  Compression package types and procedures.
//

#define COMPRESSION_FORMAT_NONE          (0x0000)   // winnt
#define COMPRESSION_FORMAT_DEFAULT       (0x0001)   // winnt
#define COMPRESSION_FORMAT_LZNT1         (0x0002)   // winnt
#define COMPRESSION_FORMAT_XPRESS        (0x0003)   // added in Windows 8
#define COMPRESSION_FORMAT_XPRESS_HUFF   (0x0004)   // added in Windows 8

#define COMPRESSION_ENGINE_STANDARD      (0x0000)   // winnt
#define COMPRESSION_ENGINE_MAXIMUM       (0x0100)   // winnt
#define COMPRESSION_ENGINE_HIBER         (0x0200)   // winnt

//
//  Compressed Data Information structure.  This structure is
//  used to describe the state of a compressed data buffer,
//  whose uncompressed size is known.  All compressed chunks
//  described by this structure must be compressed with the
//  same format.  On compressed reads, this entire structure
//  is an output, and on compressed writes the entire structure
//  is an input.
//
//
//typedef struct _COMPRESSED_DATA_INFO {
//
//	//
//	//  Code for the compression format (and engine) as
//	//  defined in ntrtl.h.  Note that COMPRESSION_FORMAT_NONE
//	//  and COMPRESSION_FORMAT_DEFAULT are invalid if
//	//  any of the described chunks are compressed.
//	//
//
//	USHORT CompressionFormatAndEngine;
//
//	//
//	//  Since chunks and compression units are expected to be
//	//  powers of 2 in size, we express then log2.  So, for
//	//  example (1 << ChunkShift) == ChunkSizeInBytes.  The
//	//  ClusterShift indicates how much space must be saved
//	//  to successfully compress a compression unit - each
//	//  successfully compressed compression unit must occupy
//	//  at least one cluster less in bytes than an uncompressed
//	//  compression unit.
//	//
//
//	UCHAR CompressionUnitShift;
//	UCHAR ChunkShift;
//	UCHAR ClusterShift;
//	UCHAR Reserved;
//
//	//
//	//  This is the number of entries in the CompressedChunkSizes
//	//  array.
//	//
//
//	USHORT NumberOfChunks;
//
//	//
//	//  This is an array of the sizes of all chunks resident
//	//  in the compressed data buffer.  There must be one entry
//	//  in this array for each chunk possible in the uncompressed
//	//  buffer size.  A size of FSRTL_CHUNK_SIZE indicates the
//	//  corresponding chunk is uncompressed and occupies exactly
//	//  that size.  A size of 0 indicates that the corresponding
//	//  chunk contains nothing but binary 0's, and occupies no
//	//  space in the compressed data.  All other sizes must be
//	//  less than FSRTL_CHUNK_SIZE, and indicate the exact size
//	//  of the compressed data in bytes.
//	//
//
//	ULONG CompressedChunkSizes[ANYSIZE_ARRAY];
//
//} COMPRESSED_DATA_INFO;
//typedef COMPRESSED_DATA_INFO *PCOMPRESSED_DATA_INFO;

FUNC(NTSYSAPI NTSTATUS, RtlGetCompressionWorkSpaceSize,
	__in USHORT CompressionFormatAndEngine, __out PULONG CompressBufferWorkSpaceSize, __out PULONG CompressFragmentWorkSpaceSize);
FUNC(NTSYSAPI NTSTATUS, RtlCompressBuffer,
	__in USHORT CompressionFormatAndEngine,
	__in_bcount(UncompressedBufferSize) PUCHAR UncompressedBuffer, __in ULONG UncompressedBufferSize,
	__out_bcount_part(CompressedBufferSize, *FinalCompressedSize) PUCHAR CompressedBuffer, __in ULONG CompressedBufferSize,
	__in ULONG UncompressedChunkSize, __out PULONG FinalCompressedSize, __in PVOID WorkSpace);
FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlDecompressBuffer,
	__in USHORT CompressionFormat,
	__out_bcount_part(UncompressedBufferSize, *FinalUncompressedSize) PUCHAR UncompressedBuffer, __in ULONG UncompressedBufferSize,
	__in_bcount(CompressedBufferSize) PUCHAR CompressedBuffer, __in ULONG CompressedBufferSize,
	__out PULONG FinalUncompressedSize);
FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlDecompressBufferEx, // added in Windows 8
	__in USHORT CompressionFormat,
	__out_bcount_part(UncompressedBufferSize, *FinalUncompressedSize) PUCHAR UncompressedBuffer, __in ULONG UncompressedBufferSize,
	__in_bcount(CompressedBufferSize) PUCHAR CompressedBuffer, __in ULONG CompressedBufferSize,
	__out PULONG FinalUncompressedSize, __in PVOID WorkSpace);
FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlDecompressFragment,
	__in USHORT CompressionFormat,
	__out_bcount_part(UncompressedFragmentSize, *FinalUncompressedSize) PUCHAR UncompressedFragment, __in ULONG UncompressedFragmentSize,
	__in_bcount(CompressedBufferSize) PUCHAR CompressedBuffer, __in ULONG CompressedBufferSize,
	__in_range(<, CompressedBufferSize) ULONG FragmentOffset, __out PULONG FinalUncompressedSize, __in PVOID WorkSpace );

// Reserved for system use:
//FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlDescribeChunk,
//	__in USHORT CompressionFormat,
//	__inout PUCHAR *CompressedBuffer, __in PUCHAR EndOfCompressedBufferPlus1,
//	__out PUCHAR *ChunkBuffer, __out PULONG ChunkSize);
//FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlReserveChunk,
//	__in USHORT CompressionFormat,
//	__inout PUCHAR *CompressedBuffer, __in PUCHAR EndOfCompressedBufferPlus1,
//	__out PUCHAR *ChunkBuffer, __in ULONG ChunkSize);
//FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlDecompressChunks,
//	__out_bcount(UncompressedBufferSize) PUCHAR UncompressedBuffer, __in ULONG UncompressedBufferSize,
//	__in_bcount(CompressedBufferSize) PUCHAR CompressedBuffer, __in ULONG CompressedBufferSize,
//	__in_bcount(CompressedTailSize) PUCHAR CompressedTail, __in ULONG CompressedTailSize, __in PCOMPRESSED_DATA_INFO CompressedDataInfo);
//FUNC(__drv_maxIRQL(APC_LEVEL) NTSYSAPI NTSTATUS, RtlCompressChunks,
//	__in_bcount(UncompressedBufferSize) PUCHAR UncompressedBuffer, __in ULONG UncompressedBufferSize,
//	__out_bcount(CompressedBufferSize) PUCHAR CompressedBuffer, __in_range(>=, (UncompressedBufferSize - (UncompressedBufferSize / 16))) ULONG CompressedBufferSize,
//	__inout_bcount(CompressedDataInfoLength) PCOMPRESSED_DATA_INFO CompressedDataInfo, __in_range(>, sizeof(COMPRESSED_DATA_INFO)) ULONG CompressedDataInfoLength, __in PVOID WorkSpace);

#endif
