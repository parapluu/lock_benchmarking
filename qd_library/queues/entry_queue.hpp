#ifndef qd_entry_queue_hpp
#define qd_entry_queue_hpp qd_entry_queue_hpp

#include<algorithm>
#include<array>
#include<atomic>
#include<iostream>
#include<typeinfo>

/**
 * @brief a buffer-based tantrum queue with fixed-size entries
 * @tparam ENTRIES the number of entries
 */
template<long ENTRIES, int BUFFER_SIZE>
class entry_queue {
	/** type for the size field for queue entries, loads must not be optimized away in flush */
	typedef std::atomic<long> sizetype;

	/** type for function pointers to be stored in this queue */
	typedef void(*ftype)(char*);
	
	struct entry_t {
		std::atomic<ftype> fun;
		char buf[BUFFER_SIZE];
	};
	void forwardall(long, long) {};
	template<typename P, typename... Ts>
	void forwardall(long idx, long offset, P&& p, Ts&&... ts) {
		auto ptr = reinterpret_cast<P*>(&entry_array[idx].buf[offset]);
		new (ptr) P(std::forward<P>(p));
		forwardall(idx, offset+sizeof(p), std::forward<Ts>(ts)...);
	}
	public:
		/** constants for current state of the queue */
		enum class status : long { OPEN=0, SUCCESS=0, FULL, CLOSED }; 

		entry_queue() : counter(ENTRIES), closed(status::CLOSED) {}
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
			closed.store(status::OPEN, std::memory_order_relaxed);
		}

		/**
		 * @brief enqueues an entry
		 * @tparam P return type of associated function
		 * @param op wrapper function for associated function
		 * @return SUCCESS on successful storing in queue, FULL if the queue is full and CLOSED if the queue is closed explicitly
		 */
		template<typename... Ps>
		status enqueue(void (*op)(char*), Ps*... ps) {
			auto current_status = closed.load(std::memory_order_relaxed);
			if(current_status != status::OPEN) {
				return current_status;
			}
			/* entry size = size of size + size of wrapper functor + size of promise + size of all parameters*/
			constexpr long size = sumsizes<Ps...>::size;
			/* get memory in buffer */
			long index = counter.fetch_add(1, std::memory_order_relaxed);
			if(index < ENTRIES) {
				static_assert(size <= BUFFER_SIZE, "entry_queue buffer per entry too small.");
				/* entry available: move op, p and parameters to buffer, then set size of entry */
				forwardall(index, 0, std::move(*ps)...);
				entry_array[index].fun.store(op, std::memory_order_release);
				return status::SUCCESS;
			} else {
				return status::FULL;
			}
		}

		/** execute all stored operations, leave queue in closed state */
		void flush() {
			long todo = 0;
			bool open = true;
			while(open) {
				long done = todo;
				todo = counter.load(std::memory_order_relaxed);
				if(todo == done) { /* close queue */
					todo = counter.exchange(ENTRIES, std::memory_order_relaxed);
					closed.store(status::CLOSED, std::memory_order_relaxed);
					open = false;
				}
				if(todo >= static_cast<long>(ENTRIES)) { /* queue closed */
					todo = ENTRIES;
					closed.store(status::CLOSED, std::memory_order_relaxed);
					open = false;
				}
				for(long index = done; index < todo; index++) {
					/* synchronization on entry size field: 0 until entry available */
					ftype fun = nullptr;
					do {
						fun = entry_array[index].fun.load(std::memory_order_acquire);
					} while(!fun);

					/* call functor with pointer to promise (of unknown type) */
					fun(&entry_array[index].buf[0]);

					/* cleanup: call destructor of (now empty) functor and clear buffer area */
//					fun->~ftype();
					entry_array[index].fun.store(nullptr, std::memory_order_relaxed);
				}
			}
		}
	private:
		/** counter for how many entries are already in use */
		std::atomic<long> counter;
		char pad[128];
		/** optimization flag: no writes when queue in known-closed state */
		std::atomic<status> closed;
		char pad2[128];
		/** the buffer for entries to this queue */
		std::array<entry_t, ENTRIES> entry_array;
};

#endif /* qd_buffer_queue_hpp */
