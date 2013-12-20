// -*- C++ -*-

#ifndef _REPORT_H_
#define _REPORT_H_

#include <set>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xdefines.h"
#include "mm.h"
#include "objectinfo.h"
#include "objectheader.h"
#include "elfinfo.h"
#include "callsite.h"
#include "cachetrack.h"

class report {
  enum { MAXBUFSIZE = 4096 };
public:

  report() {}

  void initialize(bool isHeap, void * start, void * end, void * cacheWrites, cachetrack ** cacheTrackings) 
  {
    int    count;
    void * ptr;

    if(start == NULL) {
      fprintf(stderr, "start is 0\n");
      while(1) ;
    }
    assert(start != NULL);
    assert(end != NULL);
    assert(cacheWrites != NULL);

    fprintf(stderr, "reporting start %p end %p cacheWrites %p cacheTrackings %p\n", start, end, cacheWrites, cacheTrackings);

    _isHeap = isHeap;
    _start = (char *)start;
    _end = (char *)end;
    _cacheWrites = (unsigned long *)cacheWrites;  
    _cacheTrackings = cacheTrackings; 
     
    if(_start > _end) {
      fprintf(stderr, "Start %p end %p\n", _start, _end);
    } 
    assert(_start <= _end);

    /* Get current executable file name. */
    count = readlink("/proc/self/exe", _curFilename, MAXBUFSIZE);
    if (count <= 0 || count >= MAXBUFSIZE)
    {
      fprintf(stderr, "Failed to get current executable file name\n" );
      exit(1);
    }
    _curFilename[count] = '\0';

    /* Get text segmentations information. */
    _elfInfo.hdr = (Elf_Ehdr*)mapImage(_curFilename, &_elfInfo.size);
    if (!_elfInfo.hdr) {
      fprintf(stderr, "Can't grab file %s\n", _curFilename);
      exit(1);
    }
  
    parseElfFile();
  }

   ~report() {
  }


  // Mapping the specified elf file to current process and return the starting address.
  void * mapImage(const char *filename, unsigned long *size) {
    struct stat st;
    void *map;
    int fd;

    fd = open(filename, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0)
      return NULL;

    *size = st.st_size;

    // Mapping current image to 
    map = MM::mmapAllocatePrivate(*size, NULL, fd);
    close(fd);

    if (map == MAP_FAILED)
      return NULL;
    return map;
  }

  // Verify the elf file is an valid elf file.
  bool isValidElfFile(struct elf_info * _elfInfo) 
  {
    Elf_Ehdr *hdr = _elfInfo->hdr;

    if (_elfInfo->size < sizeof(*hdr)) {
        /* file too small, assume this is an empty .o file */
        return false;
    }
    
    /* Is this a valid ELF file? */
    if ((hdr->e_ident[EI_MAG0] != ELFMAG0) ||
       (hdr->e_ident[EI_MAG1] != ELFMAG1) ||
       (hdr->e_ident[EI_MAG2] != ELFMAG2) ||
       (hdr->e_ident[EI_MAG3] != ELFMAG3)) {
      /* Not an ELF file - silently ignore it */
      return false;
   }

   return true;
  }

  int parseElfFile()
  {
    unsigned int i;
    Elf_Ehdr *hdr = _elfInfo.hdr;
    Elf_Shdr *sechdrs;
  
    if(!isValidElfFile(&_elfInfo)) {
      fprintf (stderr, "Erroneous ELF file.\n");
      exit(-1);
    }
  

    sechdrs =(Elf_Shdr *)((char *)hdr + hdr->e_shoff);
    _elfInfo.sechdrs = sechdrs;

    /* Fix endianness in section headers */
    for (i = 0; i < hdr->e_shnum; i++) {
      if (sechdrs[i].sh_type != SHT_SYMTAB)
        continue;
  
      if(sechdrs[i].sh_size == 0) 
        continue;
  
      _elfInfo.symtab_start = (Elf_Sym*)((unsigned long)hdr + sechdrs[i].sh_offset);
      _elfInfo.symtab_stop  = (Elf_Sym*)((unsigned long)hdr + sechdrs[i].sh_offset + sechdrs[i].sh_size);
      _elfInfo.strtab       = (char *)((unsigned long)hdr + sechdrs[sechdrs[i].sh_link].sh_offset);
      break;
    }
  
    return 0; 
  }

  /*
   * Find symbols according to specific address.  
   */
  Elf_Sym * find_symbol(struct elf_info *elf, Elf_Addr addr)
  {
    int bFound = 0;
    Elf_Sym *symbol;
    Elf_Ehdr *hdr = elf->hdr;
  
    for (symbol = elf->symtab_start; symbol < elf->symtab_stop; symbol++) {
      if (symbol->st_shndx >= SHN_LORESERVE)
        continue;
      
      /* Checked only when it is a global variable. */
      if(ELF_ST_BIND(symbol->st_info) != STB_GLOBAL || ELF_ST_TYPE(symbol->st_info) != STT_OBJECT)
        continue;
  
      /* If the addr is in the range of current symbol, that get it. */
      if(addr >= symbol->st_value && addr < (symbol->st_value + symbol->st_size)) {
        bFound = 1;
        break;
      }
    }
  
    if(bFound)
      return symbol;
    else
      return NULL;
  }

  void reportFalseSharing(void) {
    if(_isHeap) {
      reportHeapObjects();
    }
    else {
      reportGlobalObjects();
    }
  }
  
  
  inline bool hasFSSuspect(unsigned long * status) {
    return (*status > xdefines::THRESHOLD_TRACK_DETAILS);
  }

  inline bool isFSSuspect(unsigned long writes) {
    return writes > xdefines::THRESHOLD_TRACK_DETAILS ? true : false;
  }

  
  inline struct wordinfo * allocWordinfo(int words) {
    struct wordinfo * winfo = (struct wordinfo *)InternalHeap::getInstance().malloc(sizeof(struct wordinfo) * words);
 //   fprintf(stderr, "allocWordinfo %p\n", winfo); 
    if(winfo != NULL) {
      memset(winfo, 0, sizeof(struct wordinfo) * words);
    }

    return winfo;
  }

  //inline bool getCacheInvalidations(unsigned long * cacheWrites, int lines, ObjectInfo * object) {
  inline bool getCacheInvalidations(unsigned long cacheindex, int lines, ObjectInfo * object) {
    bool hasFS = false;
    bool hasPotentialFS = false;
    unsigned long firstOffset = getCachelineOffset((unsigned long)object->start);
    unsigned long lastOffset = getCachelineOffset((unsigned long)object->stop);
    cachetrack * track = NULL;
    struct wordinfo * winfo = NULL;
    int windex = 0;
  
    object->winfo = NULL;
    object->invalidations = 0;
    object->totalWrites = 0;
    object->totalAccesses = 0;

    assert(firstOffset < CACHE_LINE_SIZE);

    // Traverse all related cache lines.
    for(unsigned long i = cacheindex; i < (cacheindex+lines); i++) {
      unsigned long index;
      unsigned long words;

      if(lines == 1) {
        index = firstOffset/xdefines::WORD_SIZE;
        // Justify the last offset, it should not be 0 since it can point to last bytes of a cache line. 
        if(lastOffset == 0) {
          lastOffset = CACHE_LINE_SIZE;
        }
        words = (lastOffset - firstOffset)/xdefines::WORD_SIZE;
      }
      else {
        if(i == cacheindex) {
          index = firstOffset/xdefines::WORD_SIZE;
          words = (CACHE_LINE_SIZE-firstOffset)/xdefines::WORD_SIZE; 
        }
        else if(i == cacheindex + lines - 1) {
          index = 0;
          words = lastOffset/xdefines::WORD_SIZE;
        }
        else {
          index = 0;
          words = CACHE_LINE_SIZE/xdefines::WORD_SIZE;
        }
      }
      
      // We only have false sharing when the cache status is larger than predefined threshold
      // fprintf(stderr, "cache %d: cacheWrites is %ld  at %p\n", i, _cacheWrites[i], &_cacheWrites[i]);
      if(_cacheWrites[i] >= xdefines::THRESHOLD_TRACK_DETAILS) {
        track = (cachetrack *)_cacheTrackings[i];

        if(track == NULL) {
          continue;
        }

        if(track->getInvalidations() > 0) {
          fprintf(stderr, "cache %d: cacheWrites is %ld invalidations %lx at %p\n", i, _cacheWrites[i], track->getInvalidations(), track);
           
          track->reportFalseSharing();
        }
        
        if(winfo == NULL) {
          winfo = allocWordinfo(object->words);
        }

        memcpy(&winfo[windex], track->getWordinfoAddr(index), words * sizeof(struct wordinfo));

        object->totalWrites += track->getWrites();
        if(track->getInvalidations() > 0) { 
          object->invalidations += track->getInvalidations();
        }

        object->totalAccesses += track->getAccesses();
      }
   
      
      windex += words;
    }
 
    if(winfo) {
      object->winfo = (void *)winfo;
    }
   
    return hasFS; 
  }

  inline size_t getCachelineOffset(unsigned long addr) {
    return (addr - getCacheStart((void *)addr));
  }

  // Search a global object with 
  int reportHeapObjects(void) {
    unsigned long memstart = (unsigned long)_start;
    unsigned long * memend = (unsigned long *)_end; 
    unsigned long * pos = (unsigned long *)_start;

    //fprintf(stderr, "reportHeapObjects pos %p memend %p\n", pos, memend); 
    while(pos < memend) {
      // We are tracking word-by-word to find the objec theader.  
      if(*pos == objectHeader::MAGIC) {
        objectHeader * object = (objectHeader *)pos;
        unsigned long  objectStart = (unsigned long)object->getObjectStart();
        unsigned long  objectOffset = objectStart;
        unsigned long  objectEnd = (unsigned long)object->getObjectEnd();
        unsigned long  cacheStart = getCacheStart((void *)objectStart);
        
     //   fprintf(stderr, "reportHeapObjects pos %p objectStart %lx objectEnd %lx\n", pos, objectStart, objectEnd); 
        // Calculate how many cache lines are occupied by this object.
        int   cachelineIndex = getCacheline(objectOffset); 
        int   lines = getCoveredCachelines(cacheStart, objectEnd);
 
        // Check whether there are some invalidations on this object.
        bool hasFS = false;
        ObjectInfo objectinfo;
        objectinfo.isHeapObject = true;
        objectinfo.winfo = NULL;
        objectinfo.unitlength = object->getSize();
        objectinfo.words = objectinfo.unitlength/xdefines::WORD_SIZE;
        objectinfo.start = (unsigned long *)objectStart;
        objectinfo.stop = (unsigned long *)objectEnd;

  //      fprintf(stderr, "getCacheInvalidation object start %lx stop %lx lines %x unitlength %lx words %lx\n", objectStart, objectEnd, lines, object->getSize(), objectinfo.words);

      //  fprintf(stderr, "reportHeapObjects pos %p objectStart %lx objectEnd %lx checking invalidations\n", pos, objectStart, objectEnd); 
        hasFS = getCacheInvalidations(cachelineIndex, lines, &objectinfo);
        //fprintf(stderr, "reportHeapObjects pos %p objectStart %lx objectEnd %lx invalidations %lx\n", pos, objectStart, objectEnd, objectinfo.invalidations); 
    //    fprintf(stderr, "after getCacheInvalidation object start %lx stop %lx lines %x unitlength %lx words %lx\n", objectStart, objectEnd, lines, object->getSize(), objectinfo.words);
 
        // Whenever interleaved writes is larger than the specified threshold
        if(objectinfo.invalidations > xdefines::THRESHOLD_REPORT_INVALIDATIONS) {
          // Save object information.
          memcpy((void *)&objectinfo.u.callsite, object->getCallsiteAddr(), sizeof(CallSite));
          reportFalseSharingObject(&objectinfo);
        }
        
        if(objectinfo.winfo) {
         // fprintf(stderr, "objectinfo.winfo %p before free\n", objectinfo.winfo);
          InternalHeap::getInstance().free(objectinfo.winfo);
        }
          
        pos = (unsigned long *)objectEnd;
        continue;
      } 
      else {
        pos++;
      }
    }
  }

  int reportGlobalObjects(void) {
    struct elf_info *elf = &_elfInfo;  
    Elf_Ehdr *hdr = elf->hdr;
    Elf_Sym *symbol;

   fprintf(stderr, "reportGlobalObjects now. globalstart %lx globalend %lx\n", globalStart, globalEnd);
    for (symbol = elf->symtab_start; symbol < elf->symtab_stop; symbol++) {
      if(symbol == NULL) {
        continue;
      }

      if (symbol->st_shndx >= SHN_LORESERVE)
        continue;

      /* Checked only when it is a global variable. */
      if(ELF_ST_BIND(symbol->st_info) != STB_GLOBAL || ELF_ST_TYPE(symbol->st_info) != STT_OBJECT)
        continue;

      // Now current symbol is one normal object. 
      unsigned long objectStart = symbol->st_value;
      
      size_t objectSize = symbol->st_size;

      size_t objectOffset = objectStart;
      unsigned long objectEnd = objectStart + objectSize;
  
      //if(objectStart < globalStart || objectStart > globalEnd || objectEnd > globalEnd) {
      if(objectStart < globalStart || objectStart > globalEnd) {
        continue;
      }

      unsigned long cacheStart = getCacheStart((void *)objectStart);
      int   cachelineIndex = getCacheline(objectOffset);
      unsigned long lines = getCoveredCachelines(cacheStart, objectStart + objectSize);

      // Check whether there are some invalidations on this object.
      bool hasFS = false;
      ObjectInfo objectinfo;
      objectinfo.isHeapObject = false;
      objectinfo.unitlength = objectSize;
      objectinfo.words = objectSize/xdefines::WORD_SIZE;
      objectinfo.start = (unsigned long *)objectStart;
      objectinfo.stop = (unsigned long *)objectEnd;
         
      hasFS = getCacheInvalidations(cachelineIndex, lines, &objectinfo);

      // For globals, only when we need to output this object then we need to store that.
      // Since there is no accumulation for global objects.
      if(objectinfo.invalidations > xdefines::THRESHOLD_REPORT_INVALIDATIONS) {
        // Check how many objects are located in the last cache line.
        objectinfo.u.symbol = (void *)symbol;
        reportFalseSharingObject(&objectinfo);
      }
        
      if(objectinfo.winfo) {
        InternalHeap::getInstance().free(objectinfo.winfo);
      }
    }
  }

  inline unsigned long getCachelineIndex(unsigned long addr) {
    return (addr >> CACHE_LINE_SIZE_SHIFTS);
  }


  // Print out the detail object information about false sharing objects. 
  void reportFalseSharingObject(ObjectInfo * object) {
    char buf[MAXBUFSIZE];
    int ret; 
    int words = object->unitlength/xdefines::WORD_SIZE;
    int size; 

//    fprintf(stderr, "filename %s\n", _curFilename);
    if(object->isHeapObject) { 
      fprintf(stderr, "FALSE SHARING HEAP OBJECT: start %p end %p (with size %lx). Accesses %lx invalidations %lx writes %lx. Callsite stack:\n", object->start, object->stop, object->unitlength, object->totalAccesses, object->invalidations, object->totalWrites);
      for(int i = 0; i < CALL_SITE_DEPTH; i++) {
        if(object->u.callsite[i] != 0) {
          sprintf(buf, "addr2line -i -e %s %lx", _curFilename, object->u.callsite[i]);
//          fprintf(stderr, "callsite: %lx\n", object->u.callsite[i]);
          ret = system(buf);
        }
      }
    }
    else {
      fprintf(stderr, "FALSE SHARING GLOBAL VARIABLE: start %p end %p (with size %lx). Accesses %lx invalidations %lx writes %lx. \n", object->start, object->stop, object->unitlength, object->totalAccesses, object->invalidations, object->totalWrites);
      // Print object information about globals.
      Elf_Sym *symbol = (Elf_Sym *)object->u.symbol;
//find_symbol(&_elfInfo, (intptr_t)object->start);
      if(symbol != NULL) {
        const char * symname = _elfInfo.strtab + symbol->st_name;
        fprintf(stderr, "\tGlobal object: name \"%s\", start %lx, size %ld\n", symname, symbol->st_value, symbol->st_size);
      }
    }

    size = object->unitlength;
   
    #ifdef OUTPUT_WORD_ACCESSES
    fprintf(stderr, "\n");
    int * address = (int *)object->start;
    struct wordinfo * winfo = (struct wordinfo *) object->winfo;
    for(int i = 0; i < size/xdefines::WORD_SIZE; i++) {
      if((winfo[i].reads + winfo[i].writes) != 0) {
        //fprintf(stderr, "\tWord %d: at address %p (line %d), reads %d writes %d ", i, address, winfo[i].reads, winfo[i].writes); 
        fprintf(stderr, "\tAddress %p (line %ld): reads %d writes %d ", address, getCachelineIndex((intptr_t)address), winfo[i].reads, winfo[i].writes); 
        if(winfo[i].tindex == cachetrack::WORD_THREAD_SHARED) {
          fprintf(stderr, "by mulitple thread\n");
        }
        else {
          fprintf(stderr, "by thread %d\n", winfo[i].tindex);
        } 
      }
      address++;
    }
    #endif 
    fprintf(stderr, "\n\n");
  }

private:

  struct elf_info _elfInfo;

  // Profiling type.
  bool _isHeap;

  char * _start; 
  char * _end;
  unsigned long * _cacheWrites;
  cachetrack ** _cacheTrackings;
  
  char _curFilename[MAXBUFSIZE];
};


#endif
