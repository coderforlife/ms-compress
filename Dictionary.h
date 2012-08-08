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


/////////////////// Dictionary /////////////////////////////////////////////////
// The dictionary system used for LZNT1 and XPRESS compression.
//
// Most of the compression time is spent in the dictionary - particularly Find and Add.
//
// The compressor does not care about the format of the dictionary struct, it is
// completely agnostic to it and any of the function implementations.


#ifndef DICTIONARY_H
#define DICTIONARY_H
#include "compression-api.h"

EXTERN_C_START

struct _Dictionary;
typedef struct _Dictionary Dictionary;

// Creates and returns an uninitialized dictionary struct
// Returns NULL on error (and sets errno)
Dictionary* Dictionary_Create();

// Destroys a dictionary struct
void Dictionary_Destroy(Dictionary* d);

// Resets a dictionary struct ready to start a new chunk
// This should also be called after Dictionary_Create and before any Dictionary_Add/Dictionary_Find
// Returns true on success, false on error (and sets errno)
bool Dictionary_Reset(Dictionary* d);

// Adds data to the dictionary, which will be used as a starting point during future finds
// Max length is how many bytes can be read from data, regardless of the end of the chunk
// Returns true on success, false on error
bool Dictionary_Add(Dictionary* d, const_bytes data, const size_t max_len);

// Finds the best symbol in the dictionary(ies) for the data
// The second dictionary may be NULL for independent chunks, or the dictionary for the previous chunk is overlap can occur
// Returns the length of the string found, or 0 if nothing of length >= 3 was found
// offset is set to the offset from the current position to the string
uint_fast16_t Dictionary_Find(const Dictionary* d, const Dictionary* d2, const_bytes data, const uint_fast16_t max_len, const_bytes search, uint_fast16_t* offset);

EXTERN_C_END

#endif
