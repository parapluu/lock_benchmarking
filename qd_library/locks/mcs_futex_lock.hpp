#ifndef qd_mcs_futex_lock_hpp
#define qd_mcs_futex_lock_hpp qd_futex_lock_hpp

#include<atomic>
#include<unistd.h>
#include<linux/futex.h>
#include<map>
#include<stack>
#include<sys/syscall.h>
#include<sys/param.h>
#include<thread>
#include "util/pause.hpp"

struct mcs_node {
	enum field_t { free, taken, contended };
	std::atomic<field_t> is_locked;
	std::atomic<mcs_node*> next;
	mcs_node() : next(nullptr) {}
};

/**
 * @brief a MCS based lock with futex functionality
 * @details This lock cooperates with the Linux kernel when there is no chance on taking the lock.
 *          The sleep-waiting can be triggered by lock() and try_lock_or_wait().
 *          When the lock cannot be taken these two functions wait until the Linux kernel wakes them up again,
 *          which is triggered by unlock() operations.
 * @note lock() and unlock() function largely like an MCS lock,
 *       while try_lock() and try_lock_or_wait() are more similar to the futex lock.
 */
class mcs_futex_lock {
	using mcs_node = ::mcs_node;
	using field_t = size_t;
	std::atomic<field_t> locked;
	enum field_status { free=0, flag=0x1 };
	thread_local static std::map<mcs_futex_lock*, mcs_node> mcs_node_store;
	public:
		mcs_futex_lock() : locked(free) {}
		mcs_futex_lock(mcs_futex_lock&) = delete; /* TODO? */

		/**
		 * @brief take the lock
		 * @details This function will issue a futex wait syscall
		 *          while waiting for the lock.
		 */
		void lock() {
			mcs_node* mynode = &mcs_node_store[this];
			field_t current = this->locked.load(std::memory_order_relaxed);
			bool success;
			field_t f;
			do {
				f = current & flag;
				success = this->locked.compare_exchange_strong(current, reinterpret_cast<field_t>(mynode)|f, std::memory_order_acq_rel);
			} while(!success);
			mcs_node* found = reinterpret_cast<mcs_node*>(current ^ f);
			if(found != nullptr) {
				mynode->is_locked.store(mcs_node::taken, std::memory_order_release);
				found->next = mynode;
				for(int i = 0; i < 512; i++) {
					if(mynode->is_locked.load(std::memory_order_acquire) == mcs_node::free) {
						break;
					}
					qd::pause();
				}
				if(mynode->is_locked.load(std::memory_order_acquire) == mcs_node::taken) {
					mcs_node::field_t l = mcs_node::taken;
					mynode->is_locked.compare_exchange_strong(l, mcs_node::contended, std::memory_order_acq_rel);
					if(l == mcs_node::free) {
						return;
					}
				}
				while(mynode->is_locked.load(std::memory_order_acquire) != mcs_node::free) {
					wait(reinterpret_cast<int*>(&mynode->is_locked), static_cast<int>(mcs_node::contended));
				}
			}
		}

		/**
		 * @brief release the lock
		 * @details This will unlock the lock. If there are sleeping threads,
		 *          they will also be woken up.
		 */
		void unlock() {
			mcs_node* mynode = &mcs_node_store[this];
			/* thread_local mynode is set while locked */
			field_t c = reinterpret_cast<field_t>(mynode);
			if(mynode->next == nullptr) {
				bool sleep = (locked & flag) == flag;
				if(sleep) {
					c |= flag;
				}
				if(this->locked.compare_exchange_strong(c, free, std::memory_order_acq_rel)) {
					if(sleep) {
						notify_all(reinterpret_cast<int*>(&locked)); /* TODO one instead? */
					}
					return;
				} else if((c & ~flag) == reinterpret_cast<field_t>(mynode)) {
					unlock();
					return;
				}
			}
			while(mynode->next == nullptr) {
				/* wait for nextpointer */
				qd::pause();
			}
			mcs_node* next = mynode->next.load(); /* TODO */

			int status = next->is_locked.exchange(mcs_node::free, std::memory_order_acq_rel);
			if(status == mcs_node::contended) {
				notify_one(reinterpret_cast<int*>(&next->is_locked)); /* there can only be one waiting */
			}
			mynode->next.store(nullptr, std::memory_order_relaxed);
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
			mcs_node* mynode = &mcs_node_store[this];
			return this->locked.compare_exchange_strong(c, reinterpret_cast<field_t>(mynode), std::memory_order_acq_rel);
		}

		/**
		 * @brief blocking trylock.
		 * @return true iff the lock has been taken,
		 *         false after waiting for an unlock() operation on the lock.
		 */
		bool try_lock_or_wait() {
			field_t c = locked.load(std::memory_order_acquire); /*TODO acq or relaxed*/
			mcs_node* mynode = &mcs_node_store[this];
			field_t flagged = c | flag;
			while((c & flag) != flag) {
				if(c == free) {
					if(this->locked.compare_exchange_strong(c, reinterpret_cast<field_t>(mynode), std::memory_order_acq_rel)) {
						return true;
					}
					continue;
				}
				flagged = c | flag;
				if(this->locked.compare_exchange_weak(c,flagged, std::memory_order_acq_rel)) {
					break;
				}
			}
			/* get futexptr to least-significant 32 bit of address, assumes little-endian */
			int* futexptr = reinterpret_cast<int*>(&locked);
			wait(futexptr, reinterpret_cast<size_t>(flagged));
			return false;
		}

		/**
		 * @brief check lock state
		 * @return true iff the lock is taken, false otherwise
		 */
		bool is_locked() {
			return locked.load(std::memory_order_acquire) != free;
		}

		/**
		 * @brief wake threads that are waiting for a lock handover
		 */
		void wake() {
			mcs_node* mynode = &mcs_node_store[this];
			field_t c = reinterpret_cast<field_t>(mynode);
			field_t flagged = c | flag;
			if(locked == flagged) {
				notify_all(reinterpret_cast<int*>(&locked));
			}
		}

	private:
		/**
		 * @brief sleep until unlock() is called
		 * @param addr the memory address to wait for a change on
		 * @param expected currently expected value at addr
		 */
		void wait(int* addr, int expected) {
			sys_futex(addr, FUTEX_WAIT, expected, NULL, NULL, 0);
		}

		/**
		 * @brief wake one thread from sleeping that waits on this lock
		 * @param addr the memory address to wait for a change on
		 */
		void notify_one(int* addr) {
			sys_futex(addr, FUTEX_WAKE, 1, NULL, NULL, 0);
		}

		/**
		 * @brief wake all sleeping threads that wait on this lock
		 * @param addr the memory address to wait for a change on
		 */
		void notify_all(int* addr) {
			static const int WAKE_AT_ONCE = 32768;
			int woken;
			do {
				woken = sys_futex(addr, FUTEX_WAKE, WAKE_AT_ONCE, NULL, NULL, 0);
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


		static_assert(sizeof(locked) == sizeof(field_t), "std::atomic<field_t> size differs from size of field_t. Your architecture does not support the MCS futex_lock");
		static_assert(__BYTE_ORDER == __LITTLE_ENDIAN, "Your architecture's endianess is currently not supported. Please report this as a bug.");
};

thread_local std::map<mcs_futex_lock*, mcs_node> mcs_futex_lock::mcs_node_store;

#endif /* qd_mcs_futex_lock_hpp */
