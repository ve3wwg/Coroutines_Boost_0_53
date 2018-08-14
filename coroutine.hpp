//////////////////////////////////////////////////////////////////////
// coroutine.hpp -- Coroutine template class
// Date: Sat Aug 11 08:45:33 2018   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef COROUTINE_HPP
#define COROUTINE_HPP

#include <boost/coroutine/all.hpp>
#include <boost/context/all.hpp>

//////////////////////////////////////////////////////////////////////
// Base Coroutine Context
//////////////////////////////////////////////////////////////////////

class CoroutineBase {
public:	typedef CoroutineBase * (fun_t)(CoroutineBase *);

protected:
	using context_t = boost::context::fcontext_t;

	context_t	*fc=nullptr;
	CoroutineBase	*caller=nullptr;

public:	CoroutineBase() {}
	virtual ~CoroutineBase() {};
	inline virtual CoroutineBase* yield(CoroutineBase& coro);
	inline virtual CoroutineBase* yield() { return yield(*caller); }
	inline virtual CoroutineBase* yield_with(CoroutineBase& coro,void *vptr);
	inline virtual CoroutineBase* yield_with(void *vptr) { return yield_with(*caller,vptr); }
	inline virtual CoroutineBase* get_caller() noexcept { return caller; }
};

//////////////////////////////////////////////////////////////////////
// Main Context : where stack already exists
//////////////////////////////////////////////////////////////////////

class CoroutineMain : public CoroutineBase {
	context_t	main_ctx;

public:	CoroutineMain() { fc = &main_ctx; }
};

//////////////////////////////////////////////////////////////////////
// Coroutine Context : Where stack is allocated by constructor
//////////////////////////////////////////////////////////////////////

class Coroutine : public CoroutineBase {
	void		*sp=nullptr;	// Stack pointer
	std::size_t	stack_size=0;	// Stack's size

public:	inline Coroutine(fun_t,size_t stacksize=0);
	inline ~Coroutine();

	context_t *context() noexcept	{ return fc; }
};

//////////////////////////////////////////////////////////////////////
// Implementation:
//////////////////////////////////////////////////////////////////////

Coroutine::Coroutine(fun_t fun,size_t stacksize) {
	boost::coroutines::stack_allocator alloc;

	if ( !stacksize )
		stack_size = boost::coroutines::stack_allocator::minimum_stacksize();
	else	stack_size = stacksize;
	sp = alloc.allocate(stack_size);
	fc = boost::context::make_fcontext(sp,stack_size,(void (*)(intptr_t))fun);
}

Coroutine::~Coroutine() {
	boost::coroutines::stack_allocator alloc;

	if ( sp ) {
		alloc.deallocate(sp,stack_size);
		sp = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////
// Yield method: Argument is context that you're transferring to
//////////////////////////////////////////////////////////////////////

CoroutineBase *
CoroutineBase::yield(CoroutineBase& to) {
	to.caller = this;		// Save ref to calling object
	return (Coroutine*) boost::context::jump_fcontext(fc,to.fc,(intptr_t)&to);
}

CoroutineBase *
CoroutineBase::yield_with(CoroutineBase& to,void *vptr) {
	to.caller = this;		// Save ref to calling objectt
	return (Coroutine*) boost::context::jump_fcontext(fc,to.fc,(intptr_t)vptr);
}

#endif // COROUTINE_HPP

// End coroutine.hpp
