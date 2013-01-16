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

#include "Threaded.h"
#include <string.h>

#ifndef DISABLE_THREADED

#if defined(_WIN32)
DWORD WINAPI thread_start(LPVOID param)
#elif defined(PTHREADS)
void *thread_start(void *param)
#endif
{
	Threaded::Param *data = (Threaded::Param*)param;
	data->threaded->run(data->id);
	data->threaded->exit(data->id, 0);
	return 0;
}

template<typename T> inline static T* alloc(unsigned long count) { size_t sz = sizeof(T) * count; return (T*)memset(malloc(sz), 0, sz); }

// Start many threads 
bool Threaded::start(unsigned long count)
{
	if (!this->lock()) { return false; }

	// Make sure we aren't trying to start while still running
	if (this->_running)
	{
		this->unlock();
		return false;
	}

	// Setup basic variables
	bool success = true;
	this->_aborted = false;
	this->_threads_count = this->_running_count = count;
	this->_threads = alloc<thread_t>(count);
	this->_running = alloc<bool>    (count);
	this->_params  = alloc<Param>   (count);
	if (this->_threads && this->_running && this->_params)
	{
		// Start each thread
#if defined(_WIN32)
		for (unsigned long i = 0; i < count; ++i)
		{
			this->_params[i].threaded = this;
			this->_params[i].id = i;
			HANDLE hThread = CreateThread(NULL, this->_stack_size, thread_start, this->_params+i, 0, NULL);
			if (hThread == NULL) { count = i; success = false; break; }
			this->_threads[i] = hThread;
			this->_running[i] = true;
		}
#elif defined(PTHREADS)
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0 || (this->_stack_size != 0 && pthread_attr_setstacksize(&attr, this->_stack_size) != 0)) { count = 0; success = false; } // ENOMEM / EINVAL
		else
		{
			for (unsigned long i = 0; i < count; ++i)
			{
				this->_params[i].threaded = this;
				this->_params[i].id = i;
				pthread_t pThread;
				if (pthread_create(&pThread, &attr, &thread_start, this->_params+i) != 0) { count = i; success = false; break; } // EAGAIN
				this->_threads[i] = pThread;
				this->_running[i] = true;
			}
		}
		pthread_attr_destroy(&attr);
#endif
	}
	else { count = 0; success = false; }

	if (!success)
	{
		if (count > 0)
		{
			// Make sure all threads that did start are aborted
			this->_threads_count = count;
			this->unlock(); // make sure we aren't locked while waiting
			this->abort();
			this->wait();
			this->lock();
#if defined(_WIN32)
			for (unsigned long i = 0; i < count; ++i) { CloseHandle(this->_threads[i]); }
#endif
		}

		// Reset many variables
		this->_threads_count = 0;
		this->_running_count = 0;
		free(this->_params);
		free(this->_running);
		free(this->_threads);
		this->_params = NULL;
		this->_running = NULL;
		this->_threads = NULL;
	}

	this->unlock();

	return success;
}

// Exit from a thread
void Threaded::exit(unsigned long id, unsigned int exit_code)
{
	this->lock();
	--this->_running_count;
	this->_running[id] = false;
	if (this->_running_count == 0)
	{
#if defined(_WIN32)
		for (unsigned long i = 0; i < this->_threads_count; ++i) { CloseHandle(this->_threads[i]); }
#endif
		this->_threads_count = 0;
		free(this->_params);
		free(this->_running);
		free(this->_threads);
		this->_params = NULL;
		this->_running = NULL;
		this->_threads = NULL;
	}
	this->unlock();
#if defined(_WIN32)
	UNREFERENCED_PARAMETER(exit_code);
	//ExitThread(exit_code); // better handled by returning from function
#elif defined(PTHREADS)
	pthread_exit((void*)exit_code);
#endif
}

// Wait until all threads have completed
bool Threaded::wait()
{
	// Check that there are threads to be waited on
	this->lock();
	bool has_threads = this->_threads_count > 0;
	this->unlock();
	if (has_threads)
	{
#if defined(_WIN32)
		if (WaitForMultipleObjects(this->_threads_count, this->_threads, TRUE, INFINITE) == WAIT_FAILED) { return false; }
#elif defined(PTHREADS)
		for (unsigned long i = 0; i < this->_threads_count; ++i)
		{
			if (this->_running[i] && pthread_join(this->_threads[i], NULL) != 0) { return false; } // EDEADLK - A deadlock was detected or the value of thread specifies the calling thread.
		}
#endif
	}
	return true;
}

// Check if the constructor failed
bool Threaded::initFailure() { return this->_failed; }

// Initialize the threaded system
Threaded::Threaded() : _threads_count(0), _running_count(0), _threads(NULL), _running(NULL), _params(NULL), _aborted(false), _failed(false), _stack_size(0)
{
#if defined(_WIN32)
	this->_lock = CreateMutex(NULL, FALSE, NULL);
	if (!this->_lock)  { this->_failed = true; }
#elif defined(PTHREADS)
	if (pthread_mutex_init(&this->_lock, NULL) != 0) { this->_failed = true; } // EAGAIN or ENOMEM
#endif
}

// Cleanup the Threaded class, freeing all allocated resources
Threaded::~Threaded()
{
	// Make sure all threads are down
	if (this->_running_count)
	{
		this->unlock();
		this->abort();
		this->wait();
	}

	// Close the locking handle (ignoring errors)
	// Make sure the lock handle is ours then unlock and close it
#if defined(_WIN32)
	WaitForSingleObject(this->_lock, INFINITE);
	ReleaseMutex(this->_lock);
	CloseHandle(this->_lock);
#elif defined(PTHREADS)
	pthread_mutex_lock(&this->_lock);
	pthread_mutex_unlock(&this->_lock);
	pthread_mutex_destroy(&this->_lock);
#endif

	// Free memory
	free(this->_params);
	free(this->_running);
	free(this->_threads);
}

// Acquire the lock for this threaded object
// Any resources shared among the threads and the thread-management resources themselves should all be locked before-hand
bool Threaded::lock()
{
#if defined(_WIN32)
	DWORD retval = WaitForSingleObject(this->_lock, INFINITE);
	if (retval == WAIT_FAILED) { return false; }
	else if (retval == WAIT_ABANDONED) // thread previously holding mutex was terminated - bad stuff is going down
	{
		ReleaseMutex(this->_lock);
		this->abort();
		return false;
	}
#elif defined(PTHREADS)
	if (pthread_mutex_lock(&this->_lock) != 0) { return false; } // EDEADLK - The current thread already owns the mutex
#endif
	return true;
}

// Release the lock for this threaded object
// Allows other threads to access shared resources
void Threaded::unlock()
{
	// Ignore errors (usually the result of not being the thread that owns the lock)
#if defined(_WIN32)
	ReleaseMutex(this->_lock);
#elif defined(PTHREADS)
	pthread_mutex_unlock(&this->_lock);
#endif
}

// Check if any threads are running
bool Threaded::running() { return this->_running_count > 0; }

// Abort the threads. The should threads check the status every so often so aborting does not take effect immediately.
void Threaded::abort() { this->_aborted = true; }
bool Threaded::aborted() { return this->_aborted; }

// Get and set whether or not threading is used during compression / decompression
bool Threaded::_enabled = false;
bool Threaded::enabled() { return Threaded::_enabled; }
void Threaded::setEnabled(bool enabled) { Threaded::_enabled = enabled; }

// Get the number of ideal threads (includes hyperthreads)
long Threaded::_numLogicalProcs = -1;
long Threaded::idealThreadCount()
{
	if (Threaded::_numLogicalProcs == -1)
	{
#if defined(_WIN32)
		// Windows
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		Threaded::_numLogicalProcs = (long)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
		// Linux, Solaris, AIX, and Mac OS X >= 10.4 (Tiger onwards)
		Threaded::_numLogicalProcs = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
		// IRIX
		Threaded::_numLogicalProcs = sysconf(_SC_NPROC_ONLN);
#elif defined(MPC_GETNUMSPUS)
		// HPUX
		Threaded::_numLogicalProcs = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#elif defined(CTL_HW)
		// FreeBSD, MacOS X, NetBSD, OpenBSD, etc
		int mib[4] = { CTL_HW, HW_AVAILCPU, 0, 0 };
		size_t len = sizeof(Threaded::_numLogicalProcs); 
		sysctl(mib, 2, &Threaded::_numLogicalProcs, &len, NULL, 0);
		if (Threaded::_numLogicalProcs < 1)
		{
			 mib[1] = HW_NCPU;
			 sysctl(mib, 2, &Threaded::_numLogicalProcs, &len, NULL, 0);
		}
#else
		// Unsure how to get this information
		#warning Unable to determine method of finding number of processors
		#warning Threaded::idealThreadCount() will always return 0
		#warning This may negatively impact parallel performance
#endif
		if (Threaded::_numLogicalProcs < 1) { Threaded::_numLogicalProcs = 0; }
	}
	return Threaded::_numLogicalProcs;
}

// Sleep for the given number of milliseconds
void Threaded::sleep(unsigned long msecs)
{
#if defined(_WIN32)
	Sleep(msecs);
#elif defined(PTHREADS)
	struct timespec t, t2;
	t.tv_sec = msecs / 1000;
	t.tv_nsec = (msecs % 1000) * 1000 * 1000;
	nanosleep(&t, &t2);
#endif
}

// Yields current thread execution to another thread
// Exact behavior is OS-dependent
void Threaded::yieldCurrentThread()
{
#if defined(_WIN32)
	SwitchToThread();
#elif defined(PTHREADS)
	sched_yield();
#endif
}

#endif
