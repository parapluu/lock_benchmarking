#ifndef threadid_hpp
#define threadid_hpp threadid_hpp

#include <mutex>
#include <set>

class thread_id_store {
	static unsigned long max_id;
	static std::set<unsigned long> orphans;
	static std::mutex mutex;
	typedef std::lock_guard<std::mutex> scoped_lock;
	public:
		static unsigned long get() {
			scoped_lock lock(mutex);
			if(orphans.empty()) {
				max_id++;
				return max_id;
			} else {
				auto first = orphans.begin();
				auto result = *first;
				orphans.erase(first);
				return result;
			}
		}
		static void free(unsigned long idx) {
			scoped_lock lock(mutex);
			if(idx == max_id) {
				max_id--;
				while(orphans.erase(max_id)) {
					max_id--;
				}
			} else {
				orphans.insert(idx);
			}
		}
};

class thread_id_t {
	unsigned long id;
	public:
		operator unsigned long() {
			return id;
		}
		thread_id_t() : id(thread_id_store::get()) {}
		~thread_id_t() {
			thread_id_store::free(id);
		}
};

extern thread_local thread_id_t thread_id;

#endif // threadid_hpp
