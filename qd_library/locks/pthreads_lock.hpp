#ifndef qd_pthreads_lock_hpp
#define qd_pthreads_lock_hpp qd_pthreads_lock_hpp

#include<atomic>
#include<pthread.h>

/** @brief a pthreads based lock */
class pthreads_lock {
	std::atomic<bool> locked;
	pthread_mutex_t mutex;
	public:
		pthreads_lock() : locked(false), mutex(PTHREAD_MUTEX_INITIALIZER) {};
		pthreads_lock(pthreads_lock&) = delete; /* TODO? */
		bool try_lock() {
			if(!is_locked() && !pthread_mutex_trylock(&mutex)) {
				locked.store(true, std::memory_order_release);
				return true;
			} else {
				return false;
			}
		}
		void unlock() {
			locked.store(false, std::memory_order_release);
			pthread_mutex_unlock(&mutex);
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
			pthread_mutex_lock(&mutex);
			locked.store(true, std::memory_order_release);
		}
};

#endif /* qd_pthreads_lock_hpp */
