#ifndef qd_mcs_lock_hpp
#define qd_mcs_lock_hpp qd_mcs_lock_hpp

#include<atomic>
#include<map>
#include<unistd.h>
#include<stack>
#include<sys/param.h>
#include<thread>
#include "util/pause.hpp"

struct mcs_node2 {
	enum field_t { free, taken, contended };
	std::atomic<field_t> is_locked;
	std::atomic<mcs_node2*> next;
	mcs_node2() : next(nullptr) {}
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
class mcs_lock {
	using mcs_node = ::mcs_node2;
	using field_t = mcs_node*;
	std::atomic<field_t> locked;
	thread_local static std::map<mcs_lock*, mcs_node> mcs_node_store;
	public:
		mcs_lock() : locked(nullptr) {}
		mcs_lock(mcs_lock&) = delete; /* TODO? */

		/**
		 * @brief take the lock
		 * @details This function will issue a futex wait syscall
		 *          while waiting for the lock.
		 */
		void lock() {
			mcs_node* mynode = &mcs_node_store[this];
			mcs_node* found = this->locked.exchange(mynode, std::memory_order_acq_rel);
			if(found != nullptr) {
				mynode->is_locked.store(mcs_node::taken, std::memory_order_release);
				found->next = mynode;
				for(int i = 0; i < 512; i++) {
					if(mynode->is_locked.load(std::memory_order_acquire) == mcs_node::free) {
						break;
					}
					qd::pause();
				}
				while(mynode->is_locked.load(std::memory_order_acquire) != mcs_node::free) {
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
			field_t c = mynode;
			if(mynode->next == nullptr) {
				if(this->locked.compare_exchange_strong(c, nullptr, std::memory_order_acq_rel)) {
					return;
				} else if(c == reinterpret_cast<field_t>(mynode)) {
					unlock();
					return;
				}
			}
			while(mynode->next == nullptr) {
				qd::pause();
				/* wait for nextpointer */
			}
			mcs_node* next = mynode->next.load(); /* TODO */
			next->is_locked.store(mcs_node::free, std::memory_order_release);

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
			field_t c = nullptr;
			mcs_node* mynode = &mcs_node_store[this];
			bool success = this->locked.compare_exchange_strong(c, reinterpret_cast<field_t>(mynode), std::memory_order_acq_rel);
			return success;
		}

		/**
		 * @brief blocking trylock.
		 * @return true iff the lock has been taken,
		 *         false after waiting for an unlock() operation on the lock.
		 */
		bool try_lock_or_wait() {
			return try_lock();
		}

		/**
		 * @brief check lock state
		 * @return true iff the lock is taken, false otherwise
		 */
		bool is_locked() {
			return locked.load(std::memory_order_acquire) != nullptr;
		}
		void wake() {}
	private:
		static_assert(sizeof(locked) == sizeof(field_t), "std::atomic<field_t> size differs from size of field_t. Your architecture does not support the MCS futex_lock");
		static_assert(__BYTE_ORDER == __LITTLE_ENDIAN, "Your architecture's endianess is currently not supported. Please report this as a bug.");
};

thread_local std::map<mcs_lock*, mcs_node2> mcs_lock::mcs_node_store;

#endif /* qd_mcs_lock_hpp */
