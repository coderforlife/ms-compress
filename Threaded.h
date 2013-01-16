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


////////////////////////////// Threads /////////////////////////////////////////////////////////////
// A simple cross-platform threaded system. Supports WIN32 and POSIX threads.
// It runs multiple threads while only varying their id.

#ifndef THREADED_H
#define THREADED_H
#include "compression-api.h"

#ifndef DISABLE_THREADED

#if defined(_WIN32)
#undef ARRAYSIZE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(PTHREADS)
#include <pthread.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <time.h>
#include <sched.h>
#else
#error No threading support for your system. Make sure to compile with -DDISABLE_THREADED
#endif

class Threaded
{
private:
	struct Param
	{
		Threaded *threaded;
		unsigned long id;
	};

#if defined(_WIN32)
	typedef HANDLE lock_t;
	typedef HANDLE thread_t;
	friend DWORD WINAPI thread_start(LPVOID param);
#elif defined(PTHREADS)
	typedef pthread_mutex_t lock_t;
	typedef pthread_t thread_t;
	friend void *thread_start(void *param);
#endif
	lock_t _lock;
	unsigned long _threads_count;
	unsigned long _running_count;
	thread_t *_threads;
	bool *_running;
	Param *_params;
	bool _aborted, _failed;
	size_t _stack_size; // 0 = default, any other value sets that value

	static bool _enabled;
	static long _numLogicalProcs;

	void exit(unsigned long id, unsigned int exit_code);

protected:
	Threaded();
	~Threaded();

	bool lock();
	void unlock();
	
	virtual void run(unsigned long id) = 0;

public:
	bool start(unsigned long count);
	bool wait();

	void abort();
	bool aborted();
	bool running();
	bool initFailure();

	static bool enabled();
	static void setEnabled(bool enabled);
	static long idealThreadCount();

	static void sleep(unsigned long msecs);
	static void yieldCurrentThread();
};

#else

class Threaded
{
private:
protected:
public:
	static bool isEnabled() { return false; }
	static void setEnabled(bool enabled) { UNREFERENCED_PARAMETER(enabled); }
	static long idealThreadCount() { return 1; }
};

#endif
#endif