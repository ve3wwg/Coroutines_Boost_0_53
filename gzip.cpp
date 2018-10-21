//////////////////////////////////////////////////////////////////////
// gzip.cpp -- Gzip compression
// Date: Sat Oct 20 22:38:19 2018   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "gzip.hpp"

Zlib::Zlib(size_t inbufsiz,size_t outbufsiz, write_cb_t write_cb) : write_cb(write_cb) {

	gzip_wbits = 9;

	insize = inbufsiz;
	outsize = outbufsiz;

	inbuf = new unsigned char[insize];
	outbuf = new unsigned char[outsize];

	zstream.next_in = (Bytef *)inbuf;
	zstream.avail_in = 0;
	zstream.next_out = (Bytef *)outbuf;
	zstream.avail_out = outsize;

	if ( insize < 64 )
		in_threshold = 0;
	else    in_threshold = 32;

	if ( outsize < 64 )
		out_threshold = 0;
	else    out_threshold = 32;

	zstream.total_in = 0;
	zstream.total_out = 0;

	zstream.zalloc = Z_NULL;
	zstream.zfree = Z_NULL;
	zstream.opaque = Z_NULL;

	zstream.msg = 0;
	zstream.data_type = Z_BINARY;

	endbuf = inbuf + insize;

	mode = Neither;
}

Zlib::~Zlib() {
	delete []inbuf;
	delete []outbuf;
}

void
Zlib::compress(void *buf,size_t bytes,void *arg) {
        unsigned char *ubuf = (unsigned char *)buf;
        unsigned char *unext = 0;
        int rc;

        if ( mode == Gzip ) {
                rc = deflateInit2(
                        &zstream,
                        Z_BEST_SPEED,		// Low CPU please
                        Z_DEFLATED,		// Method (must be Z_DEFLATED)
                        16 + gzip_wbits,	// windowBits
                        8,			// Memory level
                        Z_DEFAULT_STRATEGY);
                assert(rc == Z_OK);
                mode = Compress;

        } else if ( mode == Neither ) {
                rc = deflateInit(&zstream,Z_BEST_SPEED);
                assert(rc == Z_OK);
                (void)rc;
                mode = Compress;
        }

        assert(mode == Compress);

        for (;;) {
                if ( zstream.avail_out <= out_threshold )
                        write_callback(arg);

                if ( zstream.avail_in > 0 ) {
                        rc = deflate(&zstream,Z_NO_FLUSH);
                        assert( rc == Z_OK || rc == Z_STREAM_END );

                        if ( zstream.avail_in <= 0 ) {
                                zstream.next_in = (Bytef *)inbuf;
                                zstream.avail_in = 0;
                        }
                }
                if ( bytes <= 0 )
                        break;

                unext = (unsigned char *)zstream.next_in + zstream.avail_in;

                if ( unext < endbuf ) {
			// We can stuff more bytes into the in buffer
                        size_t bufbytes = endbuf - unext;

                        if ( bufbytes > bytes )
                                bufbytes = bytes;

                        memcpy(unext,ubuf,bufbytes);
                        zstream.avail_in += bufbytes;
                        
                        ubuf += bufbytes;
                        bytes -= bufbytes;
                }
        }
}

void
Zlib::decompress(void *buf,size_t bytes,void *arg) {
        unsigned char *ubuf = (unsigned char *)buf;
        unsigned char *unext = 0;
        int rc;

        if ( mode == Gzip ) {
		// Use max (15) so any compressed can be decompressed
                rc = inflateInit2(&zstream,16+15);
                assert(rc == Z_OK);
                mode = Decompress;
        }

        if ( mode == Neither ) {
                rc = inflateInit(&zstream);
                assert(rc == Z_OK);
                (void)rc;
                mode = Decompress;
        }

        assert(mode == Decompress);

        for (;;) {
                if ( zstream.avail_out <= out_threshold )
                        write_callback(arg);

                if ( zstream.avail_in > 0 ) {
                        rc = inflate(&zstream,Z_NO_FLUSH);
                        assert( rc == Z_OK || rc == Z_STREAM_END );

                        if ( zstream.avail_in <= 0 ) {
                                zstream.next_in = (Bytef *)inbuf;
                                zstream.avail_in = 0;
                        }
                }
                
                if ( bytes <= 0 )
                        break;

                unext = (unsigned char *)zstream.next_in + zstream.avail_in;

                unext = (unsigned char *)zstream.next_in + zstream.avail_in;

                if ( unext < endbuf ) {
			// We can stuff more bytes into the in buffer
                        size_t bufbytes = endbuf - unext;

                        if ( bufbytes > bytes )
                                bufbytes = bytes;

                        memcpy(unext,ubuf,bufbytes);
                        zstream.avail_in += bufbytes;
                        
                        ubuf += bufbytes;
                        bytes -= bufbytes;
                }
        }
}

void
Zlib::finish(void *arg) {
        int rc = Z_OK;

        assert(mode != Neither);

        if ( zstream.avail_out < outsize )
                write_callback(arg);

        while ( rc != Z_STREAM_END && zstream.avail_in > 0 ) {
                if ( zstream.avail_out > 0 ) {
                        if ( mode == Compress )
                                rc = deflate(&zstream,Z_NO_FLUSH);
                        else    rc = inflate(&zstream,Z_NO_FLUSH);
                        assert(rc == Z_OK || rc == Z_STREAM_END);
                }
                if ( zstream.avail_out < outsize )
                        write_callback(arg);
        }

        while ( rc != Z_STREAM_END ) {
                if ( mode == Compress )
                        rc = deflate(&zstream,Z_FINISH);
                else    rc = inflate(&zstream,Z_FINISH);
                assert(rc == Z_OK || rc == Z_STREAM_END);

                if ( zstream.avail_out < outsize )
                        write_callback(arg);
        }

        if ( mode == Compress )
                rc = deflateEnd(&zstream);
        else    rc = inflateEnd(&zstream);
        assert(rc == Z_OK);
}

void
Zlib::write_callback(void *arg) {
        size_t write_bytes = (unsigned char *)zstream.next_out - outbuf;

        if ( write_bytes > 0 ) {
                write_cb((void *)outbuf,write_bytes,arg);
                zstream.next_out = (Bytef *)outbuf;
                zstream.avail_out = outsize;
        }
}

std::stringstream *
Gzip::compress(std::stringstream& instr,int wbits) {
	std::stringstream& otstr = *new std::stringstream;	// Output compressed stream
	char buf[16*1024];					// instr buffer
	std::streamsize isize = instr.tellp();			// Instream content length
	std::streamsize sn;					// Read length from instr
	
	assert(wbits >= 9);					// See zlib deflateInit2() arg windowBits
	
	// Callback to stuff compressed content into otstr:
	auto callback = [](void *buf,size_t bytes,void *arg) -> void {
		std::stringstream& outstr = *(std::stringstream*)arg;
	
		outstr.write((char *)buf,bytes);
	};
	
	Zlib zlib(8*1024,8*1024,callback);			// Configure compression object zlib
	zlib.gzip_init(10);					// Use gzip format
	instr.seekg(0);						// Rewind the read pointer in instr
	
	while ( instr.tellg() < isize ) {			// While not fully read..
		sn = instr.readsome(buf,sizeof buf);		// Read up to sizeof buf chars
		assert(sn > 0);
		zlib.compress(buf,sn,&otstr);			// Compress what we read
	}
	zlib.finish(&otstr);					// Push final bytes out to otstr
	return &otstr;						// Return new std::stringstream
}

std::stringstream *
Gzip::decompress(std::stringstream& instr,int wbits) {
	std::stringstream& otstr = *new std::stringstream;	// Output uncompressed stream
	char buf[16*1024];					// instr buffer
	std::streamsize isize = instr.tellp();			// Instream content length
	std::streamsize sn;					// Read length from instr
	
	assert(wbits >= 9);					// See zlib deflateInit2() arg windowBits
	
	// Callback to stuff uncompressed content into otstr:
	auto callback = [](void *buf,size_t bytes,void *arg) -> void {
		std::stringstream& outstr = *(std::stringstream*)arg;
	
		outstr.write((char *)buf,bytes);
	};
	
	Zlib zlib(8*1024,8*1024,callback);			// Configure compression object zlib
	
	zlib.gzip_init(wbits);					// Use gzip format
	instr.seekg(0);						// Rewind the read pointer in instr
	
	while ( instr.tellg() < isize ) {			// While not fully read..
		sn = instr.readsome(buf,sizeof buf);		// Read up to sizeof buf chars
		assert(sn > 0);
		zlib.decompress(buf,sn,&otstr);			// Compress what we read
	}
	zlib.finish(&otstr);					// Push final bytes out to otstr
	return &otstr;						// Return new std::stringstream
}
	
std::stringstream *
Gzip::decompress(const char *data,size_t bytes,int wbits) {
	std::stringstream& otstr = *new std::stringstream;	// Output uncompressed stream
	
	assert(wbits >= 9);					// See zlib deflateInit2() arg windowBits
	
	// Callback to stuff uncompressed content into otstr:
	auto callback = [](void *buf,size_t bytes,void *arg) -> void {
		std::stringstream& outstr = *(std::stringstream*)arg;
	
		outstr.write((char *)buf,bytes);
	};
	
	Zlib zlib(8*1024,8*1024,callback);			// Configure compression object zlib
	
	zlib.gzip_init(wbits);					// Use gzip format
	zlib.decompress((void*)data,bytes,&otstr);
	zlib.finish(&otstr);					// Push final bytes out to otstr
	return &otstr;						// Return new std::stringstream
}
	
// End gzip.cpp
