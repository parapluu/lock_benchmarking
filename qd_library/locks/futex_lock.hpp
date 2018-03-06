#ifndef qd_futex_lock_hpp
#define qd_futex_lock_hpp qd_futex_lock_hpp

#include<atomic>
#include<unistd.h>
#include<linux/futex.h>
#include<sys/syscall.h>

/**
 * @brief a futex based lock
 * @details This lock cooperates with the Linux kernel when there is no chance on taking the lock.
 *          The sleep-waiting can be triggered by lock() and try_lock_or_wait().
 *          When the lock cannot be taken these two functions wait until the Linux kernel wakes them up again,
 *          which is triggered by unlock() operations.
 * @note lock() and unlock() are taken from mutex in http://www.akkadia.org/drepper/futex.pdf with additional documentation used from http://locklessinc.com/articles/mutex_cv_futex/
 */
class futex_lock {
	enum field_t { free, taken, contended };
	std::atomic<field_t> locked;
	public:
		futex_lock() : locked(free) {}
		futex_lock(futex_lock&) = delete; /* TODO? */

		/**
		 * @brief take the lock
		 * @details This function will issue a futex wait syscall
		 *          while waiting for the lock.
		 */
		void lock() {
			field_t c = free;
			if(!locked.compare_exchange_strong(c, taken)) {
				if(c != contended) {
					c = locked.exchange(contended);
				}
				while(c != free) {
					wait();
					c = locked.exchange(contended);
				}
			}
		}

		/**
		 * @brief release the lock
		 * @details This will unlock the lock. If there are sleeping threads,
		 *          they will also be woken up.
		 */
		void unlock() {
			auto old = locked.exchange(free, std::memory_order_release);
			if(old == contended) {
				notify_all(); // notify_one instead?
			}
		}

		/**
		 * @brief non-blocking trylock.
		 * @return true iff the lock has been taken
		 */
		bool try_lock() {
			if(this->is_locked()) {
				return false;
			}
			field_t c = free;
			return locked.compare_exchange_strong(c, taken, std::memory_order_acq_rel, std::memory_order_relaxed);
		}

		/**
		 * @brief blocking trylock.
		 * @return true iff the lock has been taken,
		 *         false after waiting for an unlock() operation on the lock.
		 */
		bool try_lock_or_wait() {
			field_t c = locked.load(std::memory_order_acquire);
			if(c == free) {
				if(locked.compare_exchange_strong(c, taken, std::memory_order_acq_rel, std::memory_order_relaxed)) {
					return true;
				}
			}
			/* not free (or CAS failed because no longer free) */
			if(c != contended) {
				c = locked.exchange(contended);
				if(c == free) {
					return true;
				}
			}
			/* contended (or was taken and swapped to contended) */
			wait();
			return false;
		}

		/**
		 * @brief check lock state
		 * @return true iff the lock is taken, false otherwise
		 */
		bool is_locked() {
			return locked.load(std::memory_order_acquire) != free;
		}

	private:
		/** @brief sleep until unlock() is called */
		void wait() {
			sys_futex(&locked, FUTEX_WAIT, contended, NULL, NULL, 0);
		}

		/** @brief wake one thread from sleeping that waits on this lock */
		void notify_one() {
			sys_futex(&locked, FUTEX_WAKE, 1, NULL, NULL, 0);
		}

		/** @brief wake all sleeping threads that wait on this lock */
		void notify_all() {
			static const int WAKE_AT_ONCE = 32768;
			int woken;
			do {
				woken = sys_futex(&locked, FUTEX_WAKE, WAKE_AT_ONCE, NULL, NULL, 0);
			} while (woken == WAKE_AT_ONCE);
		}

		/**
		 * @brief wrapper for futex syscalls
		 * @param addr1 the address threads are observing
		 * @param op either FUTEX_WAIT or FUTEX_WAKE
		 * @param val1 if op==FUTEX_WAIT then the expected value for *addr1,
		 *             if op==FUTEX_WAKE then the number of threads to wake up
		 * @param timeout always NULL
		 * @param addr2 always NULL
		 * @param val3 always 0
		 * @return the return value of the futex syscall
		 */
		static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
		{
			return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
		}

		static_assert(sizeof(locked) == sizeof(field_t), "std::atomic<field_t> size differs from size of field_t. Your architecture does not support the futex_lock");
};

#endif /* qd_futex_lock_hpp */
