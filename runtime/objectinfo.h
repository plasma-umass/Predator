// -*- C++ -*-

#ifndef SHERIFF_OBJECTINFO_H
#define SHERIFF_OBJECTINFO_H

#include "xdefines.h"

class ObjectInfo {
public:
  // Whether this is a heap object
  bool isHeapObject;
  bool hasPotentialFS;
  int  accessThreads;     // False sharing type, inter-objects or inner-object
  int  times; 
  unsigned long words;
  long invalidations;
  unsigned long totalWrites;
  unsigned long totalAccesses;
  unsigned long unitlength;
  unsigned long totallength;
  unsigned long * start;
  unsigned long * stop;
  void * winfo;
  union {
    // Used for globals only.
    void * symbol;     
    
    // Callsite information.
    unsigned long callsite[CALL_SITE_DEPTH];
  } u;
};

#endif
