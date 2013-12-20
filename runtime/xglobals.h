// -*- C++ -*-

/*
  Copyright (c) 2008-12, University of Massachusetts Amherst.

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
 * @file   xglobals.h
 * @brief  Management the globals of applications, here we donot care about those globals of libc.so and libpthread.so.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef _XGLOBALS_H_
#define _XGLOBALS_H_

#include "xdefines.h"
#include "selfmap.h"

/// @class xglobals
/// @brief Maps the globals region onto a persistent store.
class xglobals {
public:
  
  
  xglobals ()
  {
   // fprintf(stderr, "GLOBALS_START is %x\n", GLOBALS_START);
  }

  void initialize(void) {
    // We check those mappings to find out existing globals.
    int numb;
    int i;
    
    // Trying to get information about different text segmensts.
    selfmap::getInstance().getTextRegions();

    // Trying to get the information of global regions
    selfmap::getInstance().getGlobalRegion((void **)&globalStart, (void **)&globalEnd);
  }

private:
};

#endif

