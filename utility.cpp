//////////////////////////////////////////////////////////////////////
// utility.cpp -- 
// Date: Sun Oct 14 16:24:06 2018   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#include <assert.h>

#include "utility.hpp"

static time_t time_offset = 0;

timespec&
timeofday(timespec &tod) {

	::clock_gettime(CLOCK_MONOTONIC,&tod);
	if ( !time_offset ) {
		time_t now = ::time(nullptr);
		time_offset = now - tod.tv_sec;
		assert(tod.tv_sec + time_offset == now);
	}
	tod.tv_sec += time_offset;
	return tod;
}

//////////////////////////////////////////////////////////////////////
// Uppercase in place:
//////////////////////////////////////////////////////////////////////

void
ucase_buffer(char *buf) {
	char ch;

	for ( ; (ch = *buf) != 0; ++buf ) 
		if ( ch >= 'a' && ch <= 'z' )
			*buf &= ~0x20;
}

// End utility.cpp
