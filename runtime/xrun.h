// -*- C++ -*-

/*
  Author: Emery Berger, http://www.cs.umass.edu/~emery
 
  Copyright (c) 2007-12 Emery Berger, University of Massachusetts Amherst.

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
 * @file   xrun.h
 * @brief  The main engine for consistency management, etc.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef SHERIFF_XRUN_H
#define SHERIFF_XRUN_H

#include "xdefines.h"
#include "xthread.h"

// memory
#include "xmemory.h"
#include "internalheap.h"

// Grace utilities
#include "atomic.h"

class xrun {

private:

  xrun()
  : _memory (xmemory::getInstance())
  {
  }

public:

  static xrun& getInstance() {
    static char buf[sizeof(xrun)];
    static xrun * theOneTrueObject = new (buf) xrun();

    return *theOneTrueObject;
  }

  /// @brief Initialize the system.
  void initialize()
  {
		installSignalHandler();
    InternalHeap::getInstance().initialize();
    xthread::getInstance().initialize();
    // Initialize the memory (install the memory handler)
    _memory.initialize();
//    fprintf(stderr, "in the init of xrun, finishing xmemoy\n");
  }

  void finalize (void)
  {
    fprintf(stderr, "Finalizing, check the false sharing problem now.\n");
    // If the tid was set, it means that this instance was
    // initialized: end the transaction (at the end of main()).
    _memory.finalize();
  }

  /// @brief Install a handler for KILL signals.
  void installSignalHandler() {
	  // Now set up a signal handler for SIGINT events.
    struct sigaction siga;

    // Point to the handler function.
    //siga.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART | SA_NODEFER;
    siga.sa_flags = SA_RESTART | SA_NODEFER;
		//fprintf(stderr, "install signale handler %d\n", __LINE__);

    siga.sa_handler = xmemory::sigHandler;
    if (sigaction(SIGINT, &siga, NULL) == -1) {
      perror ("installing SIGINT failed\n");
      exit (-1);
		}
#ifdef USING_SIGUSR2
    if (sigaction(SIGUSR2, &siga, NULL) == -1) {
      perror ("installing SIGINT failed\n");
      exit (-1);
		}
#endif
	}

private:

  int _tid;

  /// The memory manager (for both heap and globals).
  xmemory&     _memory;
};


#endif
