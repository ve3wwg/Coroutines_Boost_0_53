//////////////////////////////////////////////////////////////////////
// server.cpp -- Test http server.
// Date: Sat Aug 11 15:47:54 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "scheduler.hpp"

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
	Service& svc = *dynamic_cast<Service*>(co);		// This coroutine that is scheduled
	Scheduler& scheduler = *dynamic_cast<Scheduler*>(svc.get_caller()); // Invoking scheduler
	const int sock = svc.socket();			// Socket being processed
	Events& ev = svc.events();				// EPoll events control
	// Service variables:
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
	// Terminate the Service processing. WHen Scheduler recieves
	// a nullptr, it will destroy this coroutine.
	//////////////////////////////////////////////////////////////

	auto exit_coroutine = [&]() {
		printf("Exit coroutine sock=%d\n",sock);
		scheduler.del(sock);			// Remove our socket from Epoll
		close(sock);				// Close the socket
		svc.terminate();			// Delete this coroutine
		assert(0);				// Should never get here..
	};

	ev.set_ev(EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR);

	//////////////////////////////////////////////////////////////
	// Read from the socket until we block:
	//////////////////////////////////////////////////////////////

	auto io_read = [&](size_t max=0) -> int {
		int rc;	

		if ( max <= 0 )
			max = sizeof buf;
		else if ( max > sizeof buf )
			max = sizeof buf;

		for (;;) {
			rc = svc.read_sock(sock,buf,max);
			if ( rc >= 0 )
				return rc;
			printf("ERROR, %s: read(fd=%d\n",strerror(errno),sock);
			return rc;
		}
	};

	auto io_write = [&](std::stringstream& s) {
		std::string flattened(s.str());
		const char *p = flattened.c_str();
		std::size_t spos = 0, sz = flattened.size();
		int rc;

		while ( spos < sz ) {
			rc = svc.write_sock(sock,p+spos,sz);
			if ( rc < 0 ) {
				exit_coroutine(); // Fatal error
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
			rc = io_read();				// Read what we can
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

		while ( long(body.tellp()) < long(content_length) ) {
			rc = io_read(content_length);
			if ( rc > 0 )
				body.write(buf,rc);
			else if ( rc <= 0 )
				break;
		}

		if ( long(body.tellp()) != long(content_length) )
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

		ev.disable_ev(EPOLLIN);
		ev.enable_ev(EPOLLOUT);

		printf("Writing response..\n");
		io_write(rhdr);
		io_write(rbody);

		ev.disable_ev(EPOLLOUT);
		ev.enable_ev(EPOLLIN);

		if ( !keep_alivef ) {
			printf("Not keep-alive..\n");
			break;
		}
		if ( svc.err_flags() & (EPOLLHUP|EPOLLRDHUP|EPOLLERR) )
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
	Service& listen_co = *(Service*)co;
	Scheduler& scheduler = *dynamic_cast<Scheduler*>(listen_co.get_caller());
	u_address addr;
	socklen_t addrlen = sizeof addr;
	int lsock = listen_co.socket();
	int fd;

	for (;;) {
		fd = ::accept4(lsock,&addr.addr,&addrlen,SOCK_NONBLOCK);
		if ( fd < 0 ) {
			listen_co.yield();	// Yield to Epoll
		} else	{
			scheduler.add(fd,EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR,
				new Service(sock_func,fd));
		}
	}

	return nullptr;
}

int
main(int argc,char **argv) {
	Scheduler scheduler;
	int port = 2345, backlog = 50;

	auto add_listen_port = [&](const char *straddr) {
		u_address addr;
		int lfd = -1;
		bool bf;

		Sockets::import_ip(straddr,addr);
		lfd = Sockets::listen(addr,port,backlog);
		assert(lfd >= 0);
		bf = scheduler.add(lfd,EPOLLIN,new Service(listen_func,lfd));
		assert(bf);
	};

	if ( !argv[1] ) {
		add_listen_port("127.0.0.1");
	} else	{
		for ( auto x=1; x<argc; ++x )
			add_listen_port(argv[x]);
	}

	scheduler.run();
	return 0;
}

// End server.cpp
