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
 * @file   xmapping.h
 * @brief  Manage all about mappings, such as changing the protection and unprotection, 
 *         managing the relation between shared and private pages.
 *         Adopt from sheiff framework, but there are massive changes.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 


#ifndef _XMAPPING_H_
#define _XMAPPING_H_

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
#include "mm.h"

using namespace std;

class xmapping {

public:

  xmapping() 
    : _start (NULL),
      _size (0)
  {

  }
  
  ~xmapping () {  }

  // Initialize the map and corresponding part.
  void * initialize(void * startaddr = 0, size_t size = 0, size_t metasize = 0)
  {
    void * ptr = NULL;

    // Check whether the size is valid, should be page aligned.
    if(size % xdefines::PageSize != 0) {
      PRFATAL("Wrong size %lx, should be page aligned.\n", size);
    }

    _size = size;
    if(startaddr) {
      _isHeap = false;
      _start = (char *)startaddr;
    }
    else {
      _isHeap = true;
      void * startHeap = (void *)(USER_HEAP_BASE - metasize);
      // Allocate the application heap
      ptr = MM::mmapAllocatePrivate(size+metasize, startHeap);
      _start = (char *)((intptr_t)ptr + metasize);
      assert(_start == (char *)USER_HEAP_BASE);
      heapStart = (unsigned long)_start;
      heapEnd = ((unsigned long)_start + size);
    }
    _end = (void *)((intptr_t)_start + _size);

#ifndef NDEBUG
   fprintf (stderr, "_start %p, _end %p isHeap %d\n", _start, _end, _isHeap);
#endif
    return ptr;
  }

  // Do nothing 
  void finalize(void) {
  }

  /// @return the start of the memory region being managed.
  inline char * base (void) const {
    return (char *)_start;
  }

  /// @return the size in bytes of the underlying object.
  inline size_t size (void) const {
    return _size;
  }

private:

  /// True if current xmapping.h is a heap.
  bool _isHeap;

  /// The starting address of the region.
  void * _start;

  /// The size of the region.
  size_t _size;

  /// The starting address of the region.
  void * _end;
};

#endif

