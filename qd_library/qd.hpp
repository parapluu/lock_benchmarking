#ifndef qd_qd_hpp
#define qd_qd_hpp qd_qd_hpp

#include "locks/waitable_lock.hpp"
#include "locks/tatas_lock.hpp"
#include "locks/mutex_lock.hpp"
#include "locks/futex_lock.hpp"
#include "locks/mcs_futex_lock.hpp"
#include "locks/mcs_lock.hpp"
#include "locks/ticket_futex_lock.hpp"

#include "queues/buffer_queue.hpp"
#include "queues/dual_buffer_queue.hpp"
#include "queues/entry_queue.hpp"
#include "queues/simple_locked_queue.hpp"

#include "qdlock.hpp"
#include "hqdlock.hpp"
#include "mrqdlock.hpp"

#include "qd_condition_variable.hpp"

template<typename Lock>
class extended_lock : public Lock {
	public:
		bool try_lock_or_wait() {
			return this->try_lock();
		}
};

using internal_lock = mcs_futex_lock;
using qdlock = qdlock_impl<internal_lock, buffer_queue<262139>>;
using mrqdlock = mrqdlock_impl<internal_lock, buffer_queue<16384>, reader_groups<64>, 65536>;
using qd_condition_variable = qd_condition_variable_impl<mutex_lock, simple_locked_queue>;

#define DELEGATE_F(function, ...) template delegate_f<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_N(function, ...) template delegate_n<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_P(function, ...) template delegate_p<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_FP(function, ...) template delegate_fp<decltype(function), function>(__VA_ARGS__)
#define WAIT_REDELEGATE_P(function, ...) template wait_redelegate_p<decltype(function), function>(__VA_ARGS__)

#endif /* qd_qd_hpp */
