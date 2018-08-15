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

static const char html_endl[] = "\r\n";

//////////////////////////////////////////////////////////////////////
// Uppercase in place:
//////////////////////////////////////////////////////////////////////

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
	SockCoro& sock_co = *dynamic_cast<SockCoro*>(co);
	EpollCoro& epco = *dynamic_cast<EpollCoro*>(sock_co.get_caller());
	const int sock = sock_co.socket();	
	Events evt(EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR);
	std::string reqtype, path, httpvers;
	std::stringstream hbuf, body;
	std::stringstream rhdr, rbody;
	std::unordered_multimap<std::string,std::string> headers;
	std::size_t content_length = 0;
	bool keep_alivef = false;				// True when we have Connection: Keep-Alive
	std::size_t eoh=0, sob=0;
	char buf[4096];						// Careful to keep under boost::coroutines::stack_allocator::minimum_stacksize()
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
	// Terminate the SockCoro processing. WHen EpollCoro recieves
	// a nullptr, it knows to destroy this coroutine.
	//////////////////////////////////////////////////////////////

	auto exit_coroutine = [&]() {
		printf("Exit coroutine sock=%d\n",sock);
		epco.del(sock);			// Remove our socket from Epoll
		close(sock);			// Close the socket
		sock_co.yield_with(nullptr);	// Tell Epoll to drop us
		assert(0);			// Should never get here..
	};

	//////////////////////////////////////////////////////////////
	// When yielding to Epoll, update the event flags. After
	// resuming from Epoll, disable the flags like EPOLLRDHUP
	// that we now know about.
	//////////////////////////////////////////////////////////////

	auto epoll_yield = [&]() {
		if ( evt.update() )		// Event changes?
			epco.chg(sock,evt.state()); // Yes, update Epoll events
		sock_co.yield();		// Yield to Epoll
		evt.disable(sock_co.get_flags()); // Disable flags we received
	};

	//////////////////////////////////////////////////////////////
	// Read from the socket until we block:
	//////////////////////////////////////////////////////////////

	auto read_sock = [&](size_t max=0) -> int {
		int rc;	

		if ( max <= 0 )
			max = sizeof buf;
		else if ( max > sizeof buf )
			max = sizeof buf;

		for (;;) {
			rc = ::read(sock,buf,max);
			if ( rc >= 0 )
				return rc;
			if ( errno == EWOULDBLOCK ) {
				epoll_yield();
			} else if ( errno != EINTR ) {
				printf("ERROR, %s: read(fd=%d\n",strerror(errno),sock);
				return rc;
			}
		}
	};

	auto write_sock = [&](std::stringstream& s) {
		std::string flattened(s.str());
		const char *p = flattened.c_str();
		std::size_t spos = 0, sz = flattened.size();
		int rc;

		while ( spos < sz ) {
			rc = ::write(sock,p+spos,sz);
			if ( rc < 0 ) {
				if ( errno == EWOULDBLOCK )
					epoll_yield();
				else if ( errno == EINTR )
					continue;
				else	{
					exit_coroutine(); // Fatal error
				}
			} else	{
				spos += rc;
				sz -= rc;
			}
		}
	};

	//////////////////////////////////////////////////////////////
	// Keep-Alive Loop:
	//////////////////////////////////////////////////////////////

	for (;;) {
		hbuf.str("");
		hbuf.clear();
		body.str("");
		body.clear();
		rhdr.str("");
		rhdr.clear();
		rbody.str("");
		rbody.clear();

		reqtype.clear();
		path.clear();
		httpvers.clear();
		headers.clear();
		content_length = 0;
		keep_alivef = false;
		eoh = sob = 0;

		printf("Expecting request from sock=%d\n",sock);

		//////////////////////////////////////////////////////
		// Read loop for headers:
		//////////////////////////////////////////////////////

		for (;;) {
			rc = read_sock();			// Read what we can
			if ( rc < 0 )
				exit_coroutine();		// Hup? Unable to read more
			if ( rc == 0 )
				exit_coroutine();		// EOF, did not read full header
			hbuf.write(buf,rc);			// Deposit data into hbuf

			//////////////////////////////////////////////
			// Check if we have read the entire header
			//////////////////////////////////////////////

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
			} // else try to read some more..
		}

		//////////////////////////////////////////////////////
		// Extract HTTP header info:
		//////////////////////////////////////////////////////
		{
			hbuf.seekg(0);

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
					char *p = strchr(buf,':');
					if ( p )
						*p = 0;
					ucase(buf);
					if ( p ) {
						++p;
						p += strspn(p," \t\b");
						std::size_t sz = strcspn(p," \t\b");	// Check for trailing whitespace
						std::string trimmed(p,sz);		// Trimmed value

						headers.insert(std::pair<std::string,std::string>(buf,trimmed));
					} else	headers.insert(std::pair<std::string,std::string>(buf,"")); 
				}
			}

			//////////////////////////////////////////////
			// Determine content-length
			//////////////////////////////////////////////
		
			get_header_sz("CONTENT-LENGTH",content_length);

			//////////////////////////////////////////////
			// Check if we have Connection: Keep-Alive
			//////////////////////////////////////////////
			{
				std::string keep_alive;

				if ( get_header_str("CONNECTION",keep_alive) )
					keep_alivef = !strcasecmp(keep_alive.c_str(),"Keep-Alive");
			}
		}

		//////////////////////////////////////////////////////
		// Read remainder of body, if any:
		//////////////////////////////////////////////////////

		while ( body.tellp() < content_length ) {
			rc = read_sock(content_length);
			if ( rc > 0 )
				body.write(buf,rc);
			else if ( rc <= 0 )
				break;
		}

		if ( body.tellp() != content_length )
			exit_coroutine();		// Protocol error

		//////////////////////////////////////////////////////
		// Form Reponse:
		//////////////////////////////////////////////////////

		rhdr	<< "HTTP/1.1 200 OK " << html_endl;
	
		if ( keep_alivef )
			rhdr << "Connection: Keep-Alive" << html_endl;
		else	rhdr << "Connection: Close" << html_endl;

		rbody	<< "Request type: " << reqtype << html_endl
			<< "Request path: " << path << html_endl
			<< "Http Version: " << httpvers << html_endl
			<< "Request Headers were:" << html_endl;

		for ( auto& pair : headers ) {
			const std::string& hdr = pair.first;
			const std::string& val = pair.second;

			rbody	<< "Hdr: " << hdr << ": " << val << html_endl;
		}

		rhdr	<< "Content-Length: " << rbody.tellp() << html_endl
			<< html_endl;

		evt.disable(EPOLLIN);
		evt.enable(EPOLLOUT);
		if ( evt.update() )
			epco.chg(sock,evt.state(),co);

		printf("Writing response..\n");
		write_sock(rhdr);
		write_sock(rbody);

		evt.disable(EPOLLOUT);
		evt.enable(EPOLLIN);
		if ( evt.update() )
			epco.chg(sock,evt.state(),co);

		if ( !keep_alivef ) {
			printf("Not keep-alive..\n");
			break;
		}
		if ( sock_co.get_flags() & (EPOLLHUP|EPOLLRDHUP|EPOLLERR) )
			break;			// No more requests possible
	}

	exit_coroutine();			// For now..
	return nullptr;
}

//////////////////////////////////////////////////////////////////////
// Listen coroutine
//////////////////////////////////////////////////////////////////////

static CoroutineBase *
listen_func(CoroutineBase *co) {
	SockCoro& listen_co = *(SockCoro*)co;
	EpollCoro& epco = *dynamic_cast<EpollCoro*>(listen_co.get_caller());
	s_address addr;
	socklen_t addrlen = sizeof addr;
	int lsock = listen_co.socket();
	int fd;

	for (;;) {
		fd = ::accept4(lsock,&addr.addr,&addrlen,SOCK_NONBLOCK);
		if ( fd < 0 ) {
			listen_co.yield();	// Yield to Epoll
		} else	{
			epco.add(fd,EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR,
				new SockCoro(sock_func,fd));
		}
	}

	return nullptr;
}

int
main(int argc,char **argv) {
	EpollCoro epco;
	int port = 2345, backlog = 50;

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
