// -*- C++ -*-

/*
 
  Copyright (c) 2007-2012 Emery Berger, University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef _XDEFINES_H_
#define _XDEFINES_H_

#include <sys/mman.h>
#include <sys/types.h>
#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ucontext.h>
#include <pthread.h>
#include <new>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libfuncs.h"
#include "log.h"

/*
 * @file   xdefines.h   
 * @brief  Global definitions for Sheriff-Detect and Sheriff-Protect.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

extern "C"
{
  typedef void * threadFunction (void *);
  
  enum { CACHE_LINE_SIZE = 64 };
  enum { CACHE_LINE_SIZE_SHIFTS = 6 };
  enum { CACHE_LINE_SIZE_MASK = (CACHE_LINE_SIZE-1)};
  
  enum { PREDICTION_LARGE_CACHE_LINE_SIZE = 128 };
	#define LOG_SIZE 4096

  typedef enum e_access_type {
    E_ACCESS_READ = 0,
    E_ACCESS_WRITE
  } eAccessType;

  typedef struct thread {
    bool      available; // True: the thread index is free.
    int       index;
//    pid_t     tid; // Current process id of this thread.
    pthread_t self; // Results of pthread_self
		char outputBuf[LOG_SIZE];	

    // The following is the parameter about starting function. 
    threadFunction * startRoutine;
    void * startArg; 

    // We used this to record the stack range
    void * stackBottom;
    void * stackTop;
  } thread_t;

  extern unsigned long textStart, textEnd;
  extern unsigned long globalStart, globalEnd;
  extern unsigned long heapStart, heapEnd;
  extern __thread thread_t * current;
  // Whether current thread is inside backtrace phase
  // If yes, then we do not need to get backtrace for current malloc.
  extern __thread bool isBacktrace; 
  extern bool initialized;
  extern bool isMultithreading;

	// inline char getThreadBuffer()
	inline char * getThreadBuffer() {
		return current->outputBuf;
	}

  // Get thread index
  inline int getThreadIndex(void) {
    return current->index;
  }

  enum { USER_HEAP_BASE     = 0x40000000 }; // 1G
  enum { USER_HEAP_SIZE = 1048576UL * 8192 }; // 8G
  enum { MAX_USER_SPACE     = USER_HEAP_BASE + USER_HEAP_SIZE };
  enum { INTERNAL_HEAP_BASE = 0x100000000000 };
  enum { CACHE_STATUS_BASE   = 0x200000000000 };
  enum { CACHE_TRACKING_BASE = 0x200080000000 };
  enum { MEMALIGN_MAGIC_WORD = 0xCFBECFBECFBECFBE };
  enum { CALL_SITE_DEPTH = 2 };

  inline size_t alignup(size_t size, size_t alignto) {
    return ((size + (alignto - 1)) & ~(alignto -1));
  }
  
  inline size_t aligndown(size_t addr, size_t alignto) {
    return (addr & ~(alignto -1));
  }
  
  // It is easy to get the 
  inline size_t getCacheStart(void * addr) {
    return aligndown((size_t)addr, CACHE_LINE_SIZE);
  }

  //inline unsigned long * getCachelineStatus(void * addr) {
  inline unsigned long getCacheline(size_t offset) {
    return offset/CACHE_LINE_SIZE;
  }

  inline unsigned long getCachelines(size_t offset) {
    return offset/CACHE_LINE_SIZE;
  }
  
  // Caculate how many cache lines are occupied by specified address and size.
  inline int getCoveredCachelines(unsigned long start, unsigned long end) {
    size_t size = end - start;
    int lines = getCachelines(size);
    return end%CACHE_LINE_SIZE == 0 ?  lines : lines+1;
  }
  
  inline unsigned long getMin(unsigned long a, unsigned long b) {
    return (a < b ? a : b);
  }
  
  inline unsigned long getMax(unsigned long a, unsigned long b) {
    return (a > b ? a : b);
  }
};

class xdefines {
public:
  enum { STACK_SIZE = 1024 * 1024 };
  enum { PHEAP_CHUNK = 1048576 };
#ifdef CENTRALIZED_BIG_HEAP 
	// Depends
  enum { BIG_HEAP_CHUNK_SIZE = PHEAP_CHUNK };
  enum { BIG_HEAP_SIZE_CLASSES = 10 };
  //enum { PHEAP_CHUNK = 1048576 * 32 };
#endif

  enum { INTERNALHEAP_SIZE = 1048576UL * 1024 };
  enum { PAGE_SIZE = 4096UL };
  enum { PageSize = 4096UL };
  enum { PAGE_SIZE_MASK = (PAGE_SIZE-1) };

  enum { MAX_THREADS = 1024 };
  enum { NUM_HEAPS = MAX_THREADS };
  
  // 2^6 = 64
  // Since the "int" is most common word, we track reads/writes based on "int"
  enum { WORD_SIZE = sizeof(int) };
  enum { WORDS_PER_CACHE_LINE = CACHE_LINE_SIZE/WORD_SIZE };

  enum { ADDRESS_ALIGNMENT = sizeof(void *) };

  // sizeof(unsigned long) = 8;
  enum { ADDRESS_ALIGNED_BITS = 0xFFFFFFFFFFFFFFF8 };

  // We start to track all accceses only when writes is larger than this threshold.
  // If not, then we only need to track writes. 
  enum { THRESHOLD_TRACK_DETAILS = 10000 };
 
  // We should guarantee that sampling period should cover the prediction phase.
  enum { SAMPLE_ACCESSES_EACH_INTERVAL = THRESHOLD_TRACK_DETAILS};
  //enum { SAMPLE_INTERVAL = SAMPLE_ACCESSES_EACH_INTERVAL * 5 };
  //enum { SAMPLE_INTERVAL = SAMPLE_ACCESSES_EACH_INTERVAL * 100 };
  enum { SAMPLE_INTERVAL = SAMPLE_ACCESSES_EACH_INTERVAL * 100 };

  // Now we can start to check potential false sharing 
  enum { THRESHOLD_PREDICT_FALSE_SHARING = THRESHOLD_TRACK_DETAILS * 2 };
  enum { THRESHOLD_HOT_ACCESSES = THRESHOLD_TRACK_DETAILS/WORDS_PER_CACHE_LINE };

  // We only care about those words whose access is larger than the average number under 
  // the tracking period 
  enum { THRESHOLD_AVERAGE_ACCESSES = THRESHOLD_TRACK_DETAILS >> 3 };
  

  enum { THRESHOLD_REPORT_INVALIDATIONS = 1 };
  //enum { THRESHOLD_REPORT_INVALIDATIONS = THRESHOLD_HOT_ACCESSES };
};

#endif
