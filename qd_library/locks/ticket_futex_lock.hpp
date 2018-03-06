#ifndef qd_ticket_futex_lock_hpp
#define qd_ticket_futex_lock_hpp qd_futex_lock_hpp

#include<atomic>
#include<unistd.h>
#include<linux/futex.h>
#include<sys/syscall.h>
#include<thread>
#include "util/pause.hpp"

/**
 * @brief a ticket based lock with futex functionality
 * @details This lock cooperates with the Linux kernel when there is no chance on taking the lock.
 *          The sleep-waiting can be triggered by lock() and try_lock_or_wait().
 *          When the lock cannot be taken these two functions wait until the Linux kernel wakes them up again,
 *          which is triggered by unlock() operations.
 * @note lock() and unlock() function largely like a ticket lock,
 *       while try_lock() and try_lock_or_wait() are more similar to the futex lock.
 */
class ticket_futex_lock {
	using field_t = int;
	std::atomic<field_t> ticket;
	char pad[128-sizeof(ticket)];
	std::atomic<field_t> serving;
	public:
		ticket_futex_lock() : ticket(0), serving(0) {}
		ticket_futex_lock(ticket_futex_lock&) = delete; /* TODO? */

		/**
		 * @brief take the lock
		 * @details This function will issue a futex wait syscall
		 *          while waiting for the lock.
		 */
		void lock() {
			field_t mynum = ticket.fetch_add(2, std::memory_order_relaxed);
			field_t current;
			current = serving.load(std::memory_order_acquire);
			while((current&~1) != mynum ) {
				if((current & 1) != 1) {
					if(!serving.compare_exchange_strong(current, current|1, std::memory_order_acq_rel)) {
						continue;
					} else {
						current |= 1;
					}
				}
				wait(current);
				current = serving.load(std::memory_order_acquire);
			}
		}

		/**
		 * @brief release the lock
		 * @details This will unlock the lock. If there are sleeping threads,
		 *          they will also be woken up.
		 */
		void unlock() {
			field_t mine = serving.load(std::memory_order_relaxed);
			field_t current = serving.exchange((mine&~1)+2, std::memory_order_release);
			if((current & 1) == 1) {
				notify_all();
			}
		}

		/**
		 * @brief non-blocking trylock.
		 * @return true iff the lock has been taken
		 */
		bool try_lock() {
			field_t current_ticket = ticket.load(std::memory_order_relaxed);
			field_t current_serving = serving.load(std::memory_order_acquire);
			if(current_ticket != current_serving) {
					return false;
			}
			return ticket.compare_exchange_strong(current_ticket, current_ticket+2);
		}

		/**
		 * @brief blocking trylock.
		 * @return true iff the lock has been taken,
		 *         false after waiting for an unlock() operation on the lock.
		 */
		bool try_lock_or_wait() {
			field_t current_ticket = ticket.load(std::memory_order_relaxed);
			field_t current_serving = serving.load(std::memory_order_acquire);
			field_t flag = current_serving & 1;
			if((current_ticket|flag) == current_serving) {
				if(ticket.compare_exchange_strong(current_ticket, current_ticket+2)) {
					return true;
				}
			}
			if(flag != 1) {
				field_t flagged = current_serving|1;
				if(serving.compare_exchange_strong(current_serving, flagged)) {
						current_serving = flagged;
				} else {
						/* someone called unlock, no need to wait */
						return false;
				}
			}
			wait(current_serving);
			return false;
		}

		/**
		 * @brief check lock state
		 * @return true iff the lock is taken, false otherwise
		 */
		bool is_locked() {
			return ticket.load(std::memory_order_relaxed) != serving.load(std::memory_order_acquire);
		}

	private:
		/**
		 * @brief sleep until unlock() is called
		 * @param addr the memory address to wait for a change on
		 * @param expected currently expected value at addr
		 */
		void wait(int expected) {
			sys_futex(&serving, FUTEX_WAIT, expected, NULL, NULL, 0);
		}

		/**
		 * @brief wake one thread from sleeping that waits on this lock
		 * @param addr the memory address to wait for a change on
		 */
		void notify_one() {
			sys_futex(&serving, FUTEX_WAKE, 1, NULL, NULL, 0);
		}

		/**
		 * @brief wake all sleeping threads that wait on this lock
		 * @param addr the memory address to wait for a change on
		 */
		void notify_all() {
			static const int WAKE_AT_ONCE = 32768;
			int woken;
			do {
				woken = sys_futex(&serving, FUTEX_WAKE, WAKE_AT_ONCE, NULL, NULL, 0);
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


		static_assert(sizeof(serving) == sizeof(field_t), "std::atomic<field_t> size differs from size of field_t. Your architecture does not support the ticket_futex_lock");
};

#endif /* qd_ticket_futex_lock_hpp */
