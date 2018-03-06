#ifndef qd_tatas_lock_hpp
#define qd_tatas_lock_hpp qd_tatas_lock_hpp

#include<atomic>

#include "util/pause.hpp"

/** @brief a test-and-test-and-set lock */
class tatas_lock {
	std::atomic<bool> locked; /* TODO can std::atomic_flag be used? */
	public:
		tatas_lock() : locked(false) {};
		tatas_lock(tatas_lock&) = delete; /* TODO? */
		bool try_lock() {
			if(is_locked()) return false;
			return !locked.exchange(true, std::memory_order_acq_rel);
		}
		void unlock() {
			locked.store(false, std::memory_order_release);
		}
		bool is_locked() {
			return locked.load(std::memory_order_acquire);
		}
		void lock() {
			while(!try_lock()) {
				qd::pause();
			}
		}
		void wake() {}
};

#endif /* qd_tatas_lock_hpp */
