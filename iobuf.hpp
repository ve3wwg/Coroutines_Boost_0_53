//////////////////////////////////////////////////////////////////////
// iobuf.hpp -- More generalized I/O Buffer derived from std::stringstream
// Date: Sat Sep 29 11:15:04 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef IOBUF_HPP
#define IOBUF_HPP

#include <sstream>
#include <string>

class IOBuf : public std::stringstream {

public:	IOBuf() {};
	void reset() noexcept;
	std::string sample() noexcept;		// Non-destructive sample of std::stringstream
};

#endif // IOBUF_HPP

// End iobuf.hpp
