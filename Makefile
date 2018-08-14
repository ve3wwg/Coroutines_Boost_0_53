BOOST	?= /usr/local
CXX	= g++
STD	= -std=c++11

CXXFLAGS= -Wall $(STD) -I$(INCL) -g -c -Wno-unused-local-typedefs -Wno-memset-transposed-args

INCL	= $(BOOST)/include
LIBS	= $(BOOST)/lib

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $*.o

all:	boost_check coroutine server

boost_check:
	@if [ ! -d $(BOOST)/. ] ; then \
		echo "export or supply BOOST=<boost_1.53.0_dir>" ; \
		exit 1; \
	fi
	@if [ ! -f $(BOOST)/lib/libboost_context.so.1.53.0 ] ; then \
		echo "Boost library libboost_context.so.1.53.0 (version 1.53.0) required." ; \
		exit 1; \
	fi

coroutine.o: coroutine.hpp

coroutine: coroutine.o
	$(CXX) coroutine.o -L$(LIBS) -lboost_context -dl -o coroutine -Wl,-rpath=$(LIBS)

server:	epollcoro.o server.o
	$(CXX) epollcoro.o server.o -L$(LIBS) -lboost_context -dl -o server -Wl,-rpath=$(LIBS)


clean:
	rm -f *.o

clobber: clean
	rm -f coroutine .errs.t core
