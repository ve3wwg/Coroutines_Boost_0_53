//////////////////////////////////////////////////////////////////////
// iobuf.cpp -- IOBuf based upon std::stringstream
// Date: Sat Sep 29 11:20:36 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "iobuf.hpp"

std::string
IOBuf::sample() noexcept {
	size_t gpos = tellg(), ppos = tellp();
	std::string rs(str());
	
	str("");
	clear();

	// Restore stream:
	*this << rs;
	seekg(gpos);
	seekp(ppos);

	return rs;	// Return contents as a string
}

//////////////////////////////////////////////////////////////////////
// Reset the object to it's initial state
//////////////////////////////////////////////////////////////////////

void
IOBuf::reset() noexcept {
	std::stringstream::str("");
	std::stringstream::clear();
}

// End iobuf.cpp
