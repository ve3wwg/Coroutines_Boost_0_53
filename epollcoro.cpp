//////////////////////////////////////////////////////////////////////
// epollcoro.cpp -- EpollCoro Class Implementation
// Date: Sat Aug 11 14:52:20 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <assert.h>

#include "epollcoro.hpp"

EpollCoro::EpollCoro() {
	efd = epoll_create1(EPOLL_CLOEXEC);
	assert(efd > 0);
}

EpollCoro::~EpollCoro() {
	close(efd);
}

void
EpollCoro::close(int fd) {
	int rc;

	do	{
		rc = ::close(fd);
	} while ( rc == -1 && errno == EINTR );
}

bool
EpollCoro::del(int fd) {
	int rc;

	auto it = fdset.find(fd);
	if ( it == fdset.end() ) {
		errno = EBADF;
		return false;
	}

	fdset.erase(it);
	rc = epoll_ctl(efd,EPOLL_CTL_DEL,fd,nullptr);
	return !rc;
}

bool
EpollCoro::add(int fd,uint32_t events,SockCoro *co) {
	struct epoll_event evt;
	int rc;

	evt.events = events;
	evt.data.ptr = co;
	rc = epoll_ctl(efd,EPOLL_CTL_ADD,fd,&evt);
	return !rc;
}

bool
EpollCoro::chg(int fd,Events& ev,CoroutineBase *co) {
	struct epoll_event evt;
	int rc;

	if ( ev.changes() == 0 )
		return true;			// No changes

	evt.events = ev.events();
	evt.data.ptr = co;
	rc = epoll_ctl(efd,EPOLL_CTL_MOD,fd,&evt);
	return !rc;
}

void
EpollCoro::run() {
	static const int max_events = 8*1024;
	epoll_event events[max_events];
	int rc, n_events;

	for (;;) {
		rc = epoll_wait(efd,&events[0],max_events,10);
		if ( rc > 0 ) {
			n_events = rc;
			for ( int x=0; x<n_events; ++x ) {
				SockCoro& co = *(SockCoro*)events[x].data.ptr;

				co.ev_flags = events[x].events;
				co.er_flags |= co.ev_flags & (EPOLLERR|EPOLLHUP|EPOLLRDHUP);

				if ( !yield(co) )
					delete &co;	// Coroutine is done
				else if ( co.ev.sync_ev() )
					chg(co.socket(),co.ev,&co);
			}
		} else if ( rc < 0 ) {
			printf("EpollCoro: %s: epoll_wait()\n",
				strerror(errno));
		}
	}
}

int
EpollCoro::listen(s_address& address,int port,unsigned backlog,bool reuse_port) {
        static const int one = 1;
	int sock, af = address.addr4.sin_family;
	int rc;

        if ( (sock = socket(af,SOCK_STREAM|SOCK_NONBLOCK,0)) == -1 )
                return -1;              // Socket failure, see errno

	rc = setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof one);
	if ( rc == -1 ) {
		::close(sock);
		return -2;      // Set sock opt failure:
        }

#ifdef SO_REUSEPORT
        if ( reuse_port ) {
		rc = setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,(char *)&one,sizeof one);
                if ( rc == -1 ) {  
			::close(sock);
			return -2;      // Set sock opt failure:
		}
	}
#endif
        
        switch ( af ) {
        case AF_INET:
                {
                        sockaddr_in& sock_addr = address.addr4;
                        
			address.addr4.sin_port = htons(port);
                        rc = ::bind(sock,(const struct sockaddr *)&sock_addr,sizeof sock_addr);
                        if ( rc == -1 ) {  
                                ::close(sock);
                                return -3;              // bind failure
                        }
                }
                break;
        case AF_INET6:
                {
                        sockaddr_in6& addr = address.addr6;
                        
			address.addr6.sin6_port = htons(port);
                        rc = ::bind(sock,(const struct sockaddr *)&addr,sizeof addr);
                        if ( rc == -1 ) {  
                                ::close(sock);
                                return -3;              // bind failure
                        }
                }
        }
        
        rc = ::listen(sock,backlog);
        if ( rc == -1 ) {  
                ::close(sock);
                return -4;              // listen(2) failure
        }
	
	return sock;
}

bool
EpollCoro::import_ip(const char *straddr,s_address& addr) {
        bool ipv4f = !!strchr(straddr,'.');  // IPv4 uses '.'
        
        if ( ipv4f )
                return import_ipv4(straddr,addr);
        else    return import_ipv6(straddr,addr);
}

bool
EpollCoro::import_ipv4(const char *ipv4,s_address& addr) {

        memset(&addr.addr4,0,sizeof addr.addr4);
        if ( !inet_aton(ipv4,&addr.addr4.sin_addr) )
                return false;                           // Invalid address format
        addr.addr4.sin_family = AF_INET;
        return true;
}

bool
EpollCoro::import_ipv6(const char *ipv6,s_address& addr) {
        char saddr[strlen(ipv6)+1];
        char *iface;   

        memset(&addr.addr6,0,sizeof addr.addr6);
        strcpy(saddr,ipv6);

        if ( (iface = strrchr(saddr,'%')) != 0 )
                *iface++ = 0;

	addr.addr6.sin6_family = 0;
        if ( !inet_pton(AF_INET6,saddr,&addr.addr6.sin6_addr) )
                return false;
        
        if ( iface )
                addr.addr6.sin6_scope_id = if_nametoindex(iface);

        addr.addr6.sin6_family = AF_INET6;
        return true;
} 

// End epollcoro.cpp
