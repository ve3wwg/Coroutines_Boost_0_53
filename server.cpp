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

static CoroutineBase *
sock_func(CoroutineBase *co) {
	SockCoro& sock_co = *(SockCoro*)co;
	std::stringstream hbuf;
	int sock = sock_co.socket();	
	std::size_t eoh=0, sob=0;
	int rc;

	printf("Sock func ran.. fd=%d, events %04X\n",sock,sock_co.get_events());
	char buf[1024];
	for (;;) {
		rc = ::read(sock,buf,sizeof buf);
		if ( rc < 0 ) {
			if ( errno == EWOULDBLOCK ) {
				sock_co.yield();
				printf("Sock fd=%d, events=%04X\n",sock,sock_co.get_events());
			} else	{
				printf("%s: read(fd=%d\n",strerror(errno),sock);
				break;
			}
		}
			
		printf("Read %d bytes from fd=%d\n",rc,sock);
		if ( rc == 0 )
			break;
		hbuf.write(buf,rc);

		// See if we have read the whole header yet

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

		if ( p )
			break;		// Read full header
	}

	// Extract header info:

	std::unordered_multimap<std::string,std::string> headers;

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
			while ( read_line() ) {
				printf("HDR: %s\n",buf);
				char *p = strchr(buf,':');
				if ( !p )
					continue;
				*p = 0;
				ucase(buf);
				headers.insert(std::pair<std::string,std::string>(buf,++p));
			}
		}

		for ( auto& pair : headers ) {
			printf("  %s :    %s\n",
				pair.first.c_str(),
				pair.second.c_str());
		}
	}

	printf("Closing fd=%d\n",sock);
	epco_ptr->del(sock);
	close(sock);
	return nullptr;
}

static CoroutineBase *
listen_func(CoroutineBase *co) {
	SockCoro& listen_co = *(SockCoro*)co;
	s_address addr;
	socklen_t addrlen = sizeof addr;
	int lsock = listen_co.socket();
	int fd;

	for (;;) {
		fd = ::accept4(lsock,&addr.addr,&addrlen,SOCK_NONBLOCK);
		if ( fd < 0 )
			listen_co.yield();

		printf("Accepted socket fd=%d\n",fd);
		epco_ptr->add(fd,EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR,
			new SockCoro(sock_func,fd));
	}

	return co;
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

	epco.run();

	return 0;
}

// End server.cpp
