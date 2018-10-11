//////////////////////////////////////////////////////////////////////
// evtimer.hpp -- Event Timer
// Date: Wed Oct 10 21:37:36 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef EVTIMER_HPP
#define EVTIMER_HPP

#include <time.h>
#include <boost/intrusive/list.hpp>
#include "circarray.hpp"

typedef boost::intrusive::list_member_hook<> EvNode;	// Node to be included in Object

template<typename Object>
class EvTimer {
	typedef boost::intrusive::member_hook<Object,EvNode,&Object::evnode> MemberHook;
	typedef boost::intrusive::list<Object,MemberHook> ObjList;

	time_t		epoch=0;			// carray[0] begins at this time (needs to be timespec)

	struct s_tnode {
		ObjList		list;
	};

	CircArray<s_tnode>	carray;

public:	EvTimer(unsigned secs_max,unsigned granlarity_ms) noexcept;
	void insert(unsigned x,Object& object) noexcept;

	void visit(unsigned x,void (*cb)(Object& object,void *arg),void *arg) noexcept;		// Needs to be private..
};


template<typename Object>
EvTimer<Object>::EvTimer(unsigned secs_max,unsigned granularity_ms) noexcept : carray(secs_max * 1000u / granularity_ms) {
}


template<typename Object>
void
EvTimer<Object>::insert(unsigned x,Object& object) noexcept {
	carray[x].list.push_back(object);
}


template<typename Object>
void
EvTimer<Object>::visit(unsigned x,void (*cb)(Object& object,void *arg),void *arg) noexcept {
	ObjList& list = carray[x].list;
	
	while ( !list.empty() ) {
		Object& object = list.front();
		list.pop_front();
		cb(object,arg);
	}
}

#endif // EVTIMER_HPP

// End evtimer.hpp
