all:
	rm -f thread_memtest.bc thread_memtest-afterinstrument.bc thread_memtest-afterinstrument.ll
#	clang -Wl,/home/tongping/work/falsesharing/project/runtime/libdefault64.so -g -O2 thread_memtest.c -lpthread -o thread_memtest
	clang -Wl,/nfs/cm/scratch1/tonyliu/grace/branches/falsesharing/project/runtime/libdefault64.so -finstrumenter -g -O0 thread_memtest.c -lpthread -o thread_memtest
#	clang -g -O2 thread_memtest.c -lpthread -o thread_memtest-pthread

clean:
	rm -f thread_memtest.native thread_memtestnew.bc thread_memtest.bc thread_memtest-afterinstrument.bc thread_memtest-afterinstrument.ll thread_memtest.ll thread_memtest.s
#	gcc thread_memtest.s -Llibrary -lsheriff -o thread_memtest.native
#	clang -O0 -emit-llvm thread_memtest.c -S -o thread_memtest.ll
