//////////////////////////////////////////////////////////////////////
// epollcoro.hpp -- Epoll Cororoutine Class
// Date: Sat Aug 11 14:48:30 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef EPOLLCORO_HPP
#define EPOLLCORO_HPP

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h>

#include "coroutine.hpp"

#include <unordered_map>

#include "events.hpp"

union s_address {
	struct sockaddr		addr;
	struct sockaddr_in 	addr4;		// AF_INET
	struct sockaddr_in6	addr6;		// AF_INET6
};

class SockCoro : public Coroutine {
	int		sock;			// Socket

public:
	Events		ev;
	uint32_t	er_flags=0;		// Error flags
	uint32_t	ev_flags=0;		// Event flags

	SockCoro(fun_t func,int fd) : Coroutine(func), sock(fd) {}
	int socket() noexcept 			{ return sock; }
};

class EpollCoro : public CoroutineMain {
	int		efd = -1;		// From epoll_create1()

	std::unordered_map<int/*fd*/,CoroutineBase*> fdset;

public:	EpollCoro();
	virtual ~EpollCoro();
	virtual void close(int fd);
	virtual void run();

	void sync(Events& ev) noexcept;

	virtual bool add(int fd,uint32_t events,SockCoro *co);
	virtual bool del(int fd);
	virtual bool chg(int fd,Events& ev,CoroutineBase *co);

	static bool import_ip(const char *straddr,s_address& addr);
	static bool import_ipv4(const char *ipv4,s_address& addr);
	static bool import_ipv6(const char *ipv6,s_address& addr);
	static int listen(s_address& address,int port,unsigned backlog,bool reuse_port=false);
};

#endif // EPOLLCORO_HPP

// End epollcoro.hpp
