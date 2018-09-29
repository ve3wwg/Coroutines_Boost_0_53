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

// End epollcoro.cpp
