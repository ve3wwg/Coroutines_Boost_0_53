Coroutine Class using Boost 1.53
--------------------------------

Requires boost_context 1.53.0 library.

CoroutineMain:
--------------
    supplies the context for a main thread, already owning
    a thread (line a main() program).

Coroutine:
----------
    is used for a new context, requiring a thread
    to be allocated.

    Coroutine co1(myfunc[,stack_size]);

    When stack_size is not supplied, a minimum stack is
    allocated.

CoroutineBase:
--------------

    class from which Coroutine and CoroutineMain classes
    are derived.
        
CoroutineBase::yield(CoroutineBase& co)
---------------------------------------

    This method transfers control from the current coroutine
    (or main thread) to the new coroutine.

CoroutineBase::fun_t
--------------------

    This is the function pointer to the code to be run as
    a coroutine. It is passed the CoroutineBase pointer 
    of the executing coroutine (either Coroutine or 
    CoroutineMain class). The function must return its
    Coroutine class pointer. 

    Control is yielded by specifying the Coroutine class
    to pass control to.

    To pass data, derive a class from Coroutine or Coroutine
    main as required.

    CoroutineBase *fun1(CoroutineBase *co) {

        co->yield(...);
        ...
        return co;
    }

Starting a coroutine from main:
-------------------------------

    CoroutineMain mco;
        
    int main(int argc,char **argv) {
        Coroutine co1(fun1);

        mco.yield(co1);
        
Build:
------

    make [BOOST=$HOME/local]

    ./coroutine # test program

Test Example:
-------------

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

Example Output:
---------------

fun1::a x=0
fun2::a x=0
Back to main a..
fun1::b x=1
fun2::b x=1
Back to main b..

