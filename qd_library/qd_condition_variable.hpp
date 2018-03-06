#ifndef qd_condition_variable_hpp
#define qd_condition_variable_hpp qd_condition_variable_hpp

#include "util/pause.hpp"
#include "qdlock_base.hpp"

template<class MLock, class DQueue>
class qd_condition_variable_impl : private qdlock_base<MLock, DQueue> {
	typedef qdlock_base<MLock, DQueue> base;
	public:
		qd_condition_variable_impl() {
			this->delegation_queue.open();
		}
		qd_condition_variable_impl(const qd_condition_variable_impl&) = delete;
		qd_condition_variable_impl& operator=(const qd_condition_variable_impl&) = delete;

		/* TODO: these notify implementations / flush implementations run at risk of deadlocking
		 *       when the notifying thread becomes a helper and also needs to perform additional
		 *       synchronization steps.
		 */
		void notify_one() {
			this->mutex_lock.lock();
			this->delegation_queue.flush_one();
			this->mutex_lock.unlock();
		}
		void notify_all() {
			this->mutex_lock.lock();
			this->delegation_queue.flush();
			this->mutex_lock.unlock();
		}
		
		/* interface _p functions: User provides a promise, which is used explicitly by the delegated (void) function */
		template<typename Function, Function f, typename Lock, typename Promise, typename... Ps>
		auto wait_redelegate_p(Lock* l, Promise&& result, Ps&&... ps)
		-> void
		{
			wait_redelegate<Function, f, Lock, Promise, Ps...>(l, std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}
		template<typename Function, typename Lock, typename Promise, typename... Ps>
		auto wait_redelegate_p(Function&& f, Lock* l, Promise&& result, Ps&&... ps)
		-> void
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			wait_redelegate<std::nullptr_t, nullptr, Lock, Promise, Function, Ps...>(l, std::forward<Promise>(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
	private:
		
		
		template<typename Function, Function f, typename Lock, typename Promise, typename... Ps>
		auto wait_redelegate(Lock* l, Promise&& result, Ps&&... ps)
		-> void
		{
			while(true) {
				/* TODO enqueue a function that re-delegates the provided function with its parameter to Lock l TODO */
				std::nullptr_t no_promise;
				if(this->template enqueue<void (*)(Lock*, typename Promise::promise&&, Ps&&...), redelegate<Lock, Function, f, Promise&&, Ps&&...>>(&no_promise, &l, &result, (&ps)...)) {
					return;
				}
				qd::pause();
			}
		}
		template<typename Lock, typename Function, Function f, typename Promise, typename... Ps>
		static void redelegate(Lock* l, Promise&& p, Ps&&... ps) {
			using no_promise = typename base::no_promise::promise;
			l->template delegate<Function, f, no_promise, typename Lock::reader_indicator_t, Promise, Ps...>(nullptr, std::forward<Promise>(p), std::forward<Ps>(ps)...);
	}

	/* TODO: wait_for and wait_until for time-based waiting */

};

#endif
