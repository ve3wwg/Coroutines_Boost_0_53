//////////////////////////////////////////////////////////////////////
// circarray.hpp -- Circular Array
// Date: Wed Oct 10 21:17:50 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef CIRCARRAY_HPP
#define CIRCARRAY_HPP

#include <stdlib.h>

template<typename T>
class CircArray {
	size_t		sz;
	size_t		headx=0;
	T		*array;

public:	CircArray(size_t size);
	~CircArray();
	size_t size() const noexcept 	{ return sz; }
	T& advance(size_t n) noexcept;
	T& operator[](size_t x) noexcept;
};

template<typename T>
CircArray<T>::CircArray(size_t size) : sz(size) {

	array = new T[size];
	for ( size_t x=0; x<size; ++x )
		array[x] = T();
}

template<typename T>
CircArray<T>::~CircArray() {
	delete array;
	array = nullptr;
}

template<typename T>
T&
CircArray<T>::advance(size_t n) noexcept {
	headx = (headx + n) % sz;
	return array[headx];
}

template<typename T>
T&
CircArray<T>::operator[](size_t x) noexcept {
	
	if ( x >= sz )
		abort();
	return array[(headx + x) % sz];
}

#endif // CIRCARRAY_HPP

// End circarray.hpp
