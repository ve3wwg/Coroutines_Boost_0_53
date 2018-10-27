//////////////////////////////////////////////////////////////////////
// parse.hpp -- Parsing tools
// Date: Wed Oct 24 21:48:50 2018   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#ifndef PARSE_HPP
#define PARSE_HPP

#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "utility.hpp"

template<class C>
void
put(std::set<std::string>& container,const std::string& arg) {
	container.insert(arg);
}

template<class C>
void
put(std::unordered_set<std::string>& container,const std::string& arg) {
	container.insert(arg);
}

template<class C>
void
put(std::unordered_set<std::string,s_casehash,s_casecmp>& container,const std::string& arg) {
	container.insert(arg);
}

template<class C>
void
put(std::vector<std::string>& container,const std::string& arg) {
	container.push_back(arg);
}

template<class C>	// Container
void
parse_fields(C& container,const char *source) {
	int n;
	
	for (;;) {
		source += strspn(source," \t;");
		if ( !*source )
			return;

		n = strcspn(source," \t;\r\n");
		std::string word(source,n);

		if ( !word.empty() )
			put<C>(container,word);

		source += size_t(n);
	}
}

//////////////////////////////////////////////////////////////////////
// Parse source into a comma separated list of keyword=value pairs.
//////////////////////////////////////////////////////////////////////

template<class M>		// Map container
void
parse_tomap(M& map,const char *source) {
	std::string segment;
	char *v;
	int n;

	for (;;) {
		n = strspn(source," ,\t\r\n");
		source += n;
		if ( !*source )
			return;

		n = strcspn(source," \t,\r\n");
		if ( n <= 0 )
			continue;		// Empty value

		segment.assign(source,size_t(n));
		v = (char *)strchr(segment.c_str(),'=');
		if ( v ) {
			*v++ = 0;		// NB: This modifies internal data of segment
			map[segment] = v;	// Keyword = data pair
		} else	{
			map[segment];		// Just keyword
		}
		source += size_t(n);
	}
}

//////////////////////////////////////////////////////////////////////
// Parse fields separated by ';' into keyword maps
//////////////////////////////////////////////////////////////////////

template<class M>				// Map within vector
void
parse_fields(std::vector<M>& vector,const char *source) {
	std::string segment;
	int n;

	for (;;) {
		source += strspn(source," \t;\r\n");
		n = strcspn(source,";\r\n");

		if ( !*source )
			return;
		if ( n > 0 )
			segment.assign(source,n);
		else	segment = source;

		vector.emplace_back();
		M& map = vector.back();

		parse_tomap(map,segment.c_str());
		if ( map.empty() )
			vector.erase(vector.end()-1);
		source += size_t(n);
	}
}

#endif // PARSE_HPP

// End parse.hpp
