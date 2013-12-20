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
 * @file   Cachehistogram.h
 * @brief  Cache histogram is used to save the histogram about one cache line.
 *         Histogram is used to determine whether there is a cache invalidation or not when
 *         a new access is arrived. Here, we are maintaining two entries here just for simplicity.  
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 


#ifndef _CACHESTATUS_H_
#define _CACHESTATUS_H_

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

#include "xdefines.h"
using namespace std;

class cachehistogram {
  // We only use two entries here.
  enum { MAX_ENTRIES = 2 };

public:
  cachehistogram() 
  {
    
  }
  
  virtual ~cachehistogram (void) {
  }

  // This initialization is strange since we want to
  // avoid the checking whether the table is empty or not 
  // every time. 
  // In the normal execution, there is only two states: one is not full, second is full.
  // we will not have the state that table is empty.
  // So every cache line will get an invalidation
  // for free.
  inline void init(void) {
    _entries[0].tindex = 0xFF;
    _entries[0].type = E_ACCESS_WRITE;
    _index = 1;
  }

  inline bool isFullTable(void) {
    return _index == MAX_ENTRIES;
  }

  // Flush entries by write.
  inline void flushEntriesOnWrite(int tindex) {
    _entries[0].tindex = tindex;
    _entries[0].type = E_ACCESS_WRITE;
    _index = 1;
  }

  // Update cache histogram. Return value is used to indicate 
  // whether it is an invalidations or not
  inline bool updateHistogram(int tindex, eAccessType type) {
    bool hasInvalidations = false;

    if(type == E_ACCESS_READ) {
      // Check whether the table is full.
      // when the table is full, we do nothing.
      if(!isFullTable()) {
        assert(_index == 1);

        // We only add the entry when it is a different thread.
        if(_entries[0].tindex != tindex) {
          _entries[_index].tindex = tindex;
          _entries[_index].type = type;
          _index++;
        }
      }
    }
    else {
      if(isFullTable()) {
        flushEntriesOnWrite(tindex);
        hasInvalidations = true;
      }
      else {
        // Now we only have one entry there.
        if(_entries[0].tindex == tindex) {
          // We should upgrade when original is a read
          if(_entries[0].type != type) {
            _entries[0].type = type;
          }
        }
        else {
          flushEntriesOnWrite(tindex);
          hasInvalidations = true;
        }
      }
    }

    return hasInvalidations;
  }

private:
  /*
    We may not need the type at all.
    If access is a write,
      Full --> flush
      Not full --> check the first entry -- > if same, do nothing. Different, flush.

    when access is a read, 
      Full --> do nothing.
      Not full --> check the first entry --> if same do nothing. Different, fill entry.
  */
   
  struct accessEntry {
    int tindex;
    eAccessType type;
  };

  // current entry index;
  int    _index; 
  struct accessEntry _entries[MAX_ENTRIES];
};

#endif

