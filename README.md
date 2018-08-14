Coroutine Class using Boost 1.53
--------------------------------

Requires boost_context 1.53.0 library.

CoroutineMain:
--------------
    supplies the context for a main thread, already owning
    a thread (line a main() program).

Coroutine:
----------
    is used for a new context, requiring a stack
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
    	int x=10;
    
    	std::cout << "fun1::a x=" << x++ << '\n';
    	co->yield(*cr2);
    
    	std::cout << "fun1::b x=" << x++ << '\n';
    	co->yield(*cr2);
    
    	return co;
    }
    
    static CoroutineBase *
    fun2(CoroutineBase *co) {
    	int x=20;
    
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
    
    $ ./coroutine 
    fun1::a x=10
    fun2::a x=20
    Back to main a..
    fun1::b x=11
    fun2::b x=21
    Back to main b..

Server Example:
---------------

    $ ./server 

    Will cause it to listen to 127.0.0.1:2345

Client Test:
------------

    $ wget http://127.0.0.1:2345/whatever/params?pig=oink --save-headers -qO-
    HTTP/1.1 200 OK 
    Connection: Keep-Alive
    Content-Length: 209

    Request type: GET
    Request path: /whatever/params?pig=oink
    Http Version: HTTP/1.1
    Request Headers were:
    Hdr: HOST: 127.0.0.1:2345
    Hdr: CONNECTION: Keep-Alive
    Hdr: ACCEPT: */*
    Hdr: USER-AGENT: Wget/1.16
    $ 

