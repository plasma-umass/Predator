// -*- C++ -*-

/*
  Copyright (c) 2012, University of Massachusetts Amherst.

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
 * @file   selfmap.h
 * @brief  Analyze the self mapping. 
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef _SELFMAP_H_
#define _SELFMAP_H_

#include <sys/resource.h>
#include <iostream>
#include <fstream>
#include <execinfo.h>

//#include <libunwind-ptrace.h>
//#include <sys/ptrace.h>

#include "xdefines.h"

#define MAX_BUF_SIZE 4096
using namespace std;

class selfmap {

public:
  static selfmap& getInstance (void) {
    static char buf[sizeof(selfmap)];
    static selfmap * theOneTrueObject = new (buf) selfmap();
    return *theOneTrueObject;
  }

  // We may use a inode attribute to analyze whether we need to do this.
  void getRegionInfo(std::string & curentry, void ** start, void ** end) {

    // Now this entry is about globals of libc.so.
    string::size_type pos = 0;
    string::size_type endpos = 0;
    // Get the starting address and end address of this entry.
    // Normally the entry will be like this
    // "00797000-00798000 rw-p ...."
    string beginstr, endstr;

    while(curentry[pos] != '-') pos++;

    beginstr = curentry.substr(0, pos);
  
    // Skip the '-' character
    pos++;
    endpos = pos;
    
    // Now pos should point to a space or '\t'.
    while(!isspace(curentry[pos])) pos++;
    endstr = curentry.substr(endpos, pos-endpos);

    // Save this entry to the passed regions.
    *start = (void *)strtoul(beginstr.c_str(), NULL, 16);
    *end = (void *)strtoul(endstr.c_str(), NULL, 16);

    return;
  }
  
  // Trying to get information about global regions. 
  void getTextRegions(void) {
    ifstream iMapfile;
    string curentry;

    try {
      iMapfile.open("/proc/self/maps");
    } catch(...) {
      PRDBG("can't open /proc/self/maps, exit now\n");
      abort();
    } 

    // Now we analyze each line of this maps file.
    void * startaddr, * endaddr;
    bool   checkedFirstEntry = false;

    bool   hasLibStart = false;
    while(getline(iMapfile, curentry)) {
      // Check whether this entry is the text segment of application.
      if((curentry.find(" r-xp ", 0) != string::npos) && checkedFirstEntry == false) {
        getRegionInfo(curentry, &startaddr, &endaddr);

        textStart = (unsigned long)startaddr;
        textEnd = (unsigned long)endaddr;
        checkedFirstEntry = true;
      }   
      // Check whether this is this library.
      else if((curentry.find(" r-xp ", 0) != string::npos) && curentry.find("libdefault", 0) != string::npos) {
        getRegionInfo(curentry, &startaddr, &endaddr);
        
        thislibStart = startaddr;
        thislibEnd = endaddr;
      }
    }
    iMapfile.close();

    int    count;

    /* Get current executable file name. */
    count= readlink("/proc/self/exe", filename, MAX_BUF_SIZE);
    if (count <= 0 || count >= MAX_BUF_SIZE)
    {
      PRDBG("Failed to get current executable file name\n" );
      exit(1);
    }
    filename[count] = '\0';

  //  fprintf(stderr, "INITIALIZATION: textStart %p textEnd %p libStart %p libEnd %p\n", textStart, textEnd, thislibStart, thislibEnd);
  } 

  bool isApplication(void * pcaddr) {
    if((intptr_t)pcaddr >= textStart && (intptr_t)pcaddr <= textEnd) {
      return true;
    }
    else {
      return false;
    }
  }

  bool isThisLibrary(void * pcaddr) {
    if(pcaddr >= thislibStart && pcaddr <= thislibEnd) {
      return true;
    }
    else {
      return false;
    }
  }


  /* Get the stack frame inside the tracer.
   * We can not simply use backtrace to get corresponding information
   * since it will give us the call stack of signal handler.
   * Not the watching processes that we need.
   * According to http://stackoverflow.com/questions/7258273/how-to-get-a-backtrace-like-gdb-using-only-ptrace-linux-x86-x86-64,
   * The following link can give us some clues to use libunwind.
    http://git.savannah.gnu.org/cgit/libunwind.git/plain/tests/test-ptrace.c?h=v1.0-stable
    http://git.savannah.gnu.org/cgit/libunwind.git/plain/tests/test-ptrace.c?h=v1.0-stable

   */
  // Print out the code information about an eipaddress
  // Also try to print out stack trace of given pcaddr.
  void printCallStack(ucontext_t * context, void * addr, bool isOverflow) {
    void * array[10];
    int size;
    int skip = 0;

    PRDBG("Try to get backtrace with array %p\n", array);
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    PRDBG("After get backtrace with array %p\n", array);

    for(int i = 0; i < size; i++) {
      if(isThisLibrary(array[i])) {
        skip++;
      } 
      else {
        break;
      }
    }

    backtrace_symbols_fd(&array[skip], size-skip, 2);

    // Print out the source code information if it is a overflow site.
    if(isOverflow) {
      PRDBG("\nSource code information about overflow site:\n");
      char buf[MAX_BUF_SIZE];

      for(int i = skip; i < size-skip; i++) {
        if(isApplication(array[i])) {
          PRDBG("callstack frame %d: ", i);
          // Print out the corresponding source code information
          sprintf(buf, "addr2line -e %s %lx", filename,  (unsigned long)array[i]-2);
          system(buf);
        }
      }
    }

    PRDBG("\n\n");
      // We don't care about the address in the libraries.
      
      // We must traverse back to find the address in the application.
//      PRDBG("pcaddr %p is a library address\n", pcaddr);   
     // backtrace_symbols_fd(length, frames, 2);
      //for(int i = 0; i < frames; i++) {
      //  PRDBG("i %d pc %p\n", i, buf[i]);
      //}
  }
 
  // Trying to get information about global regions. 
  void getGlobalRegion(void ** start, void ** end) {
    using namespace std;
    ifstream iMapfile;
    string curentry;

    //#define PAGE_ALIGN_DOWN(x) (((size_t) (x)) & ~xdefines::PAGE_SIZE_MASK)
    //void * globalstart = (void *)PAGE_ALIGN_DOWN(&__data_start);

    try {
      iMapfile.open("/proc/self/maps");
    } catch(...) {
      PRDBG("can't open /proc/self/maps, exit now\n");
      abort();
    } 

    // Now we analyze each line of this maps file.
    bool toExit = false;
    bool toSaveRegion = false;
 
    void * startaddr, * endaddr;
    string nextentry;
		int globalCount = 0;
    int prevRegionNumb = 0;

    bool foundGlobals = false;

    while(getline(iMapfile, curentry)) {
      // Check the globals for the application. It is at the first entry
      if(((curentry.find(" rw-p ", 0) != string::npos) || (curentry.find(" rwxp ", 0) != string::npos)) && foundGlobals == false) {
        // Now it is start of global of applications
        getRegionInfo(curentry, &startaddr, &endaddr);
  
        fprintf(stderr, "Initially, startaddr %p endaddr %p\n", startaddr, endaddr); 
        getline(iMapfile, nextentry);

        void * newstart;
        void * newend;
        // Check whether next entry should be also included or not.
        //if(nextentry.find("lib") == string::npos && (nextentry.find(" 08 ") != string::npos)) {
        if(nextentry.find("lib") == string::npos || (nextentry.find(" rw-p ") != string::npos)) {
//          fprintf(stderr, "Initially, nextentry.find %d string::npos %d\n", nextentry.find("lib"), string::npos); 
//          fprintf(stderr, "nextentry.now is  %s\n", nextentry.c_str()); 
          getRegionInfo(nextentry, &newstart, &newend);
//          fprintf(stderr, "Initially, startaddr %p endaddr %p\n", newstart, endaddr); 
        }
        *start = startaddr;
        if(newstart == endaddr) {
          *end = newend;
        }
        else {
          *end = endaddr;
        }
        foundGlobals = true;
        break;
      }
    }
    iMapfile.close();
  } 
  
  // Trying to get stack information. 
  void getStackInformation(void** stackBottom, void ** stackTop) {
    using namespace std;
    ifstream iMapfile;
    string curentry;

    try {
      iMapfile.open("/proc/self/maps");
    } catch(...) {
      PRDBG("can't open /proc/self/maps, exit now\n");
      abort();
    } 

    // Now we analyze each line of this maps file.
    while(getline(iMapfile, curentry)) {

      // Find the entry for stack information.
      if(curentry.find("[stack]", 0) != string::npos) {
        string::size_type pos = 0;
        string::size_type endpos = 0;
        // Get the starting address and end address of this entry.
        // Normally the entry will be like this
        // ffce9000-ffcfe000 rw-p 7ffffffe9000 00:00 0   [stack]
        string beginstr, endstr;

        while(curentry[pos] != '-') pos++;
          beginstr = curentry.substr(0, pos);
  
          // Skip the '-' character
          pos++;
          endpos = pos;
        
          // Now pos should point to a space or '\t'.
          while(!isspace(curentry[pos])) pos++;
          endstr = curentry.substr(endpos, pos-endpos);

          // Now we get the start address of stack and end address
          *stackBottom = (void *)strtoul(beginstr.c_str(), NULL, 16);
          *stackTop = (void *)strtoul(endstr.c_str(), NULL, 16);
      }
    }
    iMapfile.close();
  }

private:
  char filename[MAX_BUF_SIZE];
  void * thislibStart;
  void * thislibEnd;
};

#endif
