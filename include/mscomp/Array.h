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


/////////////////// Array /////////////////////////////////////////////////////
// A simple array class that uses templates to determine if it is stack or heap
// allocated.

// All of the std::array functions could be added to these, but really they
// aren't necessary for this simple usage. Also, some explicit cast operators
// would be nice...

#ifndef MSCOMP_XPRESS_ARRAY_H
#define MSCOMP_XPRESS_ARRAY_H
#include "internal.h"

template<typename T, size_t N, bool UseStack = false>
class Array { private: Array(); };


template<typename T, size_t N>
class Array<T, N, false>
{
	// Heap-allocated version
	T* x;
public:
	FORCE_INLINE Array() : x((T*)malloc(N*sizeof(T))) { }
	FORCE_INLINE ~Array() { free(x); }
	FORCE_INLINE T& operator[] (size_t i) { return this->x[i]; }
	FORCE_INLINE const T& operator[] (size_t i) const { return this->x[i]; }
	FORCE_INLINE T* data() { return this->x; }
	FORCE_INLINE const T* data() const { return this->x; }
};

template<typename T, size_t N>
class Array<T, N, true>
{
	// Stack-allocated version (no constructor or destructor)
	T x[N];
public:
	FORCE_INLINE T& operator[] (size_t i) { return this->x[i]; }
	FORCE_INLINE const T& operator[] (size_t i) const { return this->x[i]; }
	FORCE_INLINE T* data() { return this->x; }
	FORCE_INLINE const T* data() const { return this->x; }
};

#endif