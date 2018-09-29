#BOOST	?= /usr/local
BOOST	?= $(HOME)/local
CXX	= g++
STD	= -std=c++11

CXXFLAGS= -Wall $(STD) -I$(INCL) -g -c -Wno-unused-local-typedefs -Wno-memset-transposed-args

INCL	= $(BOOST)/include
LIBS	= $(BOOST)/lib

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $*.o

all:	boost_check coroutine server

OBJS	= scheduler.o server.o sockets.o httpbuf.o iobuf.o utils.o

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

server:	$(OBJS)
	$(CXX) $(OBJS) -L$(LIBS) -lboost_context -dl -o server -Wl,-rpath=$(LIBS)

clean:
	rm -f *.o

clobber: clean
	rm -f coroutine .errs.t core

test:
	wget 'http://127.0.0.1:2345/some/path?var=1&var=2' --save-headers --method=POST --body-data='Some body data..' -qO - </dev/null 2>&1
