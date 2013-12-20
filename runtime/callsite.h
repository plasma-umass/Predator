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
 * @file   callsite.h
 * @brief  Management about callsite for heap objects.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 
    
#ifndef SHERIFF_CALLSITE_H
#define SHERIFF_CALLSITE_H

#include <link.h>
#include <stdio.h>
#include <cassert>
#include <setjmp.h>
#include <execinfo.h>

class CallSite {
public:
  CallSite() {
    for (int i = 0; i < CALL_SITE_DEPTH; i++) {
      _callsite[i] = 0;
    }
  }

  unsigned long getItem(unsigned int index)
  {
    assert (index < CALL_SITE_DEPTH);
    return _callsite[index];
  }

  unsigned long getDepth() 
  {
    return CALL_SITE_DEPTH;
  }

  void print()
  {
    printf("ALLOCATION CALL SITE: ");
    for(int i = 0; i < CALL_SITE_DEPTH; i++) {
      printf("%lx\t", _callsite[i]);
    }
    printf("\n");
  }

  void fetch(void) {
    void * array[10];
    int size;
    int log = 0;

  //  PRDBG("Try to get backtrace with array %p\n", array);
    if(!isBacktrace) {
      isBacktrace = true;
      // get void*'s for all entries on the stack
      size = backtrace(array, 11);
      isBacktrace = false;
   
      for(int i = 0; i < size; i++) {
        unsigned long addr = (unsigned long)array[i];
        //fprintf(stderr, "textStart %lx textEnd %lx addr %lx at log %d\n", textStart, textEnd, addr, log);
        if(addr >= textStart && addr <= textEnd) {
          _callsite[log++] = addr;
          if(log == CALL_SITE_DEPTH) {
            break;
          }
        }
      }
    }
  }

  unsigned long _callsite[CALL_SITE_DEPTH];
};

#endif
