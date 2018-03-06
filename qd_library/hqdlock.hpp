#ifndef qd_hqdlock_hpp
#define qd_hqdlock_hpp qd_hqdlock_hpp

#include <numa.h>
#include "qdlock_base.hpp"

/**
 * @brief policies for thread pinning
 */
enum class pinning_policy_t {free_threads, pinned_threads};

/**
 * @brief hierarchical queue delegation lock implementation
 * @tparam GMLock global mutual exclusion lock
 * @tparam NMLock node-local mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class GMLock, class NMLock, class DQueue, pinning_policy_t pinning_policy=pinning_policy_t::free_threads, starvation_policy_t starvation_policy=starvation_policy_t::starvation_free>
class hqdlock_impl {
	template<class, class> friend class qd_condition_variable_impl;
	
	typedef qdlock_base<NMLock, DQueue, starvation_policy> base;
	typedef hqdlock_impl<GMLock, NMLock, DQueue, pinning_policy, starvation_policy>* this_t;
	struct hierarchy_sync {
		static void lock(base* t) {
			reinterpret_cast<this_t>(t->__data)->global_lock.lock();
		};
		static void unlock(base* t) {
			reinterpret_cast<this_t>(t->__data)->global_lock.unlock();
		};
	};
	typedef hierarchy_sync hierarchy_t;
	typedef typename base::no_reader_sync reader_indicator_t;
	
	GMLock global_lock;
	char pad1[128];
	int numanodes;
	std::vector<int> numa_mapping;
	base* qdlocks;
	thread_local static bool node_id_init;
	thread_local static int node_id;
	base& get_qd() {
		int idx;
		if(pinning_policy == pinning_policy_t::free_threads) {
			int cpu = sched_getcpu();
			idx = numa_mapping[cpu];
		} else {
			if(!node_id_init) {
				node_id = sched_getcpu();
				node_id_init = true;
			}
			idx = numa_mapping[node_id];
		}
		return qdlocks[idx];
	}
	public:

		hqdlock_impl() {
			int amount = numa_num_configured_nodes();
			numanodes = amount;
			qdlocks = new base[amount];
			for(int a = 0; a < amount; a++) {
				qdlocks[a].__data = reinterpret_cast<void*>(this);
			}
			int num_cpus = numa_num_configured_cpus();
			numa_mapping.resize(num_cpus);
			for (int i = 0; i < num_cpus; ++i) {
				numa_mapping[i] = numa_node_of_cpu(i);
			}
		}
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
			get_qd().template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_t = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			get_qd().template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(nullptr, std::forward<Function>(f), std::forward<Ps>(ps)...);
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
			get_qd().template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
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
			get_qd().template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(std::move(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
			return future;
		}

		/* interface _p functions: User provides a promise, which is used explicitly by the delegated (void) function */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto delegate_p(Promise&& result, Ps&&... ps)
		-> void
		{
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			get_qd().template delegate<Function, f, no_promise, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}
		template<typename Function, typename Promise, typename... Ps>
		auto delegate_p(Function&& f, Promise&& result, Ps&&... ps)
		-> void
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using no_promise = typename base::no_promise::promise;
			using reader_sync_t = typename base::no_reader_sync;
			get_qd().template delegate<std::nullptr_t, nullptr, no_promise, reader_sync_t, hierarchy_t, Function, Promise, Ps...>(nullptr, std::forward<Function>(f), std::forward<Promise>(result), std::forward<Ps>(ps)...);
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
			get_qd().template delegate<Function, f, no_promise, reader_sync_t, hierarchy_t, promise_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
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
			get_qd().template delegate<std::nullptr_t, nullptr, no_promise, reader_sync_t, hierarchy_t, Function, promise_t, Ps...>(nullptr, std::forward<Function>(f), std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		
		/* TODO? the following functions could also implement a cohort lock */
		void lock() {
			get_qd().mutex_lock.lock();
			this->global_lock.lock();
		}
		void unlock() {
			this->global_lock.unlock();
			get_qd().mutex_lock.unlock();
		}
};

template<class GMLock, class NMLock, class DQueue, pinning_policy_t pinning_policy, starvation_policy_t starvation_policy>
thread_local bool hqdlock_impl<GMLock, NMLock, DQueue, pinning_policy, starvation_policy>::node_id_init = false;
template<class GMLock, class NMLock, class DQueue, pinning_policy_t pinning_policy, starvation_policy_t starvation_policy>
thread_local int hqdlock_impl<GMLock, NMLock, DQueue, pinning_policy, starvation_policy>::node_id;

template<class GMLock, class NMLock, class DQueue, class RIndicator, int READ_PATIENCE_LIMIT, pinning_policy_t pinning_policy=pinning_policy_t::free_threads, starvation_policy_t starvation_policy=starvation_policy_t::starvation_free>
class mrhqdlock_impl {
	char pad1[128];
	std::atomic<int> writeBarrier;
	char pad2[128];
	RIndicator reader_indicator;
	char pad3[128];
	typedef mrhqdlock_impl<GMLock, NMLock, DQueue, RIndicator, READ_PATIENCE_LIMIT, pinning_policy, starvation_policy>* this_t;
	typedef qdlock_base<NMLock, DQueue, starvation_policy> base;
	struct reader_indicator_sync {
		static void wait_writers(base* t) {
			while(reinterpret_cast<this_t>(t->__data)->writeBarrier.load() > 0) {
				qd::pause();
			}
		}
		static void wait_readers(base* t) {
			while(reinterpret_cast<this_t>(t->__data)->reader_indicator.query()) {
				qd::pause();
			}
		}
	};
	struct hierarchy_sync {
		static void lock(base* t) {
			reinterpret_cast<this_t>(t->__data)->global_lock.lock();
		};
		static void unlock(base* t) {
			reinterpret_cast<this_t>(t->__data)->global_lock.unlock();
		};
	};
	typedef hierarchy_sync hierarchy_t;
	
	GMLock global_lock;
	char pad4[128];
	int numanodes;
	std::vector<int> numa_mapping;
	base* qdlocks;
	thread_local static bool node_id_init;
	thread_local static int node_id;
	base& get_qd() {
		int idx;
		if(pinning_policy == pinning_policy_t::free_threads) {
			int cpu = sched_getcpu();
			idx = numa_mapping[cpu];
		} else {
			if(!node_id_init) {
				node_id = sched_getcpu();
				node_id_init = true;
			}
			idx = numa_mapping[node_id];
		}
		return qdlocks[idx];
	}
	public:

		mrhqdlock_impl() : writeBarrier(0) {
			int amount = numa_num_configured_nodes();
			numanodes = amount;
			qdlocks = new base[amount];
			for(int a = 0; a < amount; a++) {
				qdlocks[a].__data = reinterpret_cast<void*>(this);
			}
			int num_cpus = numa_num_configured_cpus();
			numa_mapping.resize(num_cpus);
			for (int i = 0; i < num_cpus; ++i) {
				numa_mapping[i] = numa_node_of_cpu(i);
			}
		}
		typedef reader_indicator_sync reader_sync_t;

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
			get_qd().template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_t = typename base::no_promise::promise;
			get_qd().template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(nullptr, std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
		template<typename Function, Function f, typename... Ps>
		auto delegate_f(Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			get_qd().template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		
		/* interface _f functions: Provide a future for the return value of the delegated operation */
		template<typename Function, typename... Ps>
		auto delegate_f(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			get_qd().template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, Ps...>(std::move(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
			return future;
		}
		
		/* interface _p functions: User provides a promise, which is used explicitly by the delegated (void) function */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto delegate_p(Promise&& result, Ps&&... ps)
		-> void
		{
			using no_promise = typename base::no_promise::promise;
			get_qd().template delegate<Function, f, no_promise, reader_sync_t, hierarchy_t, Ps...>(nullptr, std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}
		template<typename Function, typename Promise, typename... Ps>
		auto delegate_p(Function&& f, Promise&& result, Ps&&... ps)
		-> void
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using no_promise = typename base::no_promise::promise;
			get_qd().template delegate<std::nullptr_t, nullptr, no_promise, reader_sync_t, hierarchy_t, Function, Promise, Ps...>(nullptr, std::forward<Function>(f), std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}

		/* interface _fp functions: Promise is generated here, but delegated function uses it explicitly. */
		template<typename Return, typename Function, Function f, typename... Ps>
		auto delegate_fp(Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename base::no_promise::promise;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			get_qd().template delegate<Function, f, promise_t, reader_sync_t, hierarchy_t, promise_t, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		template<typename Return, typename Function, typename... Ps>
		auto delegate_fp(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename base::no_promise::promise;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			get_qd().template delegate<std::nullptr_t, nullptr, promise_t, reader_sync_t, hierarchy_t, Function, promise_t, Ps...>(nullptr, std::forward<Function>(f), std::move(result), std::forward<Ps>(ps)...);
			return future;
		}

		void lock() {
			while(writeBarrier.load() > 0) {
				qd::pause();
			}
			get_qd().mutex_lock.lock();
			this->global_lock.lock();
			while(reader_indicator.query()) {
				qd::pause();
			}
		}
		void unlock() {
			this->global_lock.unlock();
			get_qd().mutex_lock.unlock();
		}

		void rlock() {
			bool bRaised = false;
			int readPatience = 0;
start:
			reader_indicator.arrive();
			if(this->global_lock.is_locked()) {
				reader_indicator.depart();
				while(this->global_lock.is_locked()) { // TODO is this sufficient?
					qd::pause();
					if((readPatience == READ_PATIENCE_LIMIT) && !bRaised) {
						writeBarrier.fetch_add(1);
						bRaised = true;
					}
					readPatience += 1;
				}
				goto start;
			}
			if(bRaised) writeBarrier.fetch_sub(1);
		}
		void runlock() {
			reader_indicator.depart();
		}

};

template<class GMLock, class NMLock, class DQueue, class RIndicator, int READ_PATIENCE_LIMIT, pinning_policy_t pinning_policy, starvation_policy_t starvation_policy>
thread_local bool mrhqdlock_impl<GMLock, NMLock, DQueue, RIndicator, READ_PATIENCE_LIMIT, pinning_policy, starvation_policy>::node_id_init = false;
template<class GMLock, class NMLock, class DQueue, class RIndicator, int READ_PATIENCE_LIMIT, pinning_policy_t pinning_policy, starvation_policy_t starvation_policy>
thread_local int mrhqdlock_impl<GMLock, NMLock, DQueue, RIndicator, READ_PATIENCE_LIMIT, pinning_policy, starvation_policy>::node_id;

#endif /* qd_hqdlock_hpp */
