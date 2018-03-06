#ifndef qd_qdlock_hpp
#define qd_qdlock_hpp qdlock_hpp

#include "qdlock_base.hpp"


/**
 * @brief queue delegation lock implementation
 * @tparam MLock mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class MLock, class DQueue, starvation_policy_t starvation_policy=starvation_policy_t::starvation_free>
class qdlock_impl : private qdlock_base<MLock, DQueue, starvation_policy> {
	typedef qdlock_base<MLock, DQueue, starvation_policy> base;
	template<class, class> friend class qd_condition_variable_impl;
	public:
		typedef typename base::no_reader_sync reader_indicator_t;
		typedef typename base::no_hierarchy_sync hierarchy_t;
#if 0
		/**
		 * @brief delegate function
		 * @tparam R return type of delegated operation
		 * @tparam Ps parameter types of delegated operation
		 * @param f the delegated operation
		 * @param ps the parameters for the delegated operation
		 * @return a future for return value of delegated operation
		 */
#endif

		/* the following delegate_XX functions are all specified twice:
		 * First for a templated version, where a function is explicitly
		 * given in the template argument list. (Porbably using a macro)
		 * Second for a version where the function or functor is a
		 * normal parameter to the delegate_XX function.
		 *
		 * XX specifies how futures are dealt with:
		 * n  - no futures
		 * f  - returns future for return value of delegated operation
		 * p  - delegated operation returns void and takes a promise as
		 *      first argument, which is passed here by the user.
		 * fp - returns future for a type which is specified in the
		 *      template parameter list. The delegated operation takes
		 *      a promise for that type as its first argument.
		 */

		/* interface _n functions: Do not wait, do not provide a future. */
		template<typename Function, Function f, typename... Ps>
		void delegate_n(Ps&&... ps) {
			/* template provides function address */
			using promise_t = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			base::template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_t = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			base::template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(nullptr, std::forward<Function>(f), std::forward<Ps>(ps)...);
		}

		/* interface _f functions: Provide a future for the return value of the delegated operation */
		template<typename Function, Function f, typename... Ps>
		auto delegate_f(Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			using reader_sync_t = typename base::no_reader_sync;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		template<typename Function, typename... Ps>
		auto delegate_f(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			using reader_sync_t = typename base::no_reader_sync;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(std::move(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
			return future;
		}

		/* interface _p functions: User provides a promise, which is used explicitly by the delegated (void) function */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto delegate_p(Promise&& result, Ps&&... ps)
		-> void
		{
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			base::template delegate<Function, f, no_promise, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}
		template<typename Function, typename Promise, typename... Ps>
		auto delegate_p(Function&& f, Promise&& result, Ps&&... ps)
		-> void
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			base::template delegate<std::nullptr_t, nullptr, no_promise, reader_sync_t, hierarchy_t, Function, Promise, Ps...>(nullptr, std::forward<Function>(f), std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}

		/* interface _fp functions: Promise is generated here, but delegated function uses it explicitly. */
		template<typename Return, typename Function, Function f, typename... Ps>
		auto delegate_fp(Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename promise_factory::promise;
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<Function, f, no_promise, reader_sync_t, hierarchy_t, promise_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		template<typename Return, typename Function, typename... Ps>
		auto delegate_fp(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename promise_factory::promise;
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<std::nullptr_t, nullptr, no_promise, reader_sync_t, hierarchy_t, Function, promise_t, Ps...>(nullptr, std::forward<Function>(f), std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		

		void lock() {
			this->mutex_lock.lock();
		}
		void unlock() {
			this->mutex_lock.unlock();
		}
};

#endif /* qd_qdlock_hpp */
