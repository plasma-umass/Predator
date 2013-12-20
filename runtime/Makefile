SOURCE_DIR = .
INCLUDE_DIR = .

#SOURCE_DIR = source
#INCLUDE_DIR = include

SRCS =  $(SOURCE_DIR)/libdefault.cpp \
	$(SOURCE_DIR)/libfuncs.cpp         \
	$(SOURCE_DIR)/dlmalloc.c         \
	$(SOURCE_DIR)/finetime.c  

INCS =  $(INCLUDE_DIR)/xdefines.h     \
	$(INCLUDE_DIR)/selfmap.h         \
	$(INCLUDE_DIR)/xrun.h         \
	$(INCLUDE_DIR)/xmemory.h         \
	$(INCLUDE_DIR)/xmapping.h         \
	$(INCLUDE_DIR)/objectheader.h \
	$(INCLUDE_DIR)/xoneheap.h     \
	$(INCLUDE_DIR)/xpheap.h \
	$(INCLUDE_DIR)/callsite.h \
	$(INCLUDE_DIR)/report.h \
	$(INCLUDE_DIR)/cachetrack.h \
	$(INCLUDE_DIR)/privateheap.h \
	$(INCLUDE_DIR)/atomic.h       \
	$(INCLUDE_DIR)/elfinfo.h      \
	$(INCLUDE_DIR)/xthread.h      \
	$(INCLUDE_DIR)/finetime.h     \
	$(INCLUDE_DIR)/mm.h

DEPS = $(SRCS) $(INCS)

CXX = g++ -g -O2 -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/detect -I$(INCLUDE_DIR)/heap -I$(INCLUDE_DIR)/util -I$(INCLUDE_DIR)/sync

# Detection on 32bit
# CXX = g++ -DSSE_SUPPORT -m32 -DX86_32BIT -O3 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING
# Detection on 64bit
#CXX = g++ -DSSE_SUPPORT -m64 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING


# -march=core2 -msse3 -DSSE_SUPPORT 
#CFLAGS   = -Wall -msse3 -DSSE_SUPPORT -fno-omit-frame-pointer
#CFLAGS   = -fno-omit-frame-pointer -DPREDICTION
CFLAGS   = -fno-omit-frame-pointer -DPREDICTION -DCENTRALIZED_BIG_HEAP -DUSING_SIGUSR2
#-DOUTPUT_WORD_ACCESSES
#CFLAGS   = -fno-omit-frame-pointer -DPREDICTION -DOUTPUT_WORD_ACCESSES
#-DIMPROVE_PERF 
#-DOUTPUT_WORD_ACCESSES
#CFLAGS   = -fno-omit-frame-pointer 
CFLAGS32 = $(CFLAGS) -m32 -DX86_32BIT # -O3
CFLAGS64 = $(CFLAGS) #-m64 # -O3
#CFLAGS64 = $(CFLAGS) -m64 # -O3

INCLUDE_DIRS = -I. -I./heaplayers -I./heaplayers/util

#GET_CHARACTERISTICS

TARGETS = libdefault64.so
#TARGETS = libdefault32.so libdefault64.so

all: $(TARGETS)

libdefault32.so: $(DEPS)
	$(CXX) $(CFLAGS32) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=default_##x' $(SRCS) -o libdefault32.so  -ldl -lpthread

libdefault64.so: $(DEPS)
	$(CXX) $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=default_##x' $(SRCS) -o libdefault64.so  -ldl -lpthread

clean:
	rm -f $(TARGETS)

