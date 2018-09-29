//////////////////////////////////////////////////////////////////////
// httpbuf.hpp -- http stream buffer
// Date: Thu Sep 20 15:10:41 2018   (C) Warren Gay
///////////////////////////////////////////////////////////////////////

#ifndef HTTPBUF_HPP
#define HTTPBUF_HPP

#include "iobuf.hpp"

class HttpBuf : public IOBuf {
	typedef int (*readcb_t)(int fd,void *buf,size_t bytes,void *arg);

	enum {
		S0CR1, S0LF1, S0CR2, S0LF2,	// CR LF CR LF
		S1LF2				// LF LF 
	}	state = S0CR1;

	size_t	hdr_epos = 0;
	short	hdr_elen = 0;

public:	HttpBuf() {};
	void reset() noexcept;
	bool have_end(size_t *pepos,size_t *pelen) noexcept;
	int read_header(int fd,readcb_t,void *arg) noexcept;
};

#endif // HTTPBUF_HPP

// End httpbuf.hpp
