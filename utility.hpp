//////////////////////////////////////////////////////////////////////
// utility.hpp -- Utility Functions
// Date: Fri Oct 12 22:09:34 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <time.h>
#include <string.h>
#include <string>

void ucase_buffer(char *buf);
timespec& timeofday(timespec &tod);

inline
bool operator==(const timespec& a,const timespec &b) noexcept {
	return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

inline
bool operator!=(const timespec& a,const timespec &b) noexcept {
	return a.tv_sec != b.tv_sec || a.tv_nsec != b.tv_nsec;
}

inline
bool operator<(const timespec& a,const timespec &b) noexcept {
	return a.tv_sec < b.tv_sec || ( a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec );
}

inline
bool operator>(const timespec& a,const timespec &b) noexcept {
	return a.tv_sec > b.tv_sec || ( a.tv_sec == b.tv_sec && a.tv_nsec > b.tv_nsec );
}

inline
bool operator<=(const timespec& a,const timespec &b) noexcept {
	if ( a.tv_sec > b.tv_sec )
		return false;
	if ( a.tv_sec == b.tv_sec )
		return a.tv_nsec <= b.tv_nsec;
	else	return true;
}

inline
bool operator>=(const timespec& a,const timespec &b) noexcept {
	if ( a.tv_sec < b.tv_sec )
		return false;
	if ( a.tv_sec == b.tv_sec )
		return a.tv_nsec >= b.tv_nsec;
	else	return true;
}

inline
timespec& operator+=(timespec& left,const timespec& right) {
	left.tv_sec += right.tv_sec;
	left.tv_nsec += right.tv_nsec;
	if ( left.tv_nsec > 1000000000L ) {
		left.tv_sec += left.tv_nsec / 1000000000L;
		left.tv_nsec %= 1000000000L;
	}
	return left;
}

inline
timespec& operator-=(timespec& left,const timespec& right) {
	left.tv_sec -= right.tv_sec;
	if ( left.tv_nsec < right.tv_nsec ) {
		left.tv_sec--;
		left.tv_nsec += 1000000000L;
	}
	left.tv_nsec -= right.tv_nsec;
	return left;
}

inline
long millisecs(const timespec &tspec) {
	return tspec.tv_sec * 1000L + tspec.tv_nsec / 1000000L;
}

struct s_casehash {
	inline size_t operator()(const std::string& key) const noexcept {
		unsigned sz = unsigned(key.size());
		const char *keyp = key.c_str();
		char ch;
		size_t h = 0;

		while ( sz-- > 0 ) {
			ch = *keyp++;
			if ( ch >= 'a' && ch <= 'z' )
				ch ^= 0x20;
			h += size_t(ch);
		}
		return h;
	}
};

struct s_casecmp {
	inline bool operator()(const std::string& left,const std::string& right) const noexcept {
		return !strcasecmp(left.c_str(),right.c_str());
	}
};

#endif // UTILITY_HPP

// End utility.hpp
