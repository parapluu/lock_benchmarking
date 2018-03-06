#ifndef qd_mutex_lock_hpp
#define qd_mutex_lock_hpp qd_mutex_lock_hpp

#include<atomic>
#include<mutex>

/** @brief a std::mutex based lock */
class mutex_lock {
	std::atomic<bool> locked;
	std::mutex mutex;
	public:
		mutex_lock() : locked(false), mutex() {};
		mutex_lock(mutex_lock&) = delete; /* TODO? */
		bool try_lock() {
			if(!is_locked() && mutex.try_lock()) {
				locked.store(true, std::memory_order_release);
				return true;
			} else {
				return false;
			}
		}
		void unlock() {
			locked.store(false, std::memory_order_release);
			mutex.unlock();
		}
		bool is_locked() {
			/* This may sometimes return false when the lock is already acquired.
			 * This is safe, because the locking call that acquired the lock in
			 * that case has not yet returned (it needs to set the locked flag first),
			 * so this is concurrent with calling is_locked first and then locking the lock.
			 * 
			 * This may also sometimes return false when the lock is still locked, but
			 * about to be unlocked. This is safe, because of a similar argument as above.
			 */
			return locked.load(std::memory_order_acquire);
		}
		void lock() {
			mutex.lock();
			locked.store(true, std::memory_order_release);
		}
};

#endif /* qd_mutex_lock_hpp */
