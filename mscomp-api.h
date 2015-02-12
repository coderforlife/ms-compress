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


// General include file which includes necessary files, defines, and typedefs.

#ifndef MSCOMP_API_H
#define MSCOMP_API_H

// Compile Options
#if !defined(MSCOMP_WITH_OPT_COMPRESS) && !defined(MSCOMP_WITHOUT_OPT_COMPRESS)
#define MSCOMP_WITH_OPT_COMPRESS       // Without this option the single-call compressors are simple wrappers around the stream functions
#endif
#if !defined(MSCOMP_WITH_OPT_DECOMPRESS) && !defined(MSCOMP_WITHOUT_OPT_DECOMPRESS)
#define MSCOMP_WITH_OPT_DECOMPRESS     // Without this option the single-call decompressors are simple wrappers around the stream functions
#endif
#if !defined(MSCOMP_WITH_ERROR_MESSAGES) && !defined(MSCOMP_WITHOUT_ERROR_MESSAGES)
#define MSCOMP_WITH_ERROR_MESSAGES     // Includes detailed error messages in the stream object (mainly for debugging, error codes are always given)
#endif
#if !defined(MSCOMP_WITH_WARNING_MESSAGES) && !defined(MSCOMP_WITHOUT_WARNING_MESSAGES)
#define MSCOMP_WITH_WARNING_MESSAGES   // Includes detailed warning messages in the stream object (mainly for debugging)
#endif

#if !defined(MSCOMP_WITH_LZNT1) && !defined(MSCOMP_WITHOUT_LZNT1)
#define MSCOMP_WITH_LZNT1
#endif
#if !defined(MSCOMP_WITH_XPRESS) && !defined(MSCOMP_WITHOUT_XPRESS)
#define MSCOMP_WITH_XPRESS
#endif
#if !defined(MSCOMP_WITH_XPRESS_HUFF) && !defined(MSCOMP_WITHOUT_XPRESS_HUFF)
#define MSCOMP_WITH_XPRESS_HUFF
#endif
#if !defined(MSCOMP_WITH_LZX) && !defined(MSCOMP_WITHOUT_LZX)
//#define MSCOMP_WITH_LZX // not working yet - never enable!
#endif

#ifdef MSCOMP_API_EXPORT

#include "mscomp-api-internal.h"

#else // Importing from DLL or LIB

#include <stdint.h>
#ifdef MSCOMP_API_DLL
	#ifdef _WIN32
		#define MSCOMPAPI __declspec(dllexport)
	#else
		#define MSCOMPAPI
	#endif
#else
	#define MSCOMPAPI
#endif

#endif

#ifndef __cplusplus
	#define EXTERN_C_START
	#define EXTERN_C_END
#else
	#define EXTERN_C_START extern "C" {
	#define EXTERN_C_END   }
#endif

// Define types used
typedef uint8_t byte; // should always be unsigned char (there is a check for CHAR_BIT == 8 above)
typedef byte* bytes;
typedef const byte const_byte;
typedef const_byte* const_bytes;

typedef struct _mscomp_internal_state mscomp_internal_state;

// Formats supported
typedef enum _MSCompFormat {
	MSCOMP_NONE			= 0, // COMPRESSION_FORMAT_NONE
	MSCOMP_RESERVED		= 1, // Called COMPRESSION_FORMAT_DEFAULT in MSDN but can never be used
	MSCOMP_LZNT1		= 2, // COMPRESSION_FORMAT_LZNT1
	MSCOMP_XPRESS		= 3, // COMPRESSION_FORMAT_XPRESS
	MSCOMP_XPRESS_HUFF	= 4, // COMPRESSION_FORMAT_XPRESS_HUFF
} MSCompFormat;

// Error Codes
typedef enum _MSCompStatus
{
	MSCOMP_OK					= 0,
	MSCOMP_STREAM_END			= 1,

	MSCOMP_ERRNO				= -1,
	MSCOMP_ARG_ERROR			= -2,
	MSCOMP_DATA_ERROR			= -3,
	MSCOMP_MEM_ERROR			= -4,
	MSCOMP_BUF_ERROR			= -5,
} MSCompStatus;

// Compression Stream Object
typedef struct _mscomp_stream {
	MSCompFormat	format;
	bool		compressing;

	const_bytes	in;			// next input byte
	size_t		in_avail;	// number of bytes available at next_in
	size_t		in_total;	// total number of input bytes read so far

	bytes		out;		// next output byte should be put there
	size_t		out_avail;	// remaining free space at next_out
	size_t		out_total;	// total number of bytes output so far

#ifdef MSCOMP_WITH_ERROR_MESSAGES
	char error[256];
#endif
#ifdef MSCOMP_WITH_WARNING_MESSAGES
	char warning[256];
#endif

	mscomp_internal_state* state;
} mscomp_stream;

#endif
