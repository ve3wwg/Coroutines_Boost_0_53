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
#include "utils.hpp"

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
// Read until the end of the http header:
//////////////////////////////////////////////////////////////////////

int
HttpBuf::read_header(int fd,readcb_t readcb,void *arg) noexcept {
	char buf[1024];
	size_t epos=0, elen=0;
	int rc;

	for (;;) {
		if ( have_end(&epos,&elen) )
			return 1;
		rc = readcb(fd,buf,sizeof buf,arg);
		if ( rc < 0 )
			return rc;		// Return I/O error
		else if ( rc == 0 )
			return 0;		// EOF!
		std::stringstream::write(buf,rc);			
	}
	return 0;	// Should never get here
}

//////////////////////////////////////////////////////////////////////
// Read the remainder of the request body, if any:
//
// RETURNS:
//	< 0	Fatal I/O error
//	>= 0	Size of actually read body
//////////////////////////////////////////////////////////////////////

int
HttpBuf::read_body(int fd,readcb_t readcb,void *arg,size_t content_length) noexcept {
	size_t bpos = hdr_epos + hdr_elen;
	char buf[2048];
	int rc;

	assert(size_t(tellp()) >= bpos);
	if ( content_length <= 0 )
		return size_t(tellp()) - bpos;	// Content length

	while ( size_t(tellp()) < bpos + content_length ) {
		rc = readcb(fd,buf,sizeof buf,arg);
		if ( rc < 0 )
			return rc;		// Return I/O error
		else if ( rc == 0 )
			return 0;		// EOF!
		std::stringstream::write(buf,rc);			
	}
	return size_t(tellp()) - bpos;		// Actual body size (read)
}

//////////////////////////////////////////////////////////////////////
// Reset stream to initial state
//////////////////////////////////////////////////////////////////////

void
HttpBuf::reset() noexcept {
	state = S0CR1;
	hdr_elen = 0;
	hdr_epos = 0;
	IOBuf::reset();
}

//////////////////////////////////////////////////////////////////////
// Parse received header data into header lines:
//////////////////////////////////////////////////////////////////////

size_t
HttpBuf::parse_headers(
  std::string& reqtype,			// Out: GET/POST
  std::string& path,			// Out: Path component
  std::string& httpvers,		// Out: HTTP/x.x
  std::unordered_multimap<std::string,std::string>& headers, // Out: Parsed headers
  size_t maxhdr				// In:  Max size of headers buffer for parsing
) noexcept {
	char *buf = new char[maxhdr];

	seekg(0);

	auto read_line = [&]() -> bool {
		getline(buf,maxhdr-1);
		buf[maxhdr-1] = 0;

		size_t sz = strcspn(buf,"\r\n");
		buf[sz] = 0;
		if ( !*buf )
			return false;
		return !this->fail();
	};

	if ( read_line() ) {
		unsigned ux = strcspn(buf," \t\b");
		char *p;

		reqtype.assign(buf,ux);
		p = buf + ux;
		p += strspn(p," \t\b");
		ux = strcspn(p," \t\b");
		path.assign(p,ux);
		p += ux;
		p += strspn(p," \t\b");
		httpvers.assign(p,strcspn(p," \t\b"));

		while ( read_line() ) {
			char *p = strchr(buf,':');
			if ( p )
				*p = 0;
			ucase_buffer(buf);
			if ( p ) {
				++p;
				p += strspn(p," \t\b");
				std::size_t sz = strcspn(p," \t\b");	// Check for trailing whitespace
				std::string trimmed(p,sz);		// Trimmed value

				headers.insert(std::pair<std::string,std::string>(buf,trimmed));
			} else	headers.insert(std::pair<std::string,std::string>(buf,"")); 
		}
	}

	delete[] buf;
	buf = 0;

	seekg(hdr_epos+hdr_elen);	// Seek to start of body

	auto it = headers.find("CONTENT-LENGTH");
	if ( it == headers.end() )
		return 0;		// No body

	const std::string& clen = it->second;
	return strtoul(clen.c_str(),nullptr,10); // Body length
}

// End httpbuf.cpp
