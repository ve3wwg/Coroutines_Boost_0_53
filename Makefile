#BOOST	?= /usr/local
BOOST	?= $(HOME)/local
CXX	= g++
STD	= -std=c++11

CXXFLAGS= -Wall $(STD) -I$(INCL) -g -c -Wno-unused-local-typedefs -Wno-memset-transposed-args

INCL	= $(BOOST)/include
LIBS	= $(BOOST)/lib

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $*.o

all:	coroutine server

OBJS	= scheduler.o server.o sockets.o httpbuf.o iobuf.o utility.o

coroutine.o: coroutine.hpp

coroutine: coroutine.o
	$(CXX) coroutine.o -L$(LIBS) -lboost_context -dl -o coroutine -Wl,-rpath=$(LIBS)

server:	$(OBJS)
	$(CXX) $(OBJS) -L$(LIBS) -lboost_context -dl -o server -Wl,-rpath=$(LIBS)

clean:
	rm -f *.o

clobber: clean
	rm -f coroutine .errs.t core core.*

test:
#	wget --save-headers --method=POST --body-data='Some body data..' -qO - 'http://127.0.0.1:2345/some/path?var=1&var=2' </dev/null 2>&1
	wget --save-headers --post-data='Some body data..' -qO - 'http://127.0.0.1:2345/some/path?var=1&var=2' </dev/null 2>&1

chunked:
	wget --save-headers --post-data=$$'4\r\nWiki\r\n5\r\npedia\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n' -qO - \
		--header='Transfer-Encoding: chunked' \
		'http://127.0.0.1:2345/some/path?var=1&var=2' </dev/null 2>&1

xchunked:
	wget --save-headers --post-data=$$'4\r\nWiki\r\n5\r\npedia\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\nX-Exten1: One\r\nX=Extem2: two\r\n\r\n' -qO - \
		--header='Transfer-Encoding: chunked' \
		'http://127.0.0.1:2345/some/path?var=1&var=2' </dev/null 2>&1

