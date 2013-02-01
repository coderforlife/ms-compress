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

#ifndef COMPRESSION_API_H
#define COMPRESSION_API_H

#ifdef COMPRESSION_API_EXPORT

// For MSVC
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

// For GCC
#define __STDC_LIMIT_MACROS

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

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
	#pragma intrinsic(_rotl)
	#define ROTL(x, bits) _rotl(x, bits)
#else
	#define ROTL(x, bits) (((x) << (bits)) | ((x) >> (sizeof(x) - (bits))))
#endif

// Get ARRAYSIZE
#ifndef ARRAYSIZE
	#define ARRAYSIZE(x) sizeof(x)/sizeof(x[0])
#endif

// Compile it right
#if defined(__cplusplus_cli)
#pragma unmanaged
#endif
#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

// Warning disable support
#if defined(_MSC_VER)
#define WARNINGS_PUSH() __pragma(warning(push))
#define WARNINGS_POP()  __pragma(warning(pop))
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         __pragma(warning(disable:4127))
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                __pragma(warning(disable:4309))
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() __pragma(warning(disable:4512))
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   __pragma(warning(disable:4701))
#define WARNINGS_IGNORE_DIV_BY_0()				            __pragma(warning(disable:4723 4724))
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#define WARNINGS_PUSH() _Pragma("GCC diagnostic push")
#define WARNINGS_POP()  _Pragma("GCC diagnostic pop")
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                _Pragma("GCC diagnostic ignored \"-Woverflow\"")
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() 
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define WARNINGS_IGNORE_DIV_BY_0()                          _Pragma("GCC diagnostic ignored \"-Wdiv-by-zero\"")
#else
#define WARNINGS_PUSH() 
#define WARNINGS_POP()  
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() 
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   
#define WARNINGS_IGNORE_DIV_BY_0()                          
#endif

// Compile-time assert
#ifdef _DEBUG
#define CASSERT(expr)		char _UNIQUE_NAME[expr]
#define _UNIQUE_NAME		_MAKE_NAME(__LINE__)
#define _MAKE_NAME(line)	_MAKE_NAME2(line)
#define _MAKE_NAME2(line)	cassert_##line
#else
#define CASSERT(expr)
#endif

// Error message system (mainly for debugging)
#ifdef _DEBUG
	//#define PRINT_WARNINGS
	//#define PRINT_ERRORS
#endif

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

// DLL linkage
#ifdef COMPRESSION_API_DLL
	#ifdef _WIN32
		#define COMPAPI __declspec(dllexport)
	#else
		#define COMPAPI extern // unnecessary but whatever
	#endif
#else
	#define COMPAPI
#endif


#else // Importing from DLL or LIB

#include <stdint.h>
#ifdef COMPRESSION_API_DLL
	#ifdef _WIN32
		#define COMPAPI __declspec(dllexport)
	#else
		#define COMPAPI
	#endif
#else
	#define COMPAPI
#endif

#endif

#ifndef __cplusplus
	#define EXTERN_C_START
	#define EXTERN_C_END
#else
	#define EXTERN_C_START extern "C" {
	#define EXTERN_C_END   }
#endif

// Custom errno values
#define E_ILLEGAL_FORMAT		0x101
#define E_INSUFFICIENT_BUFFER	0x102
#define E_INVALID_DATA			0x103

// Define types used
typedef uint8_t byte; // should always be unsigned char (there is a check for CHAR_BIT == 8 above)
typedef byte* bytes;
typedef const byte const_byte;
typedef const_byte* const_bytes;

#endif
