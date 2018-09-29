//////////////////////////////////////////////////////////////////////
// events.hpp -- EPoll(2) event related
// Date: Sat Sep 29 09:49:18 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef EVENTS_HPP
#define EVENTS_HPP

#include <stdint.h>

//////////////////////////////////////////////////////////////////////
// Manage epoll(2) event notifications:
//////////////////////////////////////////////////////////////////////

class Events {
	uint32_t	ev_events=0;		// Events required
	uint32_t	ev_chgs=0;		// Changes to ev_events (xor)

public:	Events(uint32_t ev=0) : ev_events(ev) {}

	void set_ev(uint32_t ev) noexcept	{ ev_chgs = ev ^ ev_events;  }
	void enable_ev(uint32_t ev) noexcept	{ ev_chgs = ( (ev_events ^ ev_chgs) | ev ) ^ ev_events; }
	void disable_ev(uint32_t ev) noexcept	{ ev_chgs = ( (ev_events ^ ev_chgs) & ~ev ) ^ ev_events; }

	uint32_t changes() noexcept		{ return ev_chgs; }
	uint32_t events() noexcept		{ return ev_events; }

	bool sync_ev() noexcept {
		if ( !ev_chgs )
			return false;
		ev_events ^= ev_chgs;		// Apply changes
		return true;
	};
};

#endif // EVENTS_HPP

// End events.hpp
