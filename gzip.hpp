//////////////////////////////////////////////////////////////////////
// gzip.hpp -- Gzip compression
// Date: Sat Oct 20 22:37:16 2018   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#ifndef GZIP_HPP
#define GZIP_HPP

#include <stdint.h>
#include "zlib.h"

#include <string>
#include <sstream>

class Zlib {
	typedef void (*write_cb_t)(void *buf,size_t bytes,void *arg);

	unsigned char	*inbuf;		// Allocated input buffer
	size_t		insize;		// Input buffer size
	unsigned char	*endbuf;	// Points one byte past end of input buffer
	size_t		in_threshold;	// Input threshold

	unsigned char	*outbuf;	// Output buffer
	size_t		outsize;	// Output buffer size
	z_stream_s	zstream;	// Zlib stream object
	size_t		out_threshold;	// Threshold for write callback

	int		gzip_wbits;	// windowBits for defaultInit2()

	enum Mode {
		Compress,
		Decompress,
		Neither,
		Gzip
	}		mode;

public:
	write_cb_t	write_cb;	// Write callback

protected:
	void init(Mode arg_mode,size_t inbufsize,size_t outbufsize);
	void write_callback(void *arg);

public:	Zlib(size_t inbufsiz,size_t outbufsize,write_cb_t write_cb);
	~Zlib();

	void gzip_init(int wbits=9)     { mode = Gzip; gzip_wbits=wbits; } // Optional: Use gzip format

	void compress(void *buf,size_t bytes,void *arg);
	void decompress(void *buf,size_t bytes,void *arg);
	void finish(void *arg);
	
	static uint32_t crc32(void *buf,size_t buflen);
};

namespace Gzip {
	std::stringstream *compress(std::stringstream& instr,int wbits=9);
	std::stringstream *decompress(std::stringstream& instr,int wbits=15);
	std::stringstream *decompress(const char *data,size_t bytes,int wbits=15);
}

#endif // GZIP_HPP

// End gzip.hpp
