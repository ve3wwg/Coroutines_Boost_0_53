//////////////////////////////////////////////////////////////////////
// evtimer.hpp -- Event Timer
// Date: Wed Oct 10 21:37:36 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef EVTIMER_HPP
#define EVTIMER_HPP

#include <time.h>
#include <assert.h>
#include <boost/intrusive/list.hpp>
#include "circarray.hpp"
#include "utility.hpp"

typedef boost::intrusive::list_member_hook<> EvNode;	// Node to be included in Object

template<typename Object>
class EvTimer {
	typedef boost::intrusive::member_hook<Object,EvNode,&Object::evnode> MemberHook;
	typedef boost::intrusive::list<Object,MemberHook> ObjList;

	timespec		epoch;			// carray[0] begins at this time (needs to be timespec)
	timespec		incr;			// Time increment as timespec
	long			incr_ms;		// Time increment in ms

	CircArray<ObjList>	carray;

	void visit(unsigned x,void (*cb)(Object& object,void *arg),void *arg) noexcept;		// Needs to be private..

public:	EvTimer(unsigned secs_max,unsigned granlarity_ms) noexcept;
	EvTimer& insert(long ms,Object& object) noexcept;
	EvTimer& expire(const timespec& now,void (*cb)(Object& object,void *arg),void *arg) noexcept;
};


template<typename Object>
EvTimer<Object>::EvTimer(unsigned secs_max,unsigned granularity_ms) noexcept : carray(secs_max * 1000u / granularity_ms + 1) {

	incr.tv_sec = granularity_ms / 1000L;
	incr.tv_nsec = granularity_ms * 1000000L;
	incr_ms = incr.tv_sec * 1000L + incr.tv_nsec / 1000000L;
	timeofday(epoch);
}

template<typename Object>
EvTimer<Object>&
EvTimer<Object>::insert(long time_ms,Object& object) noexcept {
	timespec now;
	long incr_time_ms, x;

	timeofday(now);
	now -= epoch;
	incr_time_ms = millisecs(now) + time_ms;
	x = incr_time_ms / incr_ms;

	if ( x > long(carray.size()) )
		x = long(carray.size()) - 1;
	carray[x].push_back(object);
	return *this;
}

template<typename Object>
EvTimer<Object>&
EvTimer<Object>::expire(const timespec& now,void (*cb)(Object& object,void *arg),void *arg) noexcept {

	for ( ; epoch <= now; epoch += incr ) {
		ObjList& list = carray[0];

		while ( !list.empty() ) {
			Object& object = list.front();
			list.pop_front();
			cb(object,arg);
		}
		carray.advance(1);
	}
	return *this;
}

template<typename Object>
void
EvTimer<Object>::visit(unsigned x,void (*cb)(Object& object,void *arg),void *arg) noexcept {
	ObjList& list = carray[x];
	
	while ( !list.empty() ) {
		Object& object = list.front();
		list.pop_front();
		cb(object,arg);
	}
}

#endif // EVTIMER_HPP

// End evtimer.hpp
