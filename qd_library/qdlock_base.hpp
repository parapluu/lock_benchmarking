#ifndef qdlock_base_hpp
#define qdlock_base_hpp qdlock_base_hpp

#include<cstddef>
#include<future>
#include<type_traits>

#include "util/pause.hpp"
#include "waiting_future.hpp"


/* unpacking promise from delegation queue */
template<typename R, template<class> class Promise = std::promise>
class unpack_promise : public Promise<R> {
	public:
		unpack_promise(char* pptr) : Promise<R>(std::move(*reinterpret_cast<Promise<R>*>(pptr))) {}
		~unpack_promise() {} /* automatically destructs superclass */
		unpack_promise() = delete; /* meaningless */
		unpack_promise(unpack_promise&) = delete; /* not possible */
		unpack_promise(unpack_promise&&) = delete; /* actually implementable, TODO for now */
};


/* Next is a block of templates for delegated function wrappers.
 * They unpack the promise (using a wrapper) and fulfill it in various ways.
 * These are
 * 1) normal functions
 * 2) member functions and functors
 * A) with the function pointer known to the template (pointer known at compile time)
 * B) with the function pointer specified at runtime (signature known at compile time)
 * a) returning a future of some type
 * b) returning a future of void
 * c) returning void (fire and forget delegation)
 */

/* case 2Aa */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_function_future(char* pptr, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value((o.*f)(std::forward<Ps>(ps)...));
}

/* case 2Ba */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_function_future(char* pptr, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value((o.*f)(std::forward<Ps>(ps)...));
}

/* case 1Aa */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_future(char* pptr, Ps&&... ps)
-> typename std::enable_if<std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value(f(std::forward<Ps>(ps)...));
}

/* case 1Ba */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_function_future(char* pptr, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value(f(std::forward<Ps>(ps)...));
}

/* cases Aa?, unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_future(char* buf, Ps&&... ps)
-> typename std::enable_if<!std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_future<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/* cases Ba?, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		true
		//std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_future<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/* case 2Ab */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_void_function_future(char* pptr, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	(o.*f)(std::forward<Ps>(ps)...);
	p.set_value();
}

/* case 2Bb */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_void_function_future(char* pptr, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	(o.*f)(std::forward<Ps>(ps)...);
	p.set_value();
}
/** wrapper function for void operations */
/* 1Ab */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_void_function_future(char* pptr, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	f(ps...);
	p.set_value();
}
/* case 1Bb */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_void_function_future(char* pptr, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	f(std::forward<Ps>(ps)...);
	p.set_value();
}

/* Ab unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_void_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		!std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_void_function_future<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}
/* cases Bb, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_void_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_void_function_future<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/** wrapper function for operations without associated future */
/* case 2Ac */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_function_nofuture(char*, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	(o.*f)(std::forward<Ps>(ps)...);
}

/* case 2Bc */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_function_nofuture(char*, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	(o.*f)(std::forward<Ps>(ps)...);
}

/* case 1Ac */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_nofuture(char*, Ps&&... ps)
-> typename std::enable_if<std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	f(std::forward<Ps>(ps)...);
}

/* case 1Bc */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_function_nofuture(char*, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	f(std::forward<Ps>(ps)...);
}

/* cases Ac, unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_nofuture(char* buf, Ps&&... ps)
-> typename std::enable_if<!std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_nofuture<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/* cases Bc, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_function_nofuture(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		true
		//std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_nofuture<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/**
 * @brief policies for starvation freedom
 */
enum class starvation_policy_t {may_starve, starvation_free};

/**
 * @brief queue delegation base class
 * @tparam MLock mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class MLock, class DQueue, starvation_policy_t starvation_policy=starvation_policy_t::starvation_free>
class qdlock_base {
	public:
		void* __data;
		char pad1[128];
		MLock mutex_lock;
		char pad2[128];
	protected:
		DQueue delegation_queue;

		/** executes the operation */

		/* case 1Aa */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(Promise r, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& !std::is_same<void, decltype(f(ps...))>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
//			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			r.set_value(f(std::move(ps)...));
		}

		/** alternative for operations with a promise, case member function pointer in template, object specified */
		/* case 2Aa */
		template<typename Function, Function f, typename R, typename O, typename... Ps>
		auto execute(std::promise<typename std::result_of<decltype(&O::Function)(O, Ps...)>::type> r, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			r.set_value((o.*f)(std::move(ps)...));
		}

		/** alternative for operations with a promise, case function pointer specified */
		/* case 1Ba */
		template<typename Ignored, Ignored i, typename R, typename Function, typename... Ps>
		auto execute(std::promise<typename std::result_of<Function(Ps...)>::type> r, Function&& f, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& !std::is_same<void, typename std::result_of<Function(Ps...)>::type>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			r.set_value(f(std::move(ps)...));
		}

		/** alternative for operations witht a promise, case member function pointer and object specified */
		/* case 2Ba */
		template<typename Ignored, Ignored i, typename R, typename Function, typename O, typename... Ps>
		auto execute(std::promise<typename std::result_of<Function(Ps...)>::type> r, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			r.set_value((o.*f)(std::move(ps)...));
		}

		/** alternative for operations which return void */
		/* case 1Ab */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(std::promise<void> r, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_same<void, decltype(f(ps...))>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
			f(std::move(ps)...);
			r.set_value();
		}

		/** alternative for operations with a promise, case member function pointer in template, object specified */
		/* case 2Ab */
		template<typename Function, Function f, typename R, typename O, typename... Ps>
		auto execute(std::promise<void> r, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			(o.*f)(std::move(ps)...);
			r.set_value();
		}

		/** alternative for operations with a promise, case function pointer specified */
		/* case 1Bb */
		template<typename Ignored, Ignored i, typename R, typename Function, typename... Ps>
		auto execute(std::promise<void> r, Function&& f, Ps&&... ps)
		-> typename std::enable_if<std::is_same<Ignored, std::nullptr_t>::value, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			f(std::move(ps)...);
			r.set_value();
		}

		/** alternative for operations witht a promise, case member function pointer and object specified */
		/* case 2Bb */
		template<typename Ignored, Ignored i, typename R, typename Function, typename O, typename... Ps>
		auto execute(std::promise<void> r, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			(o.*f)(std::move(ps)...);
			r.set_value();
		}

		/** alternative for operations without a promise, case function pointer in template */
		/* case 1Ac */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(std::nullptr_t, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
			f(std::move(ps)...);
		}

		/** alternative for operations without a promise, case member function pointer in template, object specified */
		/* case 2Ac */
		template<typename Function, Function f, typename Promise, typename O, typename... Ps>
		auto execute(std::nullptr_t, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
			(o.*f)(std::move(ps)...);
		}

		/** alternative for operations without a promise, case function pointer specified */
		/* case 1Bc */
		template<typename Ignored, Ignored i, typename Promise, typename Function, typename... Ps>
		auto execute(std::nullptr_t, Function&& f, Ps&&... ps)
		-> typename std::enable_if<std::is_same<Ignored, std::nullptr_t>::value, void>::type
		{
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			f(std::move(ps)...);
		}

		/** alternative for operations without a promise, case member function pointer and object specified */
		/* case 2Bc */
		template<typename Ignored, Ignored i, typename Promise, typename Function, typename O, typename... Ps>
		auto execute(std::nullptr_t, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			(o.*f)(std::move(ps)...);
		}


		/* ENQUEUE IMPLEMENTATIONS */

		/** maybe enqueues the operation */
		/* case Aa */
		template<typename Function, Function f, typename R, typename... Ps>
		auto enqueue(std::promise<R>* r, Ps*... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& !std::is_same<R, void>::value
			, typename DQueue::status>::type
		{
			static_assert(std::is_same<R, decltype(f((*ps)...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_function_future<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, std::move(ps)..., std::move(r));
		}

		/** alternative with returning a result, case function specified as argument */
		/* case Ba */
		template<typename Ignored, std::nullptr_t i, typename R, typename Function, typename... Ps>
		auto enqueue(std::promise<R>* r, Function* f, Ps*... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& !std::is_same<R, void>::value
			, typename DQueue::status>::type
		{
			static_assert(std::is_same<R, decltype((*f)(*ps...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_function_future<types<Function, Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, std::move(f), std::move(ps)..., std::move(r));
		}

		/** alternative for operations which return void */
		/* case Ab */
		template<typename Function, Function f, typename... Ps>
		auto enqueue(std::promise<void>* r, Ps*... ps)
		-> typename DQueue::status {
			void (*d)(char*) = delegated_void_function_future<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, ps..., std::move(r));
		}
		/** alternative with returning a void, case function specified as argument */
		/* case Bb */
		template<typename Ignored, std::nullptr_t i, typename R, typename... Ps>
		auto enqueue(std::promise<void>* r, Ps*... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
			, typename DQueue::status>::type
		{
			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_void_function_future<types<Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, std::move(ps...), std::move(r));
		}


		/** alternative without returning a result, case function specified in template */
		/* case Ac */
		template<typename Function, Function f, typename... Ps>
		auto enqueue(std::nullptr_t*, Ps*... ps)
		-> typename std::enable_if<!std::is_same<Function, std::nullptr_t>::value, typename DQueue::status>::type
		{
			void (*d)(char*) = delegated_function_nofuture<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, ps...);
		}

		/** alternative without returning a result, case function specified as argument */
		/* case Bc */
		template<typename Ignored, std::nullptr_t i, typename... Ps>
		auto enqueue(std::nullptr_t*, Ps*... ps)
		-> typename DQueue::status
		{
			void (*d)(char*) = delegated_function_nofuture<types<Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, ps...);
		}

	public:
		struct no_promise {
			typedef std::nullptr_t promise;
			typedef std::nullptr_t future;
			static promise create_promise(promise**) {
				return nullptr;
			}
			static future create_future(promise) {
				return nullptr;
			}
		};

		struct no_reader_sync {
			static void wait_writers(qdlock_base<MLock, DQueue, starvation_policy>*) {};
			static void wait_readers(qdlock_base<MLock, DQueue, starvation_policy>*) {};
		};
		struct no_hierarchy_sync {
			static void lock(qdlock_base<MLock, DQueue, starvation_policy>*) {};
			static void unlock(qdlock_base<MLock, DQueue, starvation_policy>*) {};
		};
#if 0
		template<typename T>
		class wrapped_promise : public std::promise<T> {
			wrapped_promise** co_owner;
			public:
				wrapped_promise() = delete;
				wrapped_promise(wrapped_promise** ptr) : co_owner(ptr) {
					*co_owner = this;
				}
				wrapped_promise(wrapped_promise&) = delete;
				wrapped_promise(wrapped_promise&& rhs) : std::promise<T>(std::move(rhs)), co_owner(rhs.co_owner), active(rhs.active) {
					if(active) *co_owner = this;
				}
				wrapped_promise& operator=(wrapped_promise&) = delete;
				wrapped_promise& operator=(wrapped_promise&& rhs) {
					std::promise<T>::operator=(std::move(rhs));
					co_owner = rhs.co_owner;
					active = rhs.active;
					if(active) *co_owner = this;
					return *this;
				}
		};
#endif

		template<typename R>
		struct std_promise {
			typedef std::promise<R> promise;
			typedef waiting_future<R> future;
			static promise create_promise() {
				return promise();
			}
			static future create_future(promise& p) {
				return p.get_future();
			}
		};
	private:
		template<typename Function, Function f, typename Promise, typename RSync, typename HSync, typename... Ps>
		auto helper(Promise&& result, Ps&&... ps)
		-> void {
			HSync::lock(this);
			this->delegation_queue.open();
			RSync::wait_readers(this);
			execute<Function, f, Promise, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			this->delegation_queue.flush();
			HSync::unlock(this);
#ifdef QUEUE_STATS
			helpSeasonsPerformed.value++;
			numberOfDeques.value++;
#endif
		}

		template<typename Function, Function f, typename Promise, typename RSync, typename HSync, typename... Ps>
		auto executor(Promise&& result, Ps&&... ps)
		-> void {
			HSync::lock(this);
			RSync::wait_readers(this);
			execute<Function, f, Promise, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			HSync::unlock(this);
		}

		template<typename Function, Function f, typename Promise, typename... Ps>
		bool try_enqueue(int attempts, Promise* result, Ps... ps) {
			typename DQueue::status status = DQueue::status::CLOSED;;
			for(int i = 1; i <= attempts; i++) {
				status = enqueue<Function, f>(result, ps...);
				if(status == DQueue::status::SUCCESS) {
					return true;
				} else if(status == DQueue::status::FULL) {
					qd::pause();
					//break;
				} else {
					qd::pause();
				}
			}
			return false;
		}
	public:
		//-> typename std::conditional<std::is_same<std::nullptr_t, typename Promise::promise>::value, void, typename Promise::future>::type
		template<typename Function, Function f, typename Promise, typename RSync, typename HSync, typename... Ps>
		auto delegate(Promise&& result, Ps&&... ps)
		-> void
		{
			RSync::wait_writers(this);

			if(this->mutex_lock.try_lock()) {
				executor<Function, f, Promise, RSync, HSync, Ps...>(std::move(result), std::forward<Ps>(ps)...);
				this->mutex_lock.unlock();
				return;
			}
			/* for guaranteed starvation freedom add a limit here */
			for(unsigned int retries = 1; (starvation_policy == starvation_policy_t::may_starve) || retries < 512; retries++) {
				/* retry enqueueing a couple of times if CLOSED */
				if (try_enqueue<Function, f>(1, &result, (&ps)...)) {
					return;
				}
				bool lock_acquired;
				/** @todo magic number 127 */
				if(retries % (127 + 1) == 0) {
					lock_acquired = this->mutex_lock.try_lock_or_wait();
				} else {
					lock_acquired = this->mutex_lock.try_lock();
				}
				if(lock_acquired) {
					helper<Function, f, Promise, RSync, HSync, Ps...>(std::move(result), std::forward<Ps>(ps)...);
					this->mutex_lock.unlock();
					return;
				}
				std::this_thread::yield();
			}
			/* if starvation_policy is may_starve, then this is dead code, only relevant for the starvation_free variant */
			this->mutex_lock.lock();
			executor<Function, f, Promise, RSync, HSync, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			this->mutex_lock.unlock();
		}
};

#endif /* qdlock_base_hpp */
