// ms-compress-test: implements Microsoft compression algorithms
// Copyright (C) 2012  Jeffrey Bush  jeff@coderforlife.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include "stdafx.h"

#include "compression.h"
#include "win-compression.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
#endif

#define COMPRESSION_REPEAT		for (i = 0; i < 10; ++i)
#define DECOMPRESSION_REPEAT	//for (i = 0; i < 100; ++i)

static bool write_all(const wchar_t* file, bytes a, size_t len)
{
	size_t written;
	FILE* f = _wfopen(file, L"wb");
	if (f == NULL) return false;
	written = fwrite(a, 1, len, f);
	fclose(f);
	return written == len;
}
static bytes read_all(const wchar_t* file, size_t* length, size_t max_len)
{
	size_t len;
	bytes a;
	FILE* f = _wfopen(file, L"rb");
	if (f == NULL) return false;
	fseek(f, 0L, SEEK_END);
	len = ftell(f); // NOTE: ftell only gives long
	rewind(f);
	if (len > max_len)
		len = max_len;
	a = (bytes)malloc(len);
	if (a)
	{
		*length = len;
		if (fread(a, 1, len, f) != len)
		{
			free(a);
			a = NULL;
		}
	}
	fclose(f);
	return a;
}

static void fill_uncompressible(bytes a, size_t len) // optimal 4444 bytes long, then it starts to repeat
{
	size_t pos = 0;
	byte i, inc;
	while (pos < len)
	{
		for (i = 0x00; i <= 0xFF && pos < len; ++i) // inc = 0.5
		{
			a[pos++] = i;
			a[pos++] = i;
		}
		for (inc = 0x01; inc < 0xFF && pos < len; ++inc)
			for (i = 0x00; i <= 0xFF && pos < len; i += inc)
				a[pos++] = i;
		for (i = 0xFF; i >= 0x00 && pos < len; --i) // inc = 0.5
		{
			a[pos++] = i;
			a[pos++] = i;
		}
		for (inc = 0x01; inc < 0xFF && pos < len; ++inc)
			for (i = 0xFF; i >= 0x00 && pos < len; i -= inc)
				a[pos++] = i;
	}
}
static void fill_random(bytes a, size_t len)
{
	size_t i;
	srand((unsigned int)time(NULL));
	for (i = 0; i < len; ++i)
		a[i] = (byte)(rand() % 0x100);
}
static void fill_random_text(bytes a, size_t len)
{
	size_t i;
	srand((unsigned int)time(NULL));
	for (i = 0; i < len; ++i)
	{
		byte r;
		do { r = (byte)(rand() & 0xFF); } while ((r < ' ' && r != '\n') || r >= '~');
		a[i] = r;
	}
}

static bool check_equality(const_bytes a, const_bytes b, size_t len)
{
	size_t i;
	for (i = 0; i < len && a[i] == b[i]; ++i);
	if (i != len)
		wprintf(L"Unequal at %p: %02X != %02X\n", i, (unsigned int)a[i], (unsigned int)b[i]);
	return i == len;
}

static bool load_rtl_compression()
{
#ifdef _WIN64
	HMODULE ntdll = LoadLibraryW(L"ntdll-8-64.dll");
#else
	HMODULE ntdll = LoadLibraryW(L"ntdll-8-32.dll");
#endif
	return LOAD_FUNC(RtlDecompressBufferEx, ntdll) != NULL && LOAD_FUNC(RtlCompressBuffer, ntdll) != NULL && LOAD_FUNC(RtlGetCompressionWorkSpaceSize, ntdll) != NULL;
}
static void* alloc_rtl_workspace(USHORT format)
{
	ULONG CompressBufferWorkSpaceSize = 0, CompressFragmentWorkSpaceSize = 0, CompressBufferWorkSpaceSize2 = 0, CompressFragmentWorkSpaceSize2 = 0;
	if ((RtlGetCompressionWorkSpaceSize(format | COMPRESSION_ENGINE_STANDARD, &CompressBufferWorkSpaceSize, &CompressFragmentWorkSpaceSize) != STATUS_SUCCESS) ||
		(RtlGetCompressionWorkSpaceSize(format | COMPRESSION_ENGINE_MAXIMUM, &CompressBufferWorkSpaceSize2, &CompressFragmentWorkSpaceSize2) != STATUS_SUCCESS))
		return NULL;
	wprintf(L"Allocating workspace of MAX(MAX(%lu,%lu),MAX(%lu,%lu)) bytes\n", CompressBufferWorkSpaceSize, CompressFragmentWorkSpaceSize, CompressBufferWorkSpaceSize2, CompressFragmentWorkSpaceSize2);
	wprintf(L"================================================================\n");
	return malloc(MAX(MAX(CompressBufferWorkSpaceSize, CompressFragmentWorkSpaceSize), MAX(CompressBufferWorkSpaceSize2, CompressFragmentWorkSpaceSize2)));
}


static void compress_buf(USHORT format, bytes orig, size_t len, const wchar_t* rtl_out, const wchar_t* out, void* ws)
{
	NTSTATUS status = 0;
	clock_t start, end;
	ULONG uncomp_len, comp_len, comp2_len = 0;
	bytes uncomp = (bytes)malloc(len*4);
	bytes comp = (bytes)malloc(len*4);
	bytes comp2 = (bytes)malloc(len*4);
	uint_fast16_t i;
	
	CompressionFormat format2 = (CompressionFormat)format;
	/*switch (format)
	{
	case COMPRESSION_FORMAT_LZNT1:		format2 = COMPRESSION_LZNT1; break;
	case COMPRESSION_FORMAT_XPRESS:		format2 = COMPRESSION_XPRESS; break;
	case COMPRESSION_FORMAT_XPRESS_HUFF:format2 = COMPRESSION_XPRESS_HUFF; break;
	}*/
	
	start = clock();
	comp_len = 0;
	COMPRESSION_REPEAT
	status = RtlCompressBuffer(format | COMPRESSION_ENGINE_STANDARD, orig, len, comp, len*4, 4096, &comp_len, ws);
	end = clock();
	wprintf(L"rtl-compress-std: %Iu bytes\tin %u ms [%lX]\n", comp_len, end - start, status);
	write_all(rtl_out, comp, comp_len);

	start = clock();
	uncomp_len = 0;
	DECOMPRESSION_REPEAT
	status = RtlDecompressBufferEx(format, uncomp, len*4, comp, comp_len, &uncomp_len, ws);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"rtl-decompress:   %Iu bytes\tin %u ms [%lX]\n", uncomp_len, end - start, status);
	
	memset(uncomp, 0xFF, len*4);
	start = clock();
	DECOMPRESSION_REPEAT
	uncomp_len = decompress(format2, comp, comp_len, uncomp, len*4);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"decompress:       %Iu bytes\tin %u ms\n", uncomp_len, end - start);

	wprintf(L"----------------------------------------------------------------\n");
	
	start = clock();
	comp_len = 0;
	COMPRESSION_REPEAT
	status = RtlCompressBuffer(format | COMPRESSION_ENGINE_MAXIMUM, orig, len, comp, len*4, 4096, &comp_len, ws);
	end = clock();
	wprintf(L"rtl-compress-max: %Iu bytes\tin %u ms [%lX]\n", comp_len, end - start, status);
	write_all(rtl_out, comp, comp_len);

	start = clock();
	uncomp_len = 0;
	DECOMPRESSION_REPEAT
	status = RtlDecompressBufferEx(format, uncomp, len*4, comp, comp_len, &uncomp_len, ws);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"rtl-decompress:   %Iu bytes\tin %u ms [%lX]\n", uncomp_len, end - start, status);
	
	memset(uncomp, 0xFF, len*4);
	start = clock();
	DECOMPRESSION_REPEAT
	uncomp_len = decompress(format2, comp, comp_len, uncomp, len*4);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"decompress:       %Iu bytes\tin %u ms\n", uncomp_len, end - start);

	wprintf(L"----------------------------------------------------------------\n");

	start = clock();
	COMPRESSION_REPEAT
	comp2_len = compress(format2, orig, len, comp2, len*4);
	end = clock();
	check_equality(comp, comp2, MIN(comp_len, comp2_len));
	wprintf(L"compress:         %Iu bytes\tin %u ms (%Iu bytes %s)\n", comp2_len, end - start, (comp2_len>comp_len?comp2_len-comp_len:comp_len-comp2_len), (comp2_len>comp_len?L"worse":L"better"));
	write_all(out, comp2, comp2_len);
	
	start = clock();
	uncomp_len = 0;
	DECOMPRESSION_REPEAT
	status = RtlDecompressBufferEx(format, uncomp, len*4, comp2, comp2_len, &uncomp_len, ws);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"rtl-decompress:   %Iu bytes\tin %u ms [%lX]\n", uncomp_len, end - start, status);

	start = clock();
	DECOMPRESSION_REPEAT
	uncomp_len = decompress(format2, comp2, comp2_len, uncomp, len*4);
	end = clock();
	check_equality(orig, uncomp, uncomp_len);
	wprintf(L"decompress:       %Iu bytes\tin %u ms\n", uncomp_len, end - start);

	wprintf(L"================================================================\n");

	free(orig);
	free(uncomp);
	free(comp);
	free(comp2);
}
static void compress_file(USHORT format, const wchar_t* in, const wchar_t* rtl_out, const wchar_t* out, void* ws)
{
	size_t len = 0;
	bytes orig = read_all(in, &len, SIZE_MAX);
	wprintf(L"Read file %s, size: %Iu bytes\n", in, len);
	wprintf(L"----------------------------------------------------------------\n");
	compress_buf(format, orig, len, rtl_out, out, ws);
}

static bool run_tests(const wchar_t* name, USHORT format, const wchar_t* ext)
{
	static const wchar_t* files[] =
	{
		L"test.bmp", L"test.dic", L"test.dll", L"test.doc", L"test.exe", 
		L"test.hlp", L"test.jpg", L"test.log", L"test.pdf", L"test.txt",
	};
	wchar_t out_rtl[128], out[128];
	void *ws = NULL;
	int i, len = (int)wcslen(name), off = (62 - len) / 2 + 1;

	//_wcsnset(out, L'=', 64); // does not work, stops at first L'\0'
	for (i = 0; i < 64; ++i) { out[i] = L'='; }
	out[off-1] = L' ';
	wcsncpy(out+off, name, len);
	out[off+len] = L' ';
	out[64] = L'\0';
	wprintf(L"================================================================\n");
	wprintf(L"%s\n", out);
	wprintf(L"================================================================\n");

	if ((ws = alloc_rtl_workspace(format)) == NULL) return false;
	for (i = 0; i < ARRAYSIZE(files); ++i)
	{
		_snwprintf(out_rtl, ARRAYSIZE(out_rtl), L"%s.rtl.%s", files[i], ext);
		_snwprintf(out,     ARRAYSIZE(out),     L"%s.%s",     files[i], ext);
		compress_file(format, files[i], out_rtl, out, ws);
	}
	free(ws);
	wprintf(L"\n");
	return true;
}

int main()
{
	_wchdir(L"tests");

	//fill_uncompressible(input, sizeof(input));
	//fill_random(input, sizeof(input));
	//fill_random_text(input, sizeof(input));
	//write_all(L"uncompressible.dat", input, sizeof(input));

	if (!load_rtl_compression()) return -1;

	//if (!run_tests(L"LZNT1",          COMPRESSION_FORMAT_LZNT1,       L"lznt1"      )) return -1;
	//if (!run_tests(L"XPRESS",         COMPRESSION_FORMAT_XPRESS,      L"xpress"     )) return -1;
	if (!run_tests(L"XPRESS HUFFMAN", COMPRESSION_FORMAT_XPRESS_HUFF, L"xpress_huff")) return -1;

	//compress_file(COMPRESSION_FORMAT_XPRESS_HUFF, L"win8-bootmgr.exe", L"win8-bootmgr.rtl.xpress_huff", L"win8-bootmgr.xpress_huff", ws);

	return 0;
}
