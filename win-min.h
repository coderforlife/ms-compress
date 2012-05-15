#pragma once

////////// Minimal Windows definitions needed, no need to #include <windows.h> //////////

#ifdef _WIN32

#ifndef _WINDOWS_ // make sure the real <windows.h> was not already included

///// Dummy Annotations /////
#ifndef __ATTR_SAL
#define __in
#define __in_bcount(x)
#define __out
#define __out_bcount(x)
#define __out_bcount_part(x, y)
#define __inout
#define __inout_bcount(x)
#define __out_opt
#define __success(x)
#define __nullterminated
#endif
#ifndef __in_range
#define __in_range(x, y)
#endif
#ifndef __drv_maxIRQL
#define __drv_maxIRQL(x)
#endif


///// Basic definitions (from <WinNT.h> and <WinDef.h> /////
#ifndef ANYSIZE_ARRAY
#define ANYSIZE_ARRAY 1
#endif

#ifndef BASETYPES
#define BASETYPES
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef char *PSZ;
#endif  /* !BASETYPES */

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_IA64) || defined(_M_AMD64)) && !defined(MIDL_PASS)
#define DECLSPEC_IMPORT __declspec(dllimport)
#else
#define DECLSPEC_IMPORT
#endif

typedef void *PVOID;
typedef void *HMODULE;

#ifndef NTSYSAPI
#define NTSYSAPI     DECLSPEC_IMPORT
#define NTSYSCALLAPI DECLSPEC_IMPORT
#endif

#ifndef WINBASEAPI
#define WINBASEAPI DECLSPEC_IMPORT
#endif

#ifndef VOID
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#if !defined(MIDL_PASS)
typedef int INT;
#endif
#endif
typedef wchar_t WCHAR;    // wc,   16-bit UNICODE character
#ifndef CONST
#define CONST               const
#endif

typedef __nullterminated CONST CHAR *LPCSTR, *PCSTR;
typedef __nullterminated CONST WCHAR *LPCWSTR, *PCWSTR;

#define FAR
#define WINAPI __stdcall

#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define _W64 __w64
#else
#define _W64
#endif
#endif

#if (501 < __midl)
	typedef [public] __int3264 INT_PTR, *PINT_PTR;
#else  // midl64
// old midl and C++ compiler
#if defined(_WIN64)
	typedef __int64 INT_PTR, *PINT_PTR;
#else
	typedef _W64 int INT_PTR, *PINT_PTR;
#endif
#endif // midl64

#ifndef _MANAGED
#ifdef _WIN64
typedef INT_PTR (FAR WINAPI *FARPROC)();
#else
typedef int (FAR WINAPI *FARPROC)();
#endif  // _WIN64
#else
typedef INT_PTR (WINAPI *FARPROC)(void);
#endif


///// From <WinBase.h> /////
#ifdef __cplusplus
extern "C" {
#endif
WINBASEAPI __out_opt HMODULE WINAPI LoadLibraryA(__in LPCSTR lpLibFileName);
WINBASEAPI __out_opt HMODULE WINAPI LoadLibraryW(__in LPCWSTR lpLibFileName);
WINBASEAPI FARPROC WINAPI GetProcAddress(__in HMODULE hModule, __in LPCSTR lpProcName);
#ifdef __cplusplus
}
#endif

#endif

#endif
