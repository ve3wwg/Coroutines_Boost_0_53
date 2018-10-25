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
put(std::vector<std::string>& container,const std::string& arg) {
	container.push_back(arg);
}

template<class C>	// Container
void
parse_fields(const char *source,size_t offset,C& container) {
	size_t sz = strlen(source);
	
	auto OWS = [&]() {
		if ( offset >= sz )
			return;
		offset += strspn(source+offset," \t");
		if ( offset > sz )
			offset = sz;
	};
	
	auto field = [&]() -> std::string {
		size_t start_offset = offset;
		offset += strcspn(source + offset," \t;");
		return std::move(std::string(source+start_offset,offset-start_offset));
	};

	do	{
		OWS();		// Skip optional white space
		const std::string word = field();

		if ( !word.empty() )
			put<C>(container,word);

		OWS();
		if ( offset < sz && source[offset] == ';' )
			++offset;
	} while ( offset < sz );
}

#endif // PARSE_HPP

// End parse.hpp
