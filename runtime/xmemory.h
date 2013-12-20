// -*- C++ -*-

/*
  Copyright (C) 2011 University of Massachusetts Amherst.

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

/*
 * @file   xmemory.h
 * @brief  Memory management for all.
 *         This file only includes a simplified logic to detect false sharing problems.
 *         It is slower but more effective.
 * 
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

#ifndef _XMEMORY_H
#define _XMEMORY_H

//#include <signal.h>

#include <sys/types.h>
#include <unistd.h>
#include <set>

#include "xglobals.h"
#include "xpheap.h"
#include "xoneheap.h"
#include "xheap.h"
#include "callsite.h"
#include "objectheader.h"
#include "finetime.h"
#include "atomic.h"

#include "xdefines.h"
#include "ansiwrapper.h"
#include "cachetrack.h"
#include "internalheap.h"
#include "spinlock.h"
#include "report.h"
#include "mm.h"


#ifdef CENTRALIZED_BIG_HEAP
#include "bigheap.h"
#endif

class xmemory {
private:

  // Private on purpose. See getInstance(), below.
  xmemory()
  {
  }

  #define CALLSITE_SIZE sizeof(CallSite)

public:

  // Just one accessor.  Why? We don't want more than one (singleton)
  // and we want access to it neatly encapsulated here, for use by the
  // signal handler.
  static xmemory& getInstance() {
    static char buf[sizeof(xmemory)];
    static xmemory * theOneTrueObject = new (buf) xmemory();
    return *theOneTrueObject;
  }

  void initialize() {
    // Intercept SEGV signals (used for trapping initial reads and
    // writes to pages).
   //fprintf(stderr, "heap initialize. heap size %lx\n", USER_HEAP_SIZE);
	
    _heap.initialize(USER_HEAP_SIZE);
    _globals.initialize();   
#ifdef CENTRALIZED_BIG_HEAP
		_bheap.initialize();
#endif
    
    initCachelineStatus();
    _heapCacheIndex = getCachelineIndex(USER_HEAP_BASE);
  }

  void finalize() {
    reportFalseSharing();
  }
 
	static void sigHandler(int signum) {
		//xmemory::getInstance().reportFalseSharing();
		//fprintf(stderr, "Recieving sigkillHandler. I am going to exit now\n");
		if(signum == SIGINT) { 
			exit(0);
		}
		else if (signum == SIGUSR2) {
			fprintf(stderr, "Recieving SIGUSR2, check false sharing now:\n");
			xmemory::getInstance().reportFalseSharing();
		}
	}
 
  // Report false sharing for current mapping
  void reportFalseSharing() {
    // Initialize the reporter in the end.
    report reporter;

    // Reporting the false sharing in the globals.
    unsigned long index = getCachelineIndex(globalStart);
    //fprintf(stderr, "globalStart %lx\n", globalStart);
    reporter.initialize(false, (void *)globalStart, (void *)globalEnd, (void *)_cacheWrites, _cacheTrackings);
    reporter.reportFalseSharing();

    void * heapEnd = _heap.getHeapPosition();
    reporter.initialize(true, (void *)USER_HEAP_BASE, heapEnd, (void *)_cacheWrites, _cacheTrackings);
    reporter.reportFalseSharing();
  }


  inline int getHeapId(void) {
    return getThreadIndex();
  }

  inline void setMultithreading(void) {
    _isMultithreading = true;
  }

  inline void storeCallsite(unsigned long start, CallSite * callsite) {
    memcpy((void *)(start - CALLSITE_SIZE), (void *)callsite, CALLSITE_SIZE);
  }

  inline void *malloc (size_t sz) {
    void * ptr = NULL;
#ifdef CENTRALIZED_BIG_HEAP
		//if(sz > xdefines::PHEAP_CHUNK && sz == 0x1ffdd) {
		if(sz >= xdefines::BIG_HEAP_CHUNK_SIZE) {
		//	fprintf(stderr, "Big heap sz %lx\n", sz); 
			ptr = _bheap.malloc(sz);
		}
#endif
		
		if(ptr == NULL) {
  //  printf("xmemory::malloc sz %lx ptr %p\n", sz, ptr);
    	ptr = _heap.malloc(getHeapId(), sz);
  	}

    // Whether current thread is inside backtrace phase
    // Get callsite information.
    CallSite callsite;
    callsite.fetch();
    //callsite.print();

    storeCallsite((unsigned long)ptr, &callsite);
    
    return (ptr);
  }

  inline void * realloc (void * ptr, size_t sz) {
    size_t s = getSize (ptr);

    void * newptr = malloc(sz);
    if(newptr && s != 0) {
      size_t copySz = (s < sz) ? s : sz;
      memcpy (newptr, ptr, copySz);
    }

    free(ptr);
    return newptr;
  }

  inline void * memalign(size_t boundary, size_t size) {
    void * ptr;
    unsigned long wordSize = sizeof(unsigned long);

    if(boundary < 8 || ((boundary & (wordSize-1)) != 0)) {
      fprintf(stderr, "memalign boundary should be larger than 8 and aligned to at least 8 bytes. Acutally %ld!!!\n", boundary);
      abort();
    }

    ptr = malloc(boundary + size);

    // Finding the word aligned with boundary.
    unsigned long start = (unsigned long)ptr + wordSize;
    unsigned long end = (unsigned long)ptr + boundary;

    while(start <= end) {
      if((start & (boundary-1)) == 0) {
        unsigned long * prev = (unsigned long *)(start - wordSize);
        *prev = MEMALIGN_MAGIC_WORD;
        ptr = (void *)start;
        break;
      }

      start += wordSize;
    }
   
    return ptr; 
  }

  inline bool isMemalignPtr(void * ptr) {
    return (*((unsigned long *)((intptr_t)ptr - sizeof(unsigned long))) == MEMALIGN_MAGIC_WORD) ? true : false;
  }

  // We only check 
  inline void * getRealStart(void * ptr) {
    unsigned long objectHeaderSize = sizeof(objectHeader);
    unsigned long * start = (unsigned long *)((intptr_t)ptr - sizeof(unsigned long)); 
    unsigned long * end = (unsigned long *)((intptr_t)ptr - xdefines::PageSize);
    void * realptr = NULL;
    objectHeader * object = NULL;

    while(start > end) {
      object = (objectHeader *)((intptr_t)start - objectHeaderSize);
      if(object->isValidObject()) {
        realptr = start; 
        break;
      }
 
      start--;
    } 
    
    return realptr;
  }

  // When a memory object is freed, this object can be used as a false sharing object. Then we may cleanup 
  // all information about the whole cache line if there is no false sharing there.
  // FIXME: if part of cache line has false sharing problem, then we may not utilize this 
  // cache line but other parts can be reused??  
  inline void free (void * ptr) {
    if(ptr == NULL) 
      return;

    void * realPtr = ptr;
    if(isMemalignPtr(ptr)) {
      realPtr = getRealStart(ptr);
    }

    size_t s = getSize(realPtr);
#ifdef CENTRALIZED_BIG_HEAP
		if(s >= xdefines::BIG_HEAP_CHUNK_SIZE) {
			if(_bheap.free(ptr, s)) {
			//	fprintf(stderr, "Big heap free thread %lx ptr %p\n",(unsigned long)pthread_self(), ptr); 
			//	fprintf(stderr, "pthread_self %lx free s %lx ptr %p\n", (unsigned long)pthread_self(), s, realPtr);
				return;
			}
		}
#endif 
    // Actually, we should check whether we need to free an object here.
    // If it includes the false sharing suspect, we should check that.
    if(!hasFSSuspect(ptr, s)) {
      _heap.free(getHeapId(), realPtr);
    }
  }

  size_t malloc_usable_size(void * ptr) {
    return getSize(ptr);
  }
  
  /// @return the allocated size of a dynamically-allocated object.
  inline size_t getSize (void * ptr) {
    // Just pass the pointer along to the heap.
    return _heap.getSize (ptr);
  }
 
  inline size_t calcCachelines(size_t size) {
    return size >> CACHE_LINE_SIZE_SHIFTS;
  }
  
  inline unsigned long getCachelineIndex(unsigned long addr) {
    return (addr >> CACHE_LINE_SIZE_SHIFTS);
  }


  /* Initialize the cache line status.
     Currently, the memory less than MAX_USER_BASE 
     will have a corresponding status and tracking status.
     This basic idea is borrow from the AddressSanitizer so that 
     we do not need to know the details of global range.
  */
  void initCachelineStatus(void) {
    size_t cachelines; // How many number of cache lines for this map
    size_t cacheStatusSize; // How much memory  of cache line status

    // We are mapping all memory under the shadow status.
    size_t size = MAX_USER_SPACE;

    // Calculate cache lines information.
    cachelines = calcCachelines(size);

    // Allocate a mapping to hold cache line status.
    size_t sizeStatus = cachelines * sizeof(unsigned long);  

    _cacheWrites = (unsigned long *)MM::mmapAllocatePrivate(sizeStatus, (void *)CACHE_STATUS_BASE); 
    _cacheTrackings = (cachetrack **)MM::mmapAllocatePrivate(sizeStatus, (void*)CACHE_TRACKING_BASE);

    assert(sizeof(unsigned long) == sizeof(cachetrack *));
  }
  
  inline unsigned long * getCacheWrites(unsigned long addr) {
    size_t index = getCachelineIndex(addr);
    return &_cacheWrites[index]; 
  }

  // Whether the specified cache line has false sharing suspect inside?
  // FIXME: currently, we are using simple mechanism when the cache line
  // has writes more than one specified threshold.
  // In the future, we should definitely check the detailed status.
  inline bool hasFSSuspect(unsigned long * status) {
    return (*status > xdefines::THRESHOLD_TRACK_DETAILS);
  }
  
  // Checking whether a object has false sharing suspects inside.
  // Currently, we only check the cacheWrites. If this object includes 
  // cache line with instensive writing, simply return yes.
  inline bool hasFSSuspect(void * addr, size_t size) {
    bool hasSuspect = false; 
    size_t lines = size/CACHE_LINE_SIZE;

    if(size % CACHE_LINE_SIZE) {
      lines++;
    }

    unsigned long * status = getCacheWrites((unsigned long)addr);
   
    for(int i = 0; i < lines; i++) {
      if(hasFSSuspect(&status[i])) {
//        printf("addr %p size %ld can have false sharing\n", addr, size);
        hasSuspect = true;
        break;
      }
    }  

    return hasSuspect;
  }

  inline cachetrack * allocCacheTrack(unsigned long addr, unsigned long totalWrites) {
    void * ptr = InternalHeap::getInstance().malloc(sizeof(cachetrack));
    cachetrack * track = new (ptr) cachetrack(addr, totalWrites);
    return track;
  }

  inline cachetrack * deallocCacheTrack(cachetrack * track) {
    InternalHeap::getInstance().free(track);
  }

  // In order to track one cache line, we should make the _cacheWrites larger than the specified threshold.
  inline void enableCachelingTracking(unsigned long index) {
    //atomic_set(&_cacheWrites[index], xdefines::THRESHOLD_TRACK_DETAILS);
    atomic_exchange(&_cacheWrites[index], xdefines::THRESHOLD_TRACK_DETAILS);
  }

  inline cachetrack * trackCacheline(unsigned long addr, unsigned long index) {
    cachetrack * track = NULL;
    // check whether this cache line is already tracked or not.
    unsigned long writes = _cacheWrites[index];

    if(writes < xdefines::THRESHOLD_TRACK_DETAILS) {
      // If not tracked, then try to allocate an 
      track = allocCacheTrack(addr, writes);
      // Set to corresponding array.
      if(!atomic_compare_and_swap((unsigned long *)&_cacheTrackings[index], 0, (unsigned long)track)) {
		//		PRERR("	_cacheTrackings[%d] is %d wrong!!!!\n", index, _cacheTrackings[index]);
        deallocCacheTrack(track);
        track = NULL;
      }
      else {
        // Promote the accesses on index.
        enableCachelingTracking(index);
      }
    }
      
    return track;
  }

  inline bool isValidObjectHeader(objectHeader * object, unsigned long * address) {
    unsigned long * objectEnd = (unsigned long *)object->getObjectEnd();
    return (object->isValidObject() && address < objectEnd);
  }

  inline unsigned long * getAlignedAddress(unsigned long address) {
    return (unsigned long *)(address & xdefines::ADDRESS_ALIGNED_BITS);
  }

  objectHeader * getHeapObjectHeader(unsigned long * address) {
    objectHeader * object = NULL;
    unsigned long * pos = getAlignedAddress((unsigned long)address);

    // We won't consider that we can not find the magic number because
    // of underflow problem
    while(true) {
      pos--;
      if(*pos == objectHeader::MAGIC) {
        object = (objectHeader *)pos; 
        // Check whether this is a valid object header.
        if(isValidObjectHeader(object, address)) {
          break;
        }
      }
    }

    return object;
  }

  inline unsigned long getCacheStartByIndex(unsigned long index) {
    return index << CACHE_LINE_SIZE_SHIFTS;
  }

  // We only track adjacent cachelines closed to current cache line.
  inline void trackAdjacentCachelines(int index) {
    if(index < _heapCacheIndex) {
      return;  
    }

    // Try to track next cache line.
    size_t cachestart = getCacheStartByIndex(index+1);
    trackCacheline(cachestart, index + 1);

    if(index != 0) {
      cachestart = getCacheStartByIndex(index-1);
      trackCacheline(cachestart, index - 1);
    }
  }

  inline cachetrack * getCacheTracking(long index) {
    return (index >= 0 ? (_cacheTrackings[index]) : NULL);
  }

  inline bool hasEvaluatedFalsesharing(unsigned long index) {
    cachetrack * track = (cachetrack *)(_cacheTrackings[index]);
    if(track != NULL) {
      if(track->hasEvaluatedFS() || track->hasFalsesharing()) {
        return true;
      }
    }
    
    return false;
  }

  // We will check adjacent cache lines very quickly.
  inline bool hasEvaluatedAdjacentCachelines(unsigned long index) {
    if(hasEvaluatedFalsesharing(index-1) || hasEvaluatedFalsesharing(index+1)) {
      return true;
    } 
    
    return false;
  }
  
  inline unsigned long getDistance(unsigned long a, int thread1, unsigned long b, int thread2) {
    unsigned long distance = CACHE_LINE_SIZE;
    if((thread1 != thread2) || (thread1 == cachetrack::WORD_THREAD_SHARED || thread2 == cachetrack::WORD_THREAD_SHARED)) {
      if(a != 0 && b != 0) {
        distance = b - a;
      }
    }
    return distance;
  }

  unsigned long getCachelineEnd(unsigned long index) {
    return ((index + 1) * CACHE_LINE_SIZE);
  }

  // Check whether two hot accesses can possibly cause potential false sharing
  bool arePotentialFSHotAccesses(struct wordinfo * word1, struct wordinfo * word2) {
    bool arePotentialFS = false;
    
    // Only one of them has hot writes
    if(word1->writes >= xdefines::THRESHOLD_HOT_ACCESSES 
      || word2->writes >= xdefines::THRESHOLD_HOT_ACCESSES) {
      if(word1->tindex != word2->tindex) {
        arePotentialFS = true;
      }
      else if(word1->tindex == cachetrack::WORD_THREAD_SHARED 
        || word2->tindex == cachetrack::WORD_THREAD_SHARED) {
        arePotentialFS = true;
      }
    }

    return arePotentialFS;
  }

  bool searchSuitableHotAccess(bool checkBackward, unsigned long index, struct wordinfo * word, unsigned long windex, unsigned long * distance) {
    bool hasFS = false;
    unsigned long otherindex = -1;
    unsigned long start = 0, end = 0;
 
    if(checkBackward) {
      otherindex = index - 1;
      end = xdefines::WORDS_PER_CACHE_LINE - 1; 
      start = windex+1;
    }
    else {
      otherindex = index + 1;
      start = 0;
      if(windex == 0) {
        end = 0;
      }
      else {
        end = windex-1;
      }
    }
    
    cachetrack * cacheTrack = getCacheTracking(otherindex);
    if(cacheTrack == NULL) {
      return hasFS;
    }

    // Acquire the wordaccess information.
    unsigned long i;
    struct wordinfo * wordAccesses = cacheTrack->getWordAccesses();
    unsigned long dist = 0;

    // Check word by word for this cache line.
    for(i = start; i <= end; i++) {
      // If we find a hot access, check against the passed word access.
      if(wordAccesses[i].writes >= xdefines::THRESHOLD_HOT_ACCESSES ||
         wordAccesses[i].reads >= xdefines::THRESHOLD_HOT_ACCESSES) {
        if(arePotentialFSHotAccesses(&wordAccesses[i], word)) {
          hasFS = true;
          if(checkBackward) {
            dist = end-i;
          }
          else {
            dist = i - start;
          }
          dist += windex;

          *distance = dist * xdefines::WORD_SIZE;
          break;
        }
      }
      // Only if all of situation has met, then we can exit now. 
    }
    return hasFS;
  }

	// Check wehther two adjacent cache lines has potential false sharing 
  unsigned long doEvaluateAdjacentCachelines(bool checkBackward, unsigned long index, bool * hasFSPotential) {
    unsigned long alignmentOffset = 0;
    *hasFSPotential = false;

    cachetrack * thisCacheTrack = getCacheTracking(index);
    if(thisCacheTrack == NULL) {
      return 0;
    }
    struct wordinfo * wordAccesses = thisCacheTrack->getWordAccesses();

    // The FS distance between hot accesses of two adjacent cache lines.
    unsigned long distance = 0;
    bool hasFS = false;
 
    for(int i = 0; i < xdefines::WORDS_PER_CACHE_LINE; i++) {
      if(wordAccesses[i].writes >= xdefines::THRESHOLD_HOT_ACCESSES ||
         wordAccesses[i].reads >= xdefines::THRESHOLD_HOT_ACCESSES) {
        // hot accesses of other cache line. 
        hasFS = searchSuitableHotAccess(checkBackward, index, &wordAccesses[i], i, &distance); 
      }

      if(hasFS) {
        break;
      }
    } 
    if(!hasFS) {
      return 0;
    }

    // Verify whether the distance between two different cache lines  
    if(distance > 0 && distance < CACHE_LINE_SIZE) {
      alignmentOffset = (CACHE_LINE_SIZE - distance)/2;
      *hasFSPotential = true;
      //fprintf(stderr, "line %ld, distance %lx, alignment %lx, final aligmentoffset %lx\n", __LINE__, distance, alignmentOffset, alignup(alignmentOffset, xdefines::ADDRESS_ALIGNMENT));  
    }
    return alignup(alignmentOffset, xdefines::ADDRESS_ALIGNMENT);
  }

  bool checkFalsesharing(unsigned long index) {
    bool falsesharing = false;
    cachetrack * track = getCacheTracking(index);
    if(track) {
      falsesharing = track->hasFalsesharing(); 
    }

    return falsesharing;
  }

  // Start to check the potential false sharing there.
  void evaluatePotentialFalsesharing(void * address, unsigned long index) {
    // If any of adjacent cache line has been evaluated or has false sharing,
    // no need to check false sharing on slow path.
    if(hasEvaluatedAdjacentCachelines(index))
      return;
     
		//fprintf(stderr, "Evaluating Potential False Sharing on address %p\n", address);

    evaluatePotentialFSDiffAlignment((unsigned long)address, index);
    evaluatePotentialFSDoubleSize((unsigned long)address, index);
  }

	// Check whether there is a potential false sharing problem if the object alignment is different.
  void evaluatePotentialFSDiffAlignment(unsigned long address, unsigned long index) {
    bool hasPotentialFS = false;
    // Verify the adjacent cache lines
    unsigned long alignmentOffset;

		// Evaluate whether current cache line and its previous cache line has potential false sharing?
    alignmentOffset = doEvaluateAdjacentCachelines(true, index, &hasPotentialFS);
    
    if(!hasPotentialFS) {
			// Evaluate whether current cache line and its next cache line has potential false sharing?
      alignmentOffset = doEvaluateAdjacentCachelines(false, index, &hasPotentialFS);
    }
    // If we find potential false sharing problem in ajacent cachelines, we install the prediction 
		// mechanism on this object.
    if(hasPotentialFS) {
      installFalsesharingPrediction(alignmentOffset, (unsigned long)address, index);
    }
  }

  void evaluatePotentialFSDoubleSize(unsigned long address, unsigned long index) {
    bool hasPotentialFS = false;
 
  }

  // ReebleFalseSharingChecking
  void reenablePotentialFalsesharingChecking(unsigned long index) {
    // Mark all adjacent cache line to be non invaluated.

  } 

  /* Install false sharing prediction by given specific address. */
  void installFalsesharingPrediction(unsigned long offset, unsigned long address, unsigned long index) { 
//    fprintf(stderr, "Possible false sharing, alignmentOffset %lx\n", offset);
    // Now we found some potential false sharing: try to adjust the alignment of memory allocation
    objectHeader * object = getHeapObjectHeader((unsigned long *)address);
    unsigned long objectStart = (unsigned long)object->getObjectStart();
    unsigned long firstLine = getCacheline((unsigned long)object->getObjectStart());
    unsigned long lastLine = getCacheline((unsigned long)object->getObjectEnd());
    cachetrack * track = NULL;
    unsigned long i;

    // Mark all corresponding cache lines evaluated at first so other threads won't 
    // start on slow PATH at all.
    // Also, we must check whether there are some false sharing captured or not.
    // if captured, no need to install prediction module at all.
    bool falsesharing = false;
    for(i = firstLine; i <= lastLine; i++) {
      if(checkFalsesharing(i)) {
        //fprintf(stderr, "FINDING false sharing at cacheline %lx\n", i);
        falsesharing = true;
      }
    }

    // If any cache line has captured false sharing, 
    // we do not need to install prediction mechanism.
    if(falsesharing) {
      return;
    }

    unsigned long line = 0;
    unsigned long cacheStart = objectStart - offset;
    bool insertOneMoreEntry = false;
   // fprintf(stderr, "POTENTIAL false sharing: objectStart %p objectEnd %p. New cache start %lx!\n", object->getObjectStart(), object->getObjectEnd(), cacheStart); 

    cacheinfo * lastCacheinfo = NULL;
 
    // Install the prediction for all related cachelines.
    for(i = firstLine; i <= lastLine; i++, cacheStart += CACHE_LINE_SIZE) {
      if(cacheStart >= (intptr_t)object->getObjectEnd()) {
        break;
      }

      track = getCacheTracking(i);
      if(track == NULL) {
		//		PRERR("track is NULL at line %d calling trackCacheline\n", i);
        track = trackCacheline(cacheStart, i);
      }
     
			while(track == NULL) {
				atomic_cpuRelax();
				track = getCacheTracking(i);
		//		PRERR("track is NULL at line %d firstLine %d\n", i, firstLine);
			} 
      	
      assert(track != NULL);
			// Install current cache start
      lastCacheinfo = track->installCachePrediction(cacheStart, lastCacheinfo);
    }
  }

  inline void allocateCachetrack(unsigned long index) {
    size_t cachestart = index << CACHE_LINE_SIZE_SHIFTS;
    cachetrack * track = NULL;
    track = allocCacheTrack(cachestart, xdefines::THRESHOLD_TRACK_DETAILS);
    // Set to corresponding array.
    //fprintf(stderr, "allocCacheTrack index %ld at %p set to %p\n", index, &_cacheTrackings[index], track);
    if(!atomic_compare_and_swap((unsigned long *)&_cacheTrackings[index], 0, (unsigned long)track)) {
      deallocCacheTrack(track);
    }
  
    // Tracking adjacent cachelines for a heap object.
    trackAdjacentCachelines(index);
  }
 
  // Main entry of handle each access
  inline void handleAccess(unsigned long addr, int bytes, eAccessType type) {
    if((intptr_t)addr > MAX_USER_SPACE) {
      return;
    }
    // We only care those acesses of global and heap.
    unsigned long index = getCachelineIndex(addr);
    cachetrack * track = NULL;
    eResultHandleAccess result = E_RHA_NONE;
    
//    fprintf(stderr, "HandleAccess addr %lx bytes %d\n", addr, bytes);
    unsigned long * status = &_cacheWrites[index];
    unsigned long totalWrites = *status;
    if(totalWrites < xdefines::THRESHOLD_TRACK_DETAILS) {
      // Only update writes
      if(type == E_ACCESS_WRITE) {
        if(atomic_increment_and_return(status) == xdefines::THRESHOLD_TRACK_DETAILS-1) {
          size_t cachestart = getCacheStart((void *)addr);
          track = allocCacheTrack(cachestart, xdefines::THRESHOLD_TRACK_DETAILS);
          // Set to corresponding array.
          atomic_compare_and_swap((unsigned long *)&_cacheTrackings[index], 0, (unsigned long)track);

          // Tracking adjacent cachelines for a heap object.
          trackAdjacentCachelines(index);
        }
      } // end if type == E_ACCESS_WRITE
    }
    else {
      // Update the status.
      track = (cachetrack *)(_cacheTrackings[index]);
      //fprintf(stderr, "handleTrack on cache index %ld track %lp addr %lx status %p\n", index, track, addr, status);
      if(track != NULL) {
				// Verify whether this pointer is valid?
        if((intptr_t)track <= xdefines::THRESHOLD_TRACK_DETAILS) {
          PRERR("WHAT?????track at %p\n", track);
        }
        assert((intptr_t)track > xdefines::THRESHOLD_TRACK_DETAILS);
        result = track->handleAccess((void *)addr, bytes, type);
#ifdef PREDICTION
        if(result == E_RHA_EVALUATE_FALSE_SHARING) {
          evaluatePotentialFalsesharing((void *)addr, index);
        }
#endif
      } 
    }
  }

private:
  objectHeader * getObjectHeader (void * ptr) {
    objectHeader * o = (objectHeader *) ptr;
    return (o - 1);
  }

  /// The protected heap used to satisfy big objects requirement. Less
  /// than 256 bytes now.
  bool _isMultithreading;
  xpheap<xoneheap<xheap> > _heap;
  xglobals   _globals;
  
  // We are maintainning a cache line status for each cache line
#ifdef CENTRALIZED_BIG_HEAP
	bigheap _bheap;
#endif 
  unsigned long * _cacheWrites;
  cachetrack ** _cacheTrackings;
  unsigned long _heapCacheIndex; 
};

#endif
