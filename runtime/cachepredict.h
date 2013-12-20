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
 * @file   Cachepredict.h
 * @brief  The information about cache prediction. 
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 


#ifndef _CACHEPREDICT_H_
#define _CACHEPREDICT_H_

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
#include "list.h"

class cachepredict {
public:
  cachepredict() 
  {
  }
  
  ~cachepredict (void) {
  }

  void init(void) {
    _predictions = 0;
  }

  // Check whether we need prediction
  inline bool hasPrediction(void) {
    return (_predictions > 0 ? true : false);
  }

  void handleAccess(int tindex, void * addr, eAccessType type) {
    if(_predictions == 0) {
      return;
    }

    

  }

private:
  // how much predictions
  int _predictions; 

  // All predictions are appended into this list.
  list_t _predictList;
};

#endif

