// -*- C++ -*-

/*
  Copyright (c) 2011, University of Massachusetts Amherst.

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
 * @file   xpheap.h
 * @brief  A heap optimized to reduce the likelihood of false sharing.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef _BIGHEAP_H_
#define _BIGHEAP_H_

#include "kingsleyheap.h"
#include "objectheader.h"
#include "xdefines.h"

typedef struct {
	// Blocks in this size class.
	int index; 
	void * blocks[xdefines::NUM_HEAPS];
} bigHeapClass;

class bigheap
{

public: 
  bigheap() {
  }

  /// @brief Initialize the system.
  void initialize()
  {
    _totalBlocks = xdefines::MAX_THREADS;
		_totalClasses = xdefines::BIG_HEAP_SIZE_CLASSES;
 		_minSize = xdefines::BIG_HEAP_CHUNK_SIZE;
 
    pthread_mutex_init(&_lock, NULL);

    for(int i = 0; i < _totalClasses; i++) {
  		_classes[i].index = 0;
    }
  }

	// Get size classes
	size_t getSizeClass(size_t sz) {
		size_t thisClass = Kingsley::size2Class(sz)-17;
		//printf("thisClass is %d\n", thisClass);
		assert(thisClass < _totalClasses);
		return thisClass;
	}

	// Record this entry. It can be failed if the cache is full.
	bool free(void * ptr, size_t sz) {
		bool result = false;
		size_t sizeclass = getSizeClass(sz);
		bigHeapClass * myClass = &_classes[sizeclass];
	
		global_lock();
		
		if(myClass->index < _totalBlocks) {
			myClass->blocks[myClass->index] = ptr;
			result = true;	
			myClass->index++;
		}

		global_unlock();

		return result;
	}

  // Allocate a thread index under the protection of global lock
  void * malloc(size_t size) {
		void * ptr = NULL;	

		size_t sizeclass = getSizeClass(size);
		bigHeapClass * myClass = &_classes[sizeclass];
	
		global_lock();
		
		if(myClass->index > 0) {
			ptr = myClass->blocks[--myClass->index];
		}

		global_unlock();
    return ptr; 
  }

 
private:
  /// @brief Lock the lock.
  void global_lock(void) {
    pthread_mutex_lock(&_lock);
  }

  /// @brief Unlock the lock.
  void global_unlock(void) {
    pthread_mutex_unlock(&_lock);
  }
  
  //  fprintf(stderr, "remove thread %p with thread index %d\n", thread, thread->index);
  pthread_mutex_t _lock;
	size_t  _totalBlocks;
	size_t  _totalClasses;
	size_t  _minSize;
  bigHeapClass _classes[xdefines::BIG_HEAP_SIZE_CLASSES];
};

#endif // _BIGHEAP_H_


