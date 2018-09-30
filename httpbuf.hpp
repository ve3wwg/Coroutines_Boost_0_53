//////////////////////////////////////////////////////////////////////
// httpbuf.hpp -- http stream buffer
// Date: Thu Sep 20 15:10:41 2018   (C) Warren Gay
///////////////////////////////////////////////////////////////////////

#ifndef HTTPBUF_HPP
#define HTTPBUF_HPP

#include <string>
#include <unordered_map>

#include "iobuf.hpp"

class HttpBuf : public IOBuf {
	typedef int (*readcb_t)(int fd,void *buf,size_t bytes,void *arg);
	typedef int (*writecb_t)(int fd,const void *buf,size_t bytes,void *arg);

	enum {
		S0CR1, S0LF1, S0CR2, S0LF2,	// CR LF CR LF
		S1LF2				// LF LF 
	}	state = S0CR1;

	size_t	hdr_epos = 0;
	short	hdr_elen = 0;

public:	HttpBuf() {};
	void reset() noexcept;
	bool have_end(size_t *pepos,size_t *pelen) noexcept; 				// True if we have read end of header
	int read_header(int fd,readcb_t readcb,void *arg) noexcept; 			// Read up to end of header
	int read_body(int fd,readcb_t readcb,void *arg,size_t content_length) noexcept;	// Ready full body
	int write(int fd,writecb_t,void *arg) noexcept;					// Write buffer to fd
	std::string body() noexcept;		// Extract body
	size_t parse_headers(
	  std::string& reqtype,			// Out: GET/POST
	  std::string& path,			// Out: Path component
	  std::string& httpvers,		// Out: HTTP/x.x
	  std::unordered_multimap<std::string,std::string>& headers, // Out: Parsed headers
	  size_t maxhdr=2048			// In:  Max size of headers buffer for parsing
	) noexcept;
};

#endif // HTTPBUF_HPP

// End httpbuf.hpp
