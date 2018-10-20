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

#include <memory>

#include "httpbuf.hpp"
#include "utility.hpp"

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
HttpBuf::read_header(int fd,readcb_t readcb,void *arg) {
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
HttpBuf::read_body(int fd,readcb_t readcb,void *arg,size_t content_length) {
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
// Read a chunked body, starting with text arg prime.
//////////////////////////////////////////////////////////////////////

int
HttpBuf::read_chunked(int fd,readcb_t readcb,void *arg,std::stringstream& unchunked) {
	size_t bpos = hdr_epos + hdr_elen;
	char buf[2048], ch;
	size_t chunk_size;
	int rc;

	assert(size_t(tellp()) >= bpos);

	struct IO_Exception : public std::exception {
		int	rc;
		IO_Exception(int rc) : rc(rc) {};
	};

	auto get_char = [&](char& ch) {
		while ( tellg() >= tellp() ) {
			rc = readcb(fd,buf,sizeof buf,arg);
			if ( rc <= 0 )
				throw IO_Exception(rc);
			std::stringstream::write(buf,rc);
		}
		get(ch);
	};

	auto copy_unch = [&](size_t n) {
		size_t sn;
		while ( n > 0u ) {
			sn = readsome(buf,n < sizeof buf ? n : sizeof buf);
			unchunked.write(buf,sn);
			n -= sn;
		}
	};

	auto read_dat = [&](size_t n) {
		while ( n > 0u ) {
			rc = readcb(fd,buf,n < sizeof buf ? n : sizeof buf,arg);
			if ( rc <= 0 )
				throw IO_Exception(rc);
			std::stringstream::write(buf,rc);
			n -= rc;
		}
	};

	auto got_eol = [&]() -> bool {
		if ( ch == '\r' ) {
			get_char(ch);		// Try to eat LF
			if ( ch != '\n' )
				unget();	// Not LF, so give it back
			return true;
		}
		if ( ch == '\n' )		// Got LF instead, treat as CR LF
			return true;
		return false;			// ch is not at EOL
	};

	auto eat_eol = [&]() -> bool {
		get_char(ch);
		return got_eol();
	};

	try	{
		for (;;) {
			//////////////////////////////////////////////
			// Read in the chunk's size
			//////////////////////////////////////////////
			chunk_size = 0;
			for (;;) {
				get_char(ch);
				if ( ch >= '0' && ch <= '9' )
					chunk_size = chunk_size * 10u + (ch & 0x0F);
				else if ( (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') )
					chunk_size = chunk_size * 10u + (ch & 0x0F) + 9;
				else if ( got_eol() )
					break;
			}
			if ( chunk_size == 0u )
				break;			// End of chunks
			size_t have = size_t(tellp()) - size_t(tellg());

			if ( have > 0u ) {		// Copy pre-read chunk data, if any
				if ( have > chunk_size )
					have = chunk_size;
				copy_unch(have);
				chunk_size -= have;
			}

			if ( chunk_size > 0u ) {	// Else go read more to satisfy
				read_dat(chunk_size);
				copy_unch(chunk_size);
			}
			eat_eol();
		} // Reading chunks

		//////////////////////////////////////////////////////
		// Skip over trailer-part
		//////////////////////////////////////////////////////

		hdr_chunked = tellg();			// Position of where extended headers go
		for (;;) {
			get_char(ch);		
			if ( got_eol() )
				break;
			// Skip trailer-part
			for (;;) {
				get_char(ch);
				if ( got_eol() )
					break;
			}
		}
		hdr_chunklen = size_t(tellg()) - hdr_chunked;	// Length of extended headers

	} catch ( IO_Exception& e ) {
		return e.rc;				// I/O error, or EOF
	}

	return int(unchunked.tellp());
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
//
// RETURNS:
//	0	No body (or is chunked). Indicates no Content-Length
//		header was found.
//	>0	Body length
//////////////////////////////////////////////////////////////////////

size_t
HttpBuf::parse_headers(
  std::string& reqtype,			// Out: GET/POST
  std::string& path,			// Out: Path component
  std::string& httpvers,		// Out: HTTP/x.x
  headermap_t& headers, 		// Out: Parsed headers
  size_t maxhdr				// In:  Max size of headers buffer for parsing
) noexcept {
	char *buf = new char[maxhdr+1];

	seekg(0);

	auto read_line = [&]() -> bool {
		getline(buf,maxhdr);
		buf[maxhdr] = 0;

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
				*p++ = 0;
			if ( p ) {
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

//////////////////////////////////////////////////////////////////////
// Parse out extended headers, if any after chunked data
//
// RETURNS:
//	false	No extension headers found
//	true	Extension headers were returned
//////////////////////////////////////////////////////////////////////

bool
HttpBuf::parse_xheaders(headermap_t& headers,size_t maxhdr) {
	char *buf;
	size_t svgpos, endpos;
	bool xheadersf = false;

	if ( hdr_chunked <= 0 )
		return 0;		// No extended headers

	auto read_line = [&]() -> bool {
		if ( size_t(tellg()) >= endpos )
			return false;
		getline(buf,maxhdr);
		buf[maxhdr] = 0;

		size_t sz = strcspn(buf,"\r\n");
		buf[sz] = 0;
		if ( !*buf )
			return false;
		return !this->fail();
	};

	svgpos = tellg();
	endpos = hdr_chunked + hdr_chunklen;
	seekg(hdr_chunked);

	buf = new char[maxhdr+1];
	
	while ( read_line() ) {
		char *p = strchr(buf,':');
		if ( p )
			*p++ = 0;
		if ( p ) {
			p += strspn(p," \t\b");
			std::size_t sz = strcspn(p," \t\b");	// Check for trailing whitespace
			std::string trimmed(p,sz);		// Trimmed value

			headers.insert(std::pair<std::string,std::string>(buf,trimmed));
		} else	headers.insert(std::pair<std::string,std::string>(buf,""));
		xheadersf = true;
	}

	delete[] buf;
	seekg(svgpos);
	return xheadersf;
}

//////////////////////////////////////////////////////////////////////
// Extract the body out of the present buffer, returning std::string
//////////////////////////////////////////////////////////////////////

std::string
HttpBuf::body() noexcept {
	std::stringstream tstr;
	std::streamsize n;
	char buf[2048];

	seekg(hdr_epos + hdr_elen);	// Start of body

	while ( tellg() < tellp() ) {
		n = std::stringstream::readsome(buf,sizeof buf);
		tstr.write(buf,n);
	}
	return std::move(tstr.str());
}

//////////////////////////////////////////////////////////////////////
// Write buffer contents out to socket:
//
// RETURNS:
//	< 0	Error
//	1	Success
//////////////////////////////////////////////////////////////////////

int
HttpBuf::write(int fd,writecb_t writecb,void *arg) {
	char buf[2048];
	size_t n, gpos = tellg(), ppos = tellp();
	int rc;

	while ( gpos < ppos ) {
		n = std::stringstream::readsome(buf,sizeof buf);
		if ( n == 0 )
			return 1;		// Success
		rc = writecb(fd,buf,n,arg);
		if ( rc < 0 ) {
			seekg(gpos);
			return rc;		// Fail
		}
		seekg(gpos + rc);
	}
	return 1;				// Success
}

// End httpbuf.cpp
