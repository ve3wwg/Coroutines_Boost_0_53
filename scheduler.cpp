//////////////////////////////////////////////////////////////////////
// scheduler.cpp -- Scheduler Class Implementation
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

#include "scheduler.hpp"

Scheduler::Scheduler() {
	efd = epoll_create1(EPOLL_CLOEXEC);
	assert(efd > 0);
}

Scheduler::~Scheduler() {
	close(efd);
}

void
Scheduler::close(int fd) {
	int rc;

	do	{
		rc = ::close(fd);
	} while ( rc == -1 && errno == EINTR );
}

bool
Scheduler::del(int fd) {
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
Scheduler::add(int fd,uint32_t events,Service *co) {
	struct epoll_event evt;
	int rc;

	evt.events = events;
	evt.data.ptr = co;
	rc = epoll_ctl(efd,EPOLL_CTL_ADD,fd,&evt);
	return !rc;
}

bool
Scheduler::chg(int fd,Events& ev,CoroutineBase *co) {
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
Scheduler::run() {
	static const int max_events = 8*1024;
	epoll_event events[max_events];
	int rc, n_events;

	for (;;) {
		rc = epoll_wait(efd,&events[0],max_events,10);
		if ( rc > 0 ) {
			n_events = rc;

			for ( int x=0; x<n_events; ++x ) {
				Service& svc = *(Service*)events[x].data.ptr;

				svc.ev_flags = events[x].events;
				svc.er_flags |= svc.ev_flags & (EPOLLERR|EPOLLHUP|EPOLLRDHUP);

				if ( !yield(svc) )			// Invoke service coroutine
					delete &svc;			// Coroutine has terminated
				else	{
					svc.ev.disable_ev(svc.er_flags);	// No longer require notification of seen errors
					if ( svc.ev.sync_ev() )			// Changes to desired event notifications?
						chg(svc.socket(),svc.ev,&svc);	// Yes, make them so
				}
			}
		} else if ( rc < 0 ) {
			printf("Scheduler: %s: epoll_wait()\n",
				strerror(errno));
		}
	}
}

// End scheduler.cpp
