#ifndef __HANDLE_ACCESS_H__
#define __HANDLE_ACCESS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ATOMIC_INCREMENT_AND_RETURN 
#define THRESHOLD_TRACK_DETAILS (10000)
#define INVOKE_DETAILED_TRACKING (THRESHOLD_TRACK_DETAILS-1)
#define MAX_USER_SPACE    (0xC0000000)
#define CACHE_STATUS_BASE   (0x200000000000)
#define CACHE_TRACKING_BASE (0x200080000000)
#define SAMPLE_ACCESSES_EACH_INTERVAL THRESHOLD_TRACK_DETAILS
#define SAMPLE_INTERVAL (100 * SAMPLE_ACCESSES_EACH_INTERVAL)

// There are 3 cases of handle accesses:
// 1. 0 - do nothing
// 2. 1 - call
extern void allocCacheTrack(unsigned long index); 
extern void handleAccess(unsigned long addr, unsigned long size, int isWrite);
//extern __attribute__ ((always_inline)) inline unsigned long atomic_increment_and_return(volatile unsigned long * obj);
//extern __attribute__ ((always_inline)) inline int handleAccessBefore(unsigned long addr, int isWrite, int typesize);
// __attribute__ ((weak));    
//__attribute__ ((always_inline)) inline unsigned long atomic_increment_and_return(volatile unsigned long * obj) {
__attribute__ ((always_inline)) inline unsigned long atomic_increment_and_return(volatile unsigned long * obj) {
  unsigned long i = 1;
  asm volatile("lock; xadd %0, %1"
      : "+r" (i), "+m" (*obj)
      : : "memory");
  return i;
}

__attribute__ ((always_inline)) inline int handleAccessBefore(unsigned long addr, int isWrite, int typesize) {
  int result = 0;

  // We only care those acesses of global and heap.
  if(addr > MAX_USER_SPACE) {
    return 0;
  }

  unsigned long cindex = addr >> 6;
  unsigned long * cacheWrites = (unsigned long *)CACHE_STATUS_BASE;
  unsigned long * cacheTrackings = (unsigned long *)CACHE_TRACKING_BASE;

  unsigned long * status = (unsigned long *)&cacheWrites[cindex];
  unsigned long totalWrites = *status;
  if(totalWrites < THRESHOLD_TRACK_DETAILS) {
    // Only update writes
    if(isWrite == 1) {
      totalWrites = atomic_increment_and_return(status);
      if(totalWrites == INVOKE_DETAILED_TRACKING) {
        allocCacheTrack(cindex);
        result = 1;
      } 
    } 
  }
  else {
    // Update the access counter and we only calling the callback when it is necessary
    unsigned long  tracking = cacheTrackings[cindex];
    if(tracking != 0) {
      unsigned long * access = (unsigned long *)(tracking);
      *access += 1;
      if(*access%SAMPLE_INTERVAL <= SAMPLE_ACCESSES_EACH_INTERVAL) {
        handleAccess(addr, typesize>>3, isWrite);
      }
    }
  }

  return 1;
//  printf("hihi, it is a test with result %d isWrite %d address %lx\n", result, isWrite, addr);
}
#ifdef __cplusplus
}; /* end of extern "C" */
#endif

#endif
