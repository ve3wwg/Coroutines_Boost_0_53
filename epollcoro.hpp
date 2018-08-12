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

class Events {
	uint32_t	evstate;
	uint32_t	evchgs=0;

public:	Events(uint32_t ev=0) : evstate(ev) {}
	void set(uint32_t ev) noexcept		{ evchgs = evstate ^ ev;  }
	void enable(uint32_t ev) noexcept	{ evchgs = ( (evstate ^ evchgs) | ev ) ^ evstate; }
	void disable(uint32_t ev) noexcept	{ evchgs = ( (evstate ^ evchgs) & ~ev ) ^ evstate; }
	uint32_t changes() noexcept		{ return evchgs; }
	uint32_t state() noexcept		{ return evstate; }
};

union s_address {
	struct sockaddr		addr;
	struct sockaddr_in 	addr4;		// AF_INET
	struct sockaddr_in6	addr6;		// AF_INET6
};

class SockCoro : public Coroutine {
	int		sock;			// Socket
	uint32_t	events=0;		// Epoll events
	uint32_t	flags=0;		// Epoll flags like EPOLLHUP/RDHUP/ERR

public:	SockCoro(fun_t func,int fd) : Coroutine(func), sock(fd) {}
	int socket() noexcept 			{ return sock; }
	void set_events(uint32_t ev) noexcept;
	uint32_t get_events() noexcept 		{ return events; }
	uint32_t get_flags() noexcept		{ return flags; }
};

class EpollCoro {
	CoroutineMain	epco;		// Main coroutine context
	int		efd = -1;	// From epoll_create1()

	std::unordered_map<int/*fd*/,CoroutineBase*> fdset;

public:	EpollCoro();
	~EpollCoro();
	void close(int fd);
	void run();

	bool add(int fd,uint32_t events,SockCoro *co);
	bool del(int fd);
	bool chg(int fd,uint32_t events,CoroutineBase *co=nullptr);
	bool chg(int fd,Events& ev,CoroutineBase *co=nullptr);

	static bool import_ip(const char *straddr,s_address& addr);
	static bool import_ipv4(const char *ipv4,s_address& addr);
	static bool import_ipv6(const char *ipv6,s_address& addr);
	static int listen(s_address& address,int port,unsigned backlog,bool reuse_port=false);
};

#endif // EPOLLCORO_HPP

// End epollcoro.hpp
