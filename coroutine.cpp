//////////////////////////////////////////////////////////////////////
// coroutine.cpp -- Unit test for CoroutineBase class
// Date: Sat Aug 11 09:01:34 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <iostream>

#include "coroutine.hpp"

CoroutineMain mco;
Coroutine *cr1, *cr2;

static CoroutineBase *
fun1(CoroutineBase *co) {
	static int x=0;

	std::cout << "fun1::a x=" << x++ << '\n';
	co->yield(*cr2);

	std::cout << "fun1::b x=" << x++ << '\n';
	co->yield(*cr2);

	return co;
}

static CoroutineBase *
fun2(CoroutineBase *co) {
	static int x=0;

	std::cout << "fun2::a x=" << x++ << '\n';
	co->yield(mco);

	std::cout << "fun2::b x=" << x++ << '\n';
	co->yield(mco);

	return co;
}

int
main(int argc,char **argv) {
	Coroutine co1(fun1), co2(fun2);

	cr1 = &co1;
	cr2 = &co2;
	mco.yield(co1);
	std::cout << "Back to main a..\n";
	mco.yield(co1);
	std::cout << "Back to main b..\n";
	return 0;
}

// End coroutine.cpp
