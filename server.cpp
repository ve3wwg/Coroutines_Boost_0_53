//////////////////////////////////////////////////////////////////////
// server.cpp -- Test EpollCoro http server.
// Date: Sat Aug 11 15:47:54 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "epollcoro.hpp"

EpollCoro *epco_ptr = nullptr;

static void
ucase(char *buf) {
	char ch;

	for ( ; (ch = *buf) != 0; ++buf ) 
		if ( ch >= 'a' && ch <= 'z' )
			*buf &= ~0x20;
}

//////////////////////////////////////////////////////////////////////
// HTTP Request Processor
//////////////////////////////////////////////////////////////////////

static CoroutineBase *
sock_func(CoroutineBase *co) {
	SockCoro& sock_co = *(SockCoro*)co;
	std::string reqtype, path, httpvers;
	std::stringstream hbuf, body;;
	std::unordered_multimap<std::string,std::string> headers;
	std::size_t content_length = 0;
	bool keep_alivef = false;				// True when we have Connection: Keep-Alive
	int sock = sock_co.socket();	
	std::size_t eoh=0, sob=0;
	char buf[1024];						// Careful to keep under boost::coroutines::stack_allocator::minimum_stacksize()
	int rc;

	//////////////////////////////////////////////////////////////
	// Lookup a header, return std::string
	//////////////////////////////////////////////////////////////

	auto get_header_str = [&headers](const char *what,std::string& v) -> bool {
		auto it = headers.find(what);
		if ( it == headers.end() )
			return false;				// Not found
		v.assign(it->second);
		return true;
	};

	//////////////////////////////////////////////////////////////
	// Lookup a header, return std::size_t value
	//////////////////////////////////////////////////////////////

	auto get_header_sz = [&get_header_str](const char *what,std::size_t& v) -> bool {
		std::string vstr;
		v = 0;
		if ( get_header_str(what,vstr) ) {
			v = strtoul(vstr.c_str(),nullptr,10);
			return true;
		}
		return false;
	};

	//////////////////////////////////////////////////////////////
	// Read from the socket until we block:
	//////////////////////////////////////////////////////////////

	auto read_sock = [sock,&buf,&sock_co]() -> int {
		int rc;	

		for (;;) {
			rc = ::read(sock,buf,sizeof buf);
			if ( rc >= 0 )
				return rc;
			if ( errno == EWOULDBLOCK )
				sock_co.yield();	// Yield to EpollCoro
			else if ( errno != EINTR ) {
				printf("ERROR, %s: read(fd=%d\n",strerror(errno),sock);
				return rc;
			}
		}
	};

	printf("Sock func ran.. fd=%d, events %04X\n",sock,sock_co.get_events());

	for (;;) {
		rc = read_sock();
		rc = ::read(sock,buf,sizeof buf);
		if ( rc < 0 )
			break;				// Fatal error
			
		printf("READ %d bytes from fd=%d\n",rc,sock);
		if ( rc == 0 )
			break;				// EOF
		hbuf.write(buf,rc);

		//////////////////////////////////////////////////////
		// Check if we have read the entire header
		//////////////////////////////////////////////////////

		std::string flattened(hbuf.str());
		const char *fp = flattened.c_str();

		const char *p = strstr(fp,"\r\n\r\n");
		if ( !p ) {
			p = strstr(fp,"\n\n");
			if ( p ) {
				eoh = p - fp;
				sob = eoh + 2;
			}
		} else	{
			eoh = p - fp;
			sob = eoh + 4;
		}

		if ( p ) {
			std::size_t sz = flattened.size();
			if ( sob < sz ) {
				// Copy excess header to body:
				sz -= sob;
				body.write(flattened.c_str()+sob,sz);
			}
			break;		// Read full header
		}
	}

	//////////////////////////////////////////////////////////////
	// Extract HTTP header:
	//////////////////////////////////////////////////////////////

	{
		hbuf.seekg(0);
		char buf[4096];

		auto read_line = [&]() -> bool {
			hbuf.getline(buf,sizeof buf-1);
			buf[sizeof buf-1] = 0;

			size_t sz = strcspn(buf,"\r\n");
			buf[sz] = 0;
			if ( !*buf )
				return false;
			return !hbuf.fail();
		};

		if ( read_line() ) {
			unsigned ux = strcspn(buf," \t\b");
			char *p;

			reqtype.assign(buf,ux);
			p = buf + ux;
			p += strspn(p," \t\b");
			ux = strcspn(p," \t\b");
			path.assign(p,ux);
			p += ux;
			p += strspn(p," \t\b");
			httpvers.assign(p,strcspn(p," \t\b"));

			while ( read_line() ) {
				printf("HDR: %s\n",buf);
				char *p = strchr(buf,':');
				if ( p )
					*p = 0;
				ucase(buf);
				if ( p ) {
					++p;
printf("Header value = '%s'\n",p);
					p += strspn(p," \t\b");
printf("Header value now = '%s' (for '%s')\n",p,buf);
					std::size_t sz = strcspn(p," \t\b");	// Check for trailing whitespace
					std::string trimmed(p,sz);		// Trimmed value

					headers.insert(std::pair<std::string,std::string>(buf,trimmed));
				} else	headers.insert(std::pair<std::string,std::string>(buf,"")); 
			}
		}

printf("reqtype='%s', path='%s', httpvers='%s'\n",reqtype.c_str(),path.c_str(),httpvers.c_str());

		//////////////////////////////////////////////////////
		// Determine content-length
		//////////////////////////////////////////////////////
		
		get_header_sz("CONTENT-LENGTH",content_length);

		//////////////////////////////////////////////////////
		// Check if we have Connection: Keep-Alive
		//////////////////////////////////////////////////////
		{
			std::string keep_alive;

			if ( get_header_str("CONNECTION",keep_alive) )
				keep_alivef = !strcasecmp(keep_alive.c_str(),"Keep-Alive");
		}
	}

printf("Content-Length = %zu, ka=%d\n",content_length,keep_alivef);

	printf("Closing fd=%d\n",sock);
	epco_ptr->del(sock);
	close(sock);
printf("Exiting SockCoro for fd=%d\n",sock);

	sock_co.yield_with(nullptr);
	assert(0);
	return nullptr;
}

//////////////////////////////////////////////////////////////////////
// Listen coroutine
//////////////////////////////////////////////////////////////////////

static CoroutineBase *
listen_func(CoroutineBase *co) {
	SockCoro& listen_co = *(SockCoro*)co;
	s_address addr;
	socklen_t addrlen = sizeof addr;
	int lsock = listen_co.socket();
	int fd;

	for (;;) {
		fd = ::accept4(lsock,&addr.addr,&addrlen,SOCK_NONBLOCK);
		if ( fd < 0 ) {
			listen_co.yield();	// Yield to Epoll
		} else	{
			printf("ACCEPTED SOCKET fd=%d\n",fd);
			epco_ptr->add(fd,EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR,
				new SockCoro(sock_func,fd));
		}
	}

	return nullptr;
}

int
main(int argc,char **argv) {
	EpollCoro epco;
	int port = 2345, backlog = 50;

	epco_ptr = &epco;

	auto add_listen_port = [&](const char *straddr) {
		s_address addr;
		int lfd = -1;
		bool bf;

		EpollCoro::import_ip(straddr,addr);
		lfd = EpollCoro::listen(addr,port,backlog);
		assert(lfd >= 0);
		bf = epco.add(lfd,EPOLLIN,new SockCoro(listen_func,lfd));
		assert(bf);
	};

	if ( !argv[1] ) {
		add_listen_port("127.0.0.1");
	} else	{
		for ( auto x=1; x<argc; ++x )
			add_listen_port(argv[x]);
	}

	printf("Min stack size: %zu\n",boost::coroutines::stack_allocator::minimum_stacksize());

	epco.run();
	printf("EpollCoro returned.\n");

	return 0;
}

// End server.cpp
