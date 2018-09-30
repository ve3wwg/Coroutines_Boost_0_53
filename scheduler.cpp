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

int
Service::read_sock(int fd,void *buf,size_t bytes) noexcept {
	int rc;

	for (;;) {
		rc = ::read(fd,buf,bytes);
		if ( rc < 0 ) {
			switch ( errno ) {
			case EINTR:
				break;			// Signaled, retry..
			case EWOULDBLOCK:
				yield();		// No data to read, yet.
				break;
			default:
				return -errno;		// Fail..
			}
		} else	{
			return rc;			// Return what we've read
		}
	}
	return -errno;					// Should never get here
}

int
Service::write_sock(int fd,const void *buf,size_t bytes) noexcept {
	int rc;

	for (;;) {
		rc = ::write(fd,buf,bytes);
		if ( rc < 0 ) {
			switch ( errno ) {
			case EINTR:
				break;			// Signaled, retry..
			case EWOULDBLOCK:
				yield();		// Unable to write, yet.
				break;
			default:
				return -errno;		// Fail..
			}
		} else	{
			return rc;			// Return what we've written
		}
	}
	return -errno;					// Should never get here
}

//////////////////////////////////////////////////////////////////////
// Read until http buffer complete in buf:
//
// RETURNS:
//	< 0	Fatal error
//	0	EOF encountered before header was fully read
//	1	Received http header into buf
//////////////////////////////////////////////////////////////////////

int
Service::read_header(int fd,HttpBuf& buf) noexcept {
	return buf.read_header(sock,read_cb,this);
}

//////////////////////////////////////////////////////////////////////
// Read the remainder of the body, according to content_length:
//
// RETURNS:
//	< 0	Fatal error
//	>= 0	Actual body length read
//////////////////////////////////////////////////////////////////////

int
Service::read_body(int fd,HttpBuf& buf,size_t content_length) noexcept {
	return buf.read_body(sock,read_cb,this,content_length);
}

int
Service::write(int fd,HttpBuf& buf) noexcept {
	return buf.write(sock,write_cb,this);
}

//////////////////////////////////////////////////////////////////////
// Internal: Read callback
//////////////////////////////////////////////////////////////////////

int
Service::read_cb(int fd,void *buf,size_t bytes,void *arg) noexcept {
	Service& svc = *(Service*)arg;

	return svc.read_sock(fd,buf,bytes);
};

int
Service::write_cb(int fd,const void *buf,size_t bytes,void *arg) noexcept {
	Service& svc = *(Service*)arg;

	return svc.write_sock(fd,buf,bytes);
};

// End scheduler.cpp
