BOOST	?= /usr/local
CXX	= g++
STD	= -std=c++11

CXXFLAGS= -Wall $(STD) -I$(INCL) -c -Wno-unused-local-typedefs -Wno-memset-transposed-args

INCL	= $(BOOST)/include
LIBS	= $(BOOST)/lib

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $*.o

all:	check coroutine

check:
	@if [ ! -f $(BOOST)/lib/libboost_context.so.1.53.0 ] ; then \
		echo "Boost library libboost_context.so.1.53.0 (version 1.53.0) required." ; \
	fi

coroutine.o: coroutine.hpp

coroutine: coroutine.o
	$(CXX) coroutine.o -L$(LIBS) -lboost_context -dl -o coroutine -Wl,-rpath=$(LIBS)

clean:
	rm -f *.o

clobber: clean
	rm -f coroutine .errs.t
