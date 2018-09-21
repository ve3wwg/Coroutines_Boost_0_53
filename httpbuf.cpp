//////////////////////////////////////////////////////////////////////
// httpbuf.cpp -- http stream buf implementation
// Date: Thu Sep 20 15:13:32 2018   (C) Warren Gay
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "httpbuf.hpp"

//////////////////////////////////////////////////////////////////////
// Test if the std::stringstream has the http end header sequence
// \r\n\r\n or \n\n, without reprocessing everything found in the
// buffer.
//
// RETURNS:
//	true	http end header found, and position returned
//		via pointer pepos. The end header sequence length
//		is returned via pointer pelen. The offset of the
//		body in the request is given by *pepos + *pelen.
//	false	No http end header was found in the buffer (yet).
//		Neither *pepos nor *pelen is updated.
// NOTES:
//	1. The stream's read and write pointers are preserved.
//////////////////////////////////////////////////////////////////////

bool
HttpBuf::have_end(size_t *pepos,size_t *pelen) noexcept {
	size_t saved_gpos, n;
	char buf[1024], ch;

	auto found = [&]() -> bool {
		if ( pepos )			// Http end header found..
			*pepos = hdr_epos;	// At this position
		if ( pelen )
			*pelen = hdr_elen; 	// With the sequence length
		seekg(saved_gpos);		// Restore std::stringstream read pos
		return true;			// Indicate http end found
	};

	if ( hdr_elen > 0 ) {
		// We've already determined where the header end was:
		if ( pepos )
			*pepos = hdr_epos;
		if ( pelen )
			*pelen = hdr_elen;
		return true;		
	}

	if ( (saved_gpos  = tellg()) != hdr_epos )	// Save the read stream pos
		seekg(hdr_epos);			// Seek to where we left off scanning

	while ( tellg() < tellp() && (n = readsome(buf,sizeof buf)) > 0 ) {
		for ( size_t x=0; x < n; ++x ) {
			ch = buf[x];

			switch ( state ) {
			case S0CR1:
				if ( ch == '\r' )
					state = S0LF1;
				else if ( ch == '\n' )
					state = S1LF2;
				break;
			case S0LF1:
				if ( ch == '\n' )
					state = S0CR2;
				else	state = S0CR1;
				break;
			case S0CR2:
				if ( ch == '\r' )
					state = S0LF2;
				else	state = S0CR1;
				break;
			case S0LF2:
				if ( ch == '\n' ) {
					hdr_elen = 4;
					hdr_epos += x - hdr_elen + 1;
					return found();
				}
				state = S0CR1;
				break;
                        case S1LF2:
				if ( ch == '\n' ) {
					hdr_elen = 2;
					hdr_epos += x - hdr_elen + 1;
					return found();
				}
				state = S0CR1;
				break;
			}
		}
		hdr_epos += n;
	}

	// Fail:
	seekg(saved_gpos);	// Restore read position
	return false;		// No http end header sequence found (yet)
}

//////////////////////////////////////////////////////////////////////
// Reset the object to it's initial state
//////////////////////////////////////////////////////////////////////

void
HttpBuf::reset() {
	hdr_elen = 0;
	hdr_epos = 0;
	state = S0CR1;
	std::stringstream::str("");
	std::stringstream::clear();
}

// End httpbuf.cpp
