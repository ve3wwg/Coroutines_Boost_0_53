//////////////////////////////////////////////////////////////////////
// scheduler.hpp -- Epoll Cororoutine Class
// Date: Sat Aug 11 14:48:30 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <stdint.h>
#include <sys/epoll.h>
#include <unordered_map>

#include "coroutine.hpp"
#include "events.hpp"
#include "sockets.hpp"

class Scheduler;

//////////////////////////////////////////////////////////////////////
// Server processes the Service class:
//////////////////////////////////////////////////////////////////////

class Service : public Coroutine {
	friend Scheduler;

	int		sock;			// Socket
	Events		ev;
	uint32_t	er_flags=0;		// Error flags
	uint32_t	ev_flags=0;		// Event flags

public:	Service(fun_t func,int fd) : Coroutine(func), sock(fd) {}
	int socket() noexcept 			{ return sock; }
	Events &events() noexcept		{ return ev; }
	uint32_t err_flags() noexcept		{ return er_flags; }
	uint32_t evt_flags() noexcept		{ return ev_flags; }
};


//////////////////////////////////////////////////////////////////////
// epoll(2) Scheduler of Services
//////////////////////////////////////////////////////////////////////

class Scheduler : public CoroutineMain {
	int		efd = -1;		// From epoll_create1()

	std::unordered_map<int/*fd*/,CoroutineBase*> fdset;

public:	Scheduler();
	~Scheduler();
	void close(int fd);
	void run();

	void sync(Events& ev) noexcept;

	bool add(int fd,uint32_t events,Service *co);
	bool del(int fd);
	bool chg(int fd,Events& ev,CoroutineBase *co);
};

#endif // SCHEDULER_HPP

// End scheduler.hpp
