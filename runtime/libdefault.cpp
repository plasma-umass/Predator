#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include "xthread.h"
#include "cachetrack.h"
#include "xrun.h"

extern "C" {
  bool isMultithreading = false;
  void initializer (void) __attribute__((constructor));
  void finalizer (void)   __attribute__((destructor));
  bool initialized = false;
  unsigned long textStart, textEnd;
  unsigned long globalStart, globalEnd;
  unsigned long heapStart, heapEnd;
  #define INITIAL_MALLOC_SIZE 81920
  static char * tempalloced = NULL;
  static int remainning = 0;
  __thread thread_t * current;
  __thread bool isBacktrace = false;
  xmemory  & _memory = xmemory::getInstance();

  // Exposing these functions to the instrumented programs.
  void allocCacheTrack(unsigned long index)  {
  //  fprintf(stderr, "allocateCacheTrack on index %ld \n", index);
    xmemory::getInstance().allocateCachetrack(index);
  }
 
  void printAddr(unsigned long addr) {
    fprintf(stderr, "printAddr with address %lx\n", addr);
  }
  
  void printValue(unsigned long value) {
    fprintf(stderr, "printValue with value %lx\n", value);
  }
 
  void handleReadAccess(unsigned long addr, size_t size) {
    //if(isMultithreading) {
   //   fprintf(stderr, "handleAccess addr %lx size %ld isWrite %d\n", addr, size, isWrite);
    fprintf(stderr, "handleReadAccess addr %lx size %ld RRRRRRRRRRRRRRRRRRRRRR\n", addr, size);
    xmemory::getInstance().handleAccess(addr, size, E_ACCESS_READ);
    //}
  }

  void handleWriteAccess(unsigned long addr, size_t size) {
    //if(isMultithreading) {
    fprintf(stderr, "handleWriteAccess addr %lx size %ld WWWWWWWWWWWWWWWWWWWWW\n", addr, size);
    xmemory::getInstance().handleAccess(addr, size, E_ACCESS_WRITE);
    //}
  }

  void handleAccess(unsigned long addr, size_t size, unsigned long isWrite) {
    //if(isMultithreading) {
    if(isWrite == 0) {
      xmemory::getInstance().handleAccess(addr, size, E_ACCESS_READ);
    }
    else { 
      xmemory::getInstance().handleAccess(addr, size, E_ACCESS_WRITE);
    }
  }
  
#if 1
  void store_16bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 16, E_ACCESS_WRITE);
    }
  }

  void store_8bytes(unsigned long addr) {
    //fprintf(stderr, "store_8bytes %lx WWWWWWWWWWWWWWWWWWWWW isMulthreading %d\n", addr, isMultithreading);
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 8, E_ACCESS_WRITE);
    }
  }
  
  void store_4bytes(unsigned long addr) {
    //fprintf(stderr, "store_4bytes %lx WWWWWWWWWWWWWWWWWWWWW\n", addr);
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 4, E_ACCESS_WRITE);
    }
  } 
  
  void store_2bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 2, E_ACCESS_WRITE);
    }
  } 
  
  void store_1bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 1, E_ACCESS_WRITE);
    }
  } 
  
  void load_16bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 16, E_ACCESS_READ);
    }
  }
  
  void load_8bytes(unsigned long addr) {
    //fprintf(stderr, "load_8bytes %lx WWWWWWWWWWWWWWWWWWWWW\n", addr);
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 8, E_ACCESS_READ);
    }
  }
  
  void load_4bytes(unsigned long addr) {
    //fprintf(stderr, "load_4bytes %lx WWWWWWWWWWWWWWWWWWWWW\n", addr);
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 4, E_ACCESS_READ);
    }
  }
  
  void load_2bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 2, E_ACCESS_READ);
    }
  }
  
  void load_1bytes(unsigned long addr) {
    if(isMultithreading) {
      xmemory::getInstance().handleAccess(addr, 1, E_ACCESS_READ);
    }
  }
#endif

  void initializer (void) {
    // Using globals to provide allocation
    // before initialized.
    // We can not use stack variable here since different process
    // may use this to share information. 
    static char tempbuf[INITIAL_MALLOC_SIZE];

    // temprary allocation
    tempalloced = (char *)tempbuf;
    remainning = INITIAL_MALLOC_SIZE;

    init_real_functions();
 
//   fprintf(stderr, "in the initializer\n");
    xrun::getInstance().initialize();
    initialized = true;
  }

  void finalizer (void) {
    initialized = false;
    xrun::getInstance().finalize();
  }

  inline void setMultithreadingPhase(void) {
    isMultithreading = true;
  }

  // Temporary mallocation before initlization has been finished.
  static void * tempmalloc(int size) {
    void * ptr = NULL;
    if(remainning < size) {
      // complaining. Tried to set to larger
   //   printf("Not enough temporary buffer, size %d remainning %d\n", size, remainning);
      exit(-1);
    }
    else {
      ptr = (void *)tempalloced;
      tempalloced += size;
      remainning -= size;
    }
    return ptr;
  }

  /// Functions related to memory management.
  void * malloc (size_t sz) {
    void * ptr;
  //  printf("**MALLOC sz %ld, initialized %d******\n", sz, initialized);
    if (!initialized) {
      ptr = tempmalloc(sz);
    } else {
   // printf("**actual MALLOC sz %ld *\n", sz);
      ptr = xmemory::getInstance().malloc (sz);
    }
    if (ptr == NULL) {
      fprintf (stderr, "Out of memory!\n");
      ::abort();
    }
//    fprintf(stderr, "********MALLOC sz %ld ptr %p***********\n", sz, ptr);
    return ptr;
  }
  
  void * calloc (size_t nmemb, size_t sz) {
    void * ptr;
    
    ptr = malloc (nmemb * sz);
	  memset(ptr, 0, sz*nmemb);
    return ptr;
  }

  void free (void * ptr) {
    // We donot free any object if it is before 
    // initializaton has been finished to simplify
    // the logic of tempmalloc
    if(initialized) {
      xmemory::getInstance().free (ptr);
    }
  }

  size_t malloc_usable_size(void * ptr) {
    if(initialized) {
      return xmemory::getInstance().malloc_usable_size(ptr);
    }
    return 0;
  }

  // Implement the initialization
  void * memalign (size_t boundary, size_t size) {
    if(initialized) {
      return xmemory::getInstance().memalign(boundary, size);
    }
	  fprintf(stderr, "Does not support memalign. boundary %lx, size %lx\n", boundary, size);
//    ::abort();
    return NULL;
  }

  void * realloc (void * ptr, size_t sz) {
    return xmemory::getInstance().realloc (ptr, sz);
  }

  // Intercept the pthread_create function.
  int pthread_create (pthread_t * tid, const pthread_attr_t * attr, void *(*start_routine) (void *), void * arg)
  {
    setMultithreadingPhase();
    return xthread::getInstance().thread_create(tid, attr, start_routine, arg);
  }
};

