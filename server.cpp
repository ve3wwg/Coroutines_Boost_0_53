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
#include "httpbuf.hpp"

static const char html_endl[] = "\r\n";

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
	HttpBuf hbuf;
	HttpBuf rhdr, rbody;
	std::string body;
	std::unordered_multimap<std::string,std::string> headers;
	std::size_t content_length = 0;
	bool keep_alivef = false;				// True when we have Connection: Keep-Alive

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
	// Keep-Alive Loop:
	//////////////////////////////////////////////////////////////

	for (;;) {
		hbuf.reset();
		rhdr.reset();
		rbody.reset();

		reqtype.clear();
		path.clear();
		httpvers.clear();
		headers.clear();
		content_length = 0;
		keep_alivef = false;

		printf("Expecting request from sock=%d\n",sock);

		//////////////////////////////////////////////////////
		// Read http headers:
		//////////////////////////////////////////////////////

		if ( svc.read_header(sock,hbuf) != 1 )
			exit_coroutine();		// Fail!

		content_length = hbuf.parse_headers(reqtype,path,httpvers,headers,4096);

		//////////////////////////////////////////////////////
		// Check if we have Connection: Keep-Alive
		//////////////////////////////////////////////////////
		{
			std::string keep_alive;

			if ( get_header_str("CONNECTION",keep_alive) )
				keep_alivef = !strcasecmp(keep_alive.c_str(),"Keep-Alive");
		}

		svc.read_body(sock,hbuf,content_length);
		body.assign(hbuf.body());

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
			<< "Extracted body was " << body.size() << " bytes in length" << html_endl
			<< "Body was:" << html_endl
			<< body << html_endl
			<< html_endl;

		ev.disable_ev(EPOLLIN);
		ev.enable_ev(EPOLLOUT);

		svc.write(sock,rhdr);
		svc.write(sock,rbody);

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
