#pragma once

// For MSC
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

// For GCC
#define __STDC_LIMIT_MACROS		

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

// Custom errno values
#define E_ILLEGAL_FORMAT		0x101
#define E_INSUFFICIENT_BUFFER	0x102
#define E_INVALID_DATA			0x103

// Check that it is 8 bits to the byte
#if CHAR_BIT != 8
	#error Unsupported char size
#endif

// Determine the endianness of the compilation, however this isn't very accurate
// It would be much better to define LITTLE_ENDIAN, BIG_ENDIAN, or PDP_ENDIAN yourself
// LITTLE_ENDIAN is what the program is developed for and tested with
// BIG_ENDIAN and PDP_ENDIAN are untested
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(PDP_ENDIAN) && !defined(_MSC_VER) && !defined(_WIN32) && !defined(__LITTLE_ENDIAN__) && !defined(__IEEE_LITTLE_ENDIAN)
	#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) || defined(__IEEE_BIG_ENDIAN)
		#define BIG_ENDIAN
	#else
		#include <endian.h>
		#if (defined(__PDP_ENDIAN) && !defined(__LITTLE_ENDIAN)) || __BYTE_ORDER == __PDP_ENDIAN
			#define PDP_ENDIAN
		#elif (defined(__BIG_ENDIAN) && !defined(__LITTLE_ENDIAN)) || __BYTE_ORDER == __BIG_ENDIAN
			#define BIG_ENDIAN
		#endif
	#endif
#endif

#if defined(BIG_ENDIAN)
	#define GET_UINT16(x)		((x)[0]|((x)[1]<<8))
	#define GET_UINT32(x)		((x)[0]|((x)[1]<<8)|((x)[2]<<16)|((x)[3]<<24))
	#define SET_UINT16(x,val)	(((byte*)(x))[0]=(byte)(val), ((byte*)(x))[1]=(byte)((val) >> 8))
	#define SET_UINT32(x,val)	(((byte*)(x))[0]=(byte)(val), ((byte*)(x))[1]=(byte)((val) >> 8), ((byte*)(x))[2]=(byte)((val) >> 16), ((byte*)(x))[3]=(byte)((val) >> 24))
#elif defined(PDP_ENDIAN) // for 16-bit ints its the same as little-endian
	#define GET_UINT16(x)		(*(const uint16_t*)(x))
	#define GET_UINT32(x)		(*(const uint16_t*)(x)|(*(const uint16_t*)((x)+2)<<16))
	#define SET_UINT16(x,val)	(*(uint16_t*)(x) = (uint16_t)(val))
	#define SET_UINT32(x,val)	(*(uint16_t*)(x) = (uint16_t)(val), *(((uint16_t*)(x))+1) = (uint16_t)((val) >> 16))
#else
	#ifndef LITTLE_ENDIAN
		#define LITTLE_ENDIAN
	#endif
	#define GET_UINT16(x)		(*(const uint16_t*)(x))
	#define GET_UINT32(x)		(*(const uint32_t*)(x))
	#define SET_UINT16(x,val)	(*(uint16_t*)(x) = (uint16_t)(val))
	#define SET_UINT32(x,val)	(*(uint32_t*)(x) = (uint32_t)(val))
#endif

// Determine the number of bits used by pointers
#ifndef PNTR_BITS
	#if SIZE_MAX == UINT64_MAX
		#define PNTR_BITS 64
	#elif SIZE_MAX == UINT32_MAX
		#define PNTR_BITS 32
	#elif SIZE_MAX == UINT16_MAX
		#define PNTR_BITS 16
	#else
		#error You must define PNTR_BITS to be the number of bits used for pointers
	#endif
#endif

// Get ROTL function
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64))
	#include <stdlib.h>
	#pragma intrinsic(_rotl)
	#define ROTL(x, bits) _rotl(x, bits)
#else
	#define ROTL(x, bits) (((x) << (bits)) | ((x) >> (sizeof(x) - (bits))))
#endif


// Error message system (mainly for debugging)
//#ifdef _DEBUG
	#define PRINT_WARNINGS
	#define PRINT_ERRORS
//#endif

#if defined(PRINT_WARNINGS) || defined(PRINT_ERRORS)
	#include <stdio.h>
#endif

#ifdef PRINT_WARNINGS
	#define PRINT_WARNING(...)	fprintf(stderr, __VA_ARGS__)
#else
	#define PRINT_WARNING(...)
#endif

#ifdef PRINT_ERRORS
	#define PRINT_ERROR(...)	fprintf(stderr, __VA_ARGS__)
#else
	#define PRINT_ERROR(...)
#endif

// Define types used
typedef uint8_t byte;
typedef byte* bytes;
typedef const byte const_byte;
typedef const_byte* const_bytes;

// If compiling as C than make sure we have access to some C++ keywords
#ifndef __cplusplus
	typedef uint_fast8_t bool;
	#define true  1
	#define false 0
	#define inline __inline
#endif
