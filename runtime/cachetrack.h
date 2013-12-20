// -*- C++ -*-

/*
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
 * @file   cachetrack.h
 * @brief  Track read/write access on specified cache. 
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 


#ifndef _TRACK_H_
#define _TRACK_H_

#include <set>
#include <list>
#include <vector>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <algorithm> //sort
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#include "atomic.h"

#include "xdefines.h"
#include "ansiwrapper.h"
#include "cacheinfo.h"
#include "cachehistogram.h"
#include "spinlock.h"
#include "internalheap.h"

using namespace std;

extern "C" {
  struct wordinfo {
    int         unitsize;
    int         tindex;
    int         reads;
    int         writes;
  };

  typedef enum E_Result_Handle_Access {
    E_RHA_NONE = 0,
    E_RHA_TRACK_ADJACENT_CACHELINES, 
    E_RHA_EVALUATE_FALSE_SHARING 
  } eResultHandleAccess;
};

class cachetrack {
public:
//  enum { WORD_SIZE = 4 };
  enum { WORD_THREAD_SHARED = 0xFFFFFFFF };
  enum { WORD_THREAD_INIT = xdefines::MAX_THREADS };

  typedef enum E_Access_Pattern {
    E_ACCESS_FALSE_SHARING = 0,
    E_ACCESS_TRUE_SHARING
  }eAccessPattern;

  cachetrack(size_t start, unsigned long writes) {
    _cacheStart = start;
    _lock.init();
    _thisCache.initialize((void *)_cacheStart, CACHE_LINE_SIZE);
    _writes = writes; 
    _accesses = 0;
    _hasPredicts = false;

    // Set the words information.
    memset(&_words[0], 0, sizeof(struct wordinfo) * xdefines::WORDS_PER_CACHE_LINE); 
    for(int i = 0; i < xdefines::WORDS_PER_CACHE_LINE; i++) {
      _words[i].tindex = WORD_THREAD_INIT; 
    }
  }
  
  ~cachetrack (void) {
  }

  void resetCacheinfo(unsigned long addr) {
//    fprintf(stderr, "resetCacheinfo at addr %lx\n", addr);
    _thisCache.initialize((void *)addr, CACHE_LINE_SIZE);
  }

  cacheinfo * installCachePrediction(unsigned long start, cacheinfo * lastCacheInfo) {
    // Allocate a cache info.
    cacheinfo * cacheInfo = (cacheinfo *)InternalHeap::getInstance().malloc(sizeof(cacheinfo));
    assert(cacheInfo != NULL);
    cacheInfo->initialize((void *)start, CACHE_LINE_SIZE);

    //fprintf(stderr, "*******install cache prediction at address %lx********\n", start);
    if(lastCacheInfo) {
      _predictCache1 = lastCacheInfo;
      _predictCache2 = cacheInfo;
    }
    else {
      _predictCache1 = cacheInfo;
      _predictCache2 = NULL;
    }
    
    _hasPredicts = true;
    return cacheInfo;
  }

  // Record word access. We are not holding lock since it is impossible to have race condition.
  // Here, if the access is always double word, we do not care about next word. since 
  // the first word's unit size will be DOUBLE_WORD 
  void recordWordAccess(int tindex, void * addr, int bytes, eAccessType type) {
    int index = getCacheOffset((size_t)addr);
    if(tindex != _words[index].tindex) {
      if(_words[index].tindex == WORD_THREAD_INIT) {
        _words[index].tindex = tindex;
      }
      else if(_words[index].tindex != WORD_THREAD_SHARED) {
        _words[index].tindex = WORD_THREAD_SHARED;
      }
    }
    _words[index].unitsize = bytes;

    // Update corresponding counter for each word.
    if(type == E_ACCESS_WRITE) {
      _words[index].writes++;
    }
    else {
      _words[index].reads++;
    }
  }

  unsigned long getWrites(void) {
    return _writes;
  }
  
  unsigned long getAccesses(void) {
    return _accesses;
  }

  long getInvalidations(void) {
    long invalidations;
    invalidations = _thisCache.invalidations();
    if(_hasPredicts) {
      if(_predictCache2) {
        invalidations += _predictCache1->invalidations()/2;
        invalidations += _predictCache2->invalidations()/2;
      }
      else {
        invalidations += _predictCache1->invalidations();
      }
    }
    if(invalidations) {
    //  fprintf(stderr, "CAche invalidations %lx\n", invalidations);
    }
    return invalidations;
  }

  void reportWordAccesses(void) {
    unsigned long i;
    for(i = 0; i < xdefines::WORDS_PER_CACHE_LINE; i++) {
      if(_words[i].writes > xdefines::THRESHOLD_HOT_ACCESSES || _words[i].reads > xdefines::THRESHOLD_HOT_ACCESSES) { 
//        fprintf(stderr, "Word %ld: address %lx reads %x writes %x thread %x\n", i, (unsigned long)_cacheStart + i * xdefines::WORD_SIZE, _words[i].reads, _words[i].writes, _words[i].tindex);
      }
    } 

  }

  /// Report false sharing
  void reportFalseSharing(void) {
    if(_thisCache.invalidations() > 0) {
      fprintf(stderr, "\tCacheStart 0x%lx Accesses %lx (at %p) Writes %lx invalidations %ld THREAD %d\n", _cacheStart, _accesses, &_accesses,  _writes, _thisCache.invalidations(), getThreadIndex());
      //fprintf(stderr, "\tCacheStart 0x%lx Accesses %lx (at %p) Writes %lx invalidations %ld THREAD %d\n", _cacheStart, _accesses, &_accesses,  _writes, _thisCache.invalidations(), getThreadIndex());
    }
    if(_hasPredicts) {
      if(_predictCache1 && _predictCache1->invalidations() > 0) {
        fprintf(stderr, "Potential false sharing: _cacheStart %p invalidations %lx\n", _predictCache1->getCacheStart(), _predictCache1->invalidations());
      }
#if 0
      if(_predictCache2 && _predictCache2->invalidations() > 0) {
        fprintf(stderr, "Potential false sharing: _cacheStart %p invalidations %lx\n", _predictCache2->getCacheStart(), _predictCache2->invalidations());
      }
#endif
    }
//    if(_thisCache.invalidations() < 1) {
    reportWordAccesses();
//    }
  }

  void reportPotentialFalseSharing(void) {
//    fprintf(stderr, "PPPPPPPPPPPotential false sharing\n");
    reportFalseSharing();
  }

  void * getWordinfoAddr(int index) {
    return &_words[index];
  }

  inline bool toSampleAccess(void) {
    // Check whether we need to sample this lines accesses now.
    _accesses += 1;
    return _accesses%xdefines::SAMPLE_INTERVAL <= xdefines::SAMPLE_ACCESSES_EACH_INTERVAL;
  }

  bool hasEvaluatedFS(void) {
    return _hasPredicts;
  }

  bool hasFalsesharing(void) {
    bool falsesharing = false;
    if(getInvalidations() > 0) { 
      falsesharing = true;
    }

    return falsesharing;
  }

  // Main function to handle each access. 
  // Here, all writes on this cache line should be larger than the predifined threshold.
  eResultHandleAccess handleAccess(void * addr, int bytes, eAccessType type) {
    eResultHandleAccess result = E_RHA_NONE;
    unsigned long accessNum = _accesses;
    // Check whether we need to sample this lines accesses now.
    if(toSampleAccess()) {
      int tindex = getThreadIndex();
      int wordindex = getCacheOffset((size_t)addr);
      
      // Record the detailed information of this accesses.
      recordWordAccess(tindex, addr, bytes, type);

      lock();

      // Update the writes information 
      if(type == E_ACCESS_WRITE) {
        _writes++;

        // We should check whether we need to predict false sharings
        // based on the information what we have.
        if((_writes == xdefines::THRESHOLD_PREDICT_FALSE_SHARING) && (_hasPredicts == false)) {
          // If we already have captured invalidations, maybe we do not 
          // need to evaluate the potential false sharing anymore.
          if(!hasFalsesharing()) {
            result = E_RHA_EVALUATE_FALSE_SHARING;
          }
        }
      }

      unlock(); 
      
      // Check whether we will have an invalidation
      if(!_thisCache.handleAccess(tindex, (char *)addr, type)) {
      	if(_hasPredicts) {
        	if(!_predictCache1->handleAccess(tindex, (char *)addr, type)) {
        		if(_predictCache2) {
          		_predictCache2->handleAccess(tindex, (char *)addr, type);
        		}
					}
				}
      } 
    }

    // It is possible that we will miss some potential checking. Now let's do that again.
    if(!_hasPredicts && accessNum%xdefines::SAMPLE_INTERVAL == xdefines::THRESHOLD_PREDICT_FALSE_SHARING) {
			result = E_RHA_EVALUATE_FALSE_SHARING;
    }
    
    return result;
  }

  cacheinfo * getCacheinfo(void) {
    return &_thisCache;
  }

  void lock(void) {
    _lock.lock();
  }

  void unlock(void) {
    _lock.unlock();
  }

  struct wordinfo * getWordAccesses(void) {
    return &_words[0];
  }

private:
  //inline int getCacheOffset(size_t addr) {
  int getCacheOffset(size_t addr) {
    return ((addr - _cacheStart)/xdefines::WORD_SIZE);
  }
  
  inline unsigned long getAccessAddress(int index) {
    return (_cacheStart + index * xdefines::WORD_SIZE);
  }

  // Make sure _accesses is the first word since some of code
  // can be putted into the application itself. 
  unsigned long  _accesses;

  // How many writes on this cache line.
  unsigned long  _writes;

  spinlock       _lock;
  cacheinfo      _thisCache;
  size_t         _cacheStart;

  // Detailed information of all words in current cache line 
  struct wordinfo _words[xdefines::WORDS_PER_CACHE_LINE];

  // Whether we should check _predictCache?
  bool _hasPredicts;

  // Prediction information
  cacheinfo  * _predictCache1;
  cacheinfo  * _predictCache2;
  cacheinfo  * _doubleSizeCache;
};

#endif

