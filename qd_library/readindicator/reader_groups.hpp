#ifndef qd_reader_groups_hpp
#define qd_reader_groups_hpp qd_reader_groups_hpp

#include "threadid.hpp"

template<int GROUPS>
class reader_groups {
	struct alignas(64) counter_t {
		char pad1[64];
		std::atomic<int> cnt;
		char pad2[64];
		counter_t() : cnt(0) {}
	};
	std::array<counter_t, GROUPS> counters;
	public:
		reader_groups() {
			for(int i = 0; i < GROUPS; i++) {
				counters[i].cnt.store(0, std::memory_order_release);
			}
		}
		bool query() {
			for(counter_t& counter : counters)
				if(counter.cnt.load(std::memory_order_acquire) > 0) return true;
			return false;
		}
		void arrive() {
			counters[thread_id % GROUPS].cnt.fetch_add(1, std::memory_order_release);
		}
		void depart() {
			counters[thread_id % GROUPS].cnt.fetch_sub(1, std::memory_order_release);
		}
};

#endif /* qd_reader_groups_hpp */
