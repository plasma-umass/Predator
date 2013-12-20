// -*- C++ -*-

/*
  Author: Emery Berger, http://www.cs.umass.edu/~emery
 
  Copyright (c) 2007-8 Emery Berger, University of Massachusetts Amherst.

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

#ifndef _XONEHEAP_H_
#define _XONEHEAP_H_

/**
 * @class xoneheap
 * @brief Wraps a single heap instance.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

template <class SourceHeap>
class xoneheap {
public:
  xoneheap() {
    //fprintf(stderr, "xoneheap constructor.\n");
  }
  void * initialize(size_t size, size_t metasize) { return getHeap()->initialize(size, metasize); }
  void finalize (void * end) { getHeap()->finalize(end); }
#if 0
  void cleanup() { getHeap()->cleanup(); }

  bool FSSuspect(void * addr, size_t size) { return getHeap()->FSSuspect(addr, size); }
  void handleAccess(void * addr, int bytes, eAccessType type) { return getHeap()->handleAccess(addr, bytes, type); }

  void reportFalseSharing(void) { return getHeap()->reportFalseSharing(getHeapPosition()); }
  bool inRange (void * ptr) { return getHeap()->inRange(ptr); }
#endif
  // Get heap start and end, this is used to check range.
  void * getHeapStart(void) { return getHeap()->getHeapStart(); }
  void * getHeapEnd(void) { return getHeap()->getHeapEnd(); }
  void * getHeapPosition(void) { return getHeap()->getHeapPosition(); }
 
  bool nop() { return getHeap()->nop(); }
 

  void * malloc (size_t sz) { return getHeap()->malloc(sz); }
  void free (void * ptr) { getHeap()->free(ptr); }
  size_t getSize (void * ptr) { return getHeap()->getSize(ptr); }

private:

  SourceHeap * getHeap (void) {
    static char heapbuf[sizeof(SourceHeap)];
    static SourceHeap * _heap = new (heapbuf) SourceHeap;
    //fprintf (stderr, "heap is in %p\n", _heap);
    return _heap;
  }

};


#endif // _XONEHEAP_H_
