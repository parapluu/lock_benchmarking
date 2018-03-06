#ifndef qd_waitable_lock_hpp
#define qd_waitable_lock_hpp qd_waitable_lock_hpp

#include<condition_variable>

/**
 * @brief lock class wrapper to add wait/notify functionality
 * @tparam Lock a locking class
 * @details This wrapper adds functionality to wait on each instance of a class
 *          without requireing a lock to be taken/released. This is useful when
 *          implementing another lock, so that spinning can be avoided.
 * @warning  This implementation relies on std::condition_variable_any not actually needing a lock,
 *       which violates its preconditions.
 * @remarks This is likely not the most efficient way of implementing waiting.
 * @todo The private inheritance is used to get a memory layout in which
 *       clang++-3.4 spills less than if these structures appeared in the opposite
 *       order. This "optimization" might not be the best solution.
 */
template<typename Lock>
class waitable_lock : private std::condition_variable_any, public Lock {
	/**
	 * @brief a dummy lock class
	 * @warning This lock does not provide locking.
	 * @details This class is not intended for use as a lock, but as
	 *          std::condition_variable_any requires a lock class,
	 *          this provides it.
	 */
	struct null_lock {
		void lock() {}
		void unlock() {}
	};

	/** @brief an associated dummy lock for the std::condition_variable_any */
	null_lock not_a_lock;

	public:

		/** @brief wait until notified */
		void wait() {
			std::condition_variable_any::wait(not_a_lock);
		}

		/** @brief notify (at least) one waiting thread */
		void notify_one() {
			std::condition_variable_any::notify_one();
		}

		/** @brief notify all waiting threads */
		void notify_all() {
			std::condition_variable_any::notify_all();
		}
};

#endif /* qd_waitable_lock_hpp */
