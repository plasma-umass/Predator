
// -*- C++ -*-

/*
Allocate and manage thread index.
Second, try to maintain a thread local variable to save some thread local information.
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
 * @file   xthread.h
 * @brief  Managing the thread creation, etc.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef _XTHREAD_H_
#define _XTHREAD_H_

#include <new>
#include <pthread.h>
#include <sys/types.h>
#include "xdefines.h"

class xthread {
private:
    xthread() 
    { }

public:
  static xthread& getInstance() {
    static char buf[sizeof(xthread)];
    static xthread * theOneTrueObject = new (buf) xthread();
    return *theOneTrueObject;
  }

  /// @brief Initialize the system.
  void initialize()
  {
    _aliveThreads = 0;
    _threadIndex = 0;

    _totalThreads = xdefines::MAX_THREADS;
  
    pthread_mutex_init(&_lock, NULL);

    // Shared the threads information. 
    memset(&_threads, 0, sizeof(_threads));

    // Initialize all mutex.
    thread_t * thisThread;
  
    for(int i = 0; i < _totalThreads; i++) {
      thisThread = &_threads[i];
 
      thisThread->available = true;
    }

    // Allocate the threadindex for current thread.
    initInitialThread(); 
  }

  // Initialize the first threadd
  void initInitialThread(void) {
    int tindex;
    
     // Allocate a global thread index for current thread.
    tindex = allocThreadIndex();

    // First, xdefines::MAX_ALIVE_THREADS is too small.
    if(tindex == -1) {
      PRDBG("The alive threads is larger than xefines::MAX_THREADS larger!!\n");
      assert(0);
    }

    // Get corresponding thread_t structure.
    current  = getThreadInfo(tindex);

    current->index = tindex;
    current->self  = pthread_self();
  }


  thread_t * getThreadInfo(int index) {
    assert(index < _totalThreads);

    return &_threads[index];
  }

  // Allocate a thread index under the protection of global lock
  int allocThreadIndex(void) {
   int index = -1;

    if(_aliveThreads >= _totalThreads) {
      fprintf(stderr, "Set xdefines::MAX_THREADS to larger. _alivethreads %d totalthreads %d", _aliveThreads, _totalThreads);
      abort(); 
    } 

    int origindex = _threadIndex;
//		fprintf(stderr, "threadindex %d\n", _threadIndex);
    thread_t * thread;
    while(true) {  
      thread = getThreadInfo(_threadIndex);
      if(thread->available) {
        thread->available = false;
        index = _threadIndex;
        
        // A thread is counted as alive when its structure is allocated.
        _aliveThreads++;

        _threadIndex = (_threadIndex+1)%_totalThreads;
        break;
      }
      else {
        _threadIndex = (_threadIndex+1)%_totalThreads;
      }
    
      // It is impossible that we search the whole array and we can't find
      // an available slot. 
      assert(_threadIndex != origindex); 
    }
    return index; 
  }

  /// Create the wrapper 
  /// @ Intercepting the thread_creation operation.
  int thread_create(pthread_t * tid, const pthread_attr_t * attr, threadFunction * fn, void * arg) {
    void * ptr = NULL;
    int tindex;
    int result;

    // Lock and record
    global_lock();

    // Allocate a global thread index for current thread.
    tindex = allocThreadIndex();
    
    // First, xdefines::MAX_ALIVE_THREADS is too small.
    if(tindex == -1) {
      PRDBG("The alive threads is larger than xefines::MAX_THREADS larger!!\n");
      assert(0);
    }
 
    // Get corresponding thread_t structure.
    thread_t * children = getThreadInfo(tindex);
    
    children->index = tindex;
    children->startRoutine = fn;
    children->startArg = arg;

    global_unlock();

    result =  WRAP(pthread_create)(tid, attr, startThread, (void *)children);

    // Set up the thread index in the local thread area.
    return result;
  }      


  // @Global entry of all entry function.
  static void * startThread(void * arg) {
    void * result;

    current = (thread_t *)arg;
//    current->tid = gettid();
    current->self = pthread_self();

    // from the TLS storage.
    result = current->startRoutine(current->startArg);

    // Decrease the alive threads
    xthread::getInstance().removeThread(current);
    
    return result;
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
  
  void removeThread(thread_t * thread) {
  //  fprintf(stderr, "remove thread %p with thread index %d\n", thread, thread->index);
    global_lock();

    current->available = true;
    --_aliveThreads;

    if(_aliveThreads == 0) {
      isMultithreading = false;
    }

    global_unlock();
  }

  pthread_mutex_t _lock;
  int _threadIndex;
  int _aliveThreads;
  int _totalThreads;  
  // Total threads we can support is MAX_THREADS
  thread_t  _threads[xdefines::MAX_THREADS];
};
#endif

