#ifndef qd_dual_buffer_queue_hpp
#define qd_dual_buffer_queue_hpp qd_dual_buffer_queue_hpp

#include<algorithm>
#include<array>
#include<atomic>
#include<new>
#include "util/pause.hpp"
#include "util/type_tools.hpp"

/**
 * @brief policies for atomic instructions to use
 */
enum class atomic_instruction_policy_t {use_fetch_and_add, use_compare_and_swap};

void no_op(char*) {}

/**
 * @brief a entry-based tantrum queue with a fixed buffer for parameters
 * @tparam ENTRIES the number of entries
 * @tparam ARRAY_SIZE the buffer's size in bytes
 */
template<unsigned ENTRIES, unsigned int ENTRY_SIZE=16, atomic_instruction_policy_t atomic_instruction_policy=atomic_instruction_policy_t::use_fetch_and_add>
class dual_buffer_queue {
	/** type for sizes of queue entries */
	typedef unsigned s_type;
	/** type for the size field for queue entries, loads must not be optimized away in flush */
	typedef std::atomic<char*> sizetype;

	/** type for function pointers to be stored in this queue */
	typedef void(*ftype)(char*);
	
	public:
		/** constants for current state of the queue */
		enum class status { OPEN=0, SUCCESS=0, FULL, CLOSED }; 

		dual_buffer_queue() : counter(ENTRIES) {}
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
		}

		void forwardall(unsigned) {};
		template<typename P, typename... Ts>
		void forwardall(unsigned offset, P&& p, Ts&&... ts) {
			auto ptr = reinterpret_cast<P*>(&buffer_array[offset]);
			new (ptr) P(std::forward<P>(p));
			forwardall(offset+sizeof(p), std::forward<Ts>(ts)...);
		}
		/**
		 * @brief enqueues an entry
		 * @tparam P return type of associated function
		 * @param op wrapper function for associated function
		 * @return SUCCESS on successful storing in queue, FULL if the queue is full and CLOSED if the queue is closed explicitly
		 */
		template<typename... Ps>
		status enqueue(ftype op, Ps*... ps) {
			/* entry size = size of size + size of wrapper functor + size of promise + size of all parameters*/
			constexpr unsigned size = aligned(sumsizes<Ps...>::size + sizeof(unsigned));
			constexpr unsigned req_entries = (size+ENTRY_SIZE-1)/ENTRY_SIZE;
			unsigned index = counter.load(std::memory_order_relaxed);
			if(index > (ENTRIES - req_entries)) {
				return status::CLOSED;
			}
			/* get memory in buffer */
			if(atomic_instruction_policy == atomic_instruction_policy_t::use_fetch_and_add) {
				index = counter.fetch_add(req_entries, std::memory_order_relaxed);
			} else {
				while(!counter.compare_exchange_weak(index, index+req_entries, std::memory_order_relaxed)) {
					//qd::pause();
				}
			}
			if(index <= ENTRIES - req_entries) {
				/* enough memory available: move op, p and parameters to buffer, then set size of entry */
				*reinterpret_cast<unsigned*>(&buffer_array[index*ENTRY_SIZE]) = req_entries;
				forwardall(index*ENTRY_SIZE + sizeof(unsigned), std::move(*ps)...);
				entries[index].f.store(op, std::memory_order_release);
				return status::SUCCESS;
			} else {
				if(index < ENTRIES) {
					*reinterpret_cast<unsigned*>(&buffer_array[index*ENTRY_SIZE]) = ENTRIES-index;
					entries[index].f.store(&no_op, std::memory_order_release);
				}
				return status::FULL;
			}
		}

		/** execute all stored operations, leave queue in closed state */
		void flush() {
			unsigned todo = 0;
			bool open = true;
			while(open) {
				unsigned done = todo;
				todo = counter.load(std::memory_order_relaxed);
				if(todo == done) { /* close queue */
					todo = counter.exchange(ENTRIES, std::memory_order_relaxed);
					open = false;
				}
				if(todo >= ENTRIES) { /* queue closed */
					todo = ENTRIES;
					open = false;
				}
				for(unsigned index = done; index < todo; ) {
					/* synchronization on entry size field: 0 until entry available */
					ftype fun = entries[index].f.load(std::memory_order_acquire);
					while(!fun) {
						qd::pause();
						fun = entries[index].f.load(std::memory_order_acquire);
					};
					/* call functor with pointer to promise (of unknown type) */
					unsigned req_entries = *reinterpret_cast<unsigned*>(&buffer_array[index*ENTRY_SIZE]);
					fun(&buffer_array[index*ENTRY_SIZE + sizeof(req_entries)]);
					index += req_entries;
#ifdef QUEUE_STATS
					numberOfDeques.value++;
#endif
				}
			}
			;
			if(todo > 0) {
				std::fill(reinterpret_cast<char*>(&entries[0]), reinterpret_cast<char*>(&entries[todo])-1, 0);
			}
		}
	private:
		/** counter for how much of the buffer is currently in use; offset to free area in buffer_array */
		std::atomic<s_type> counter;
		char pad[128];
		/** optimization flag: no writes when queue in known-closed state */
		struct entry_t {
			std::atomic<ftype> f;
		};
		std::array<entry_t, ENTRIES> entries;
		char pad3[128];
		/** the buffer for entries to this queue */
		std::array<char, ENTRIES*ENTRY_SIZE> buffer_array;

		static constexpr s_type aligned(s_type size) {
			return size==0?1:size;
			//return (size<4*ENTRY_SIZE)?4*ENTRY_SIZE:size;
			//return ((size + sizeof(sizetype) - 1) / sizeof(sizetype)) * sizeof(sizetype); /* TODO: better way of computing a ceil? */
		}
};

#endif /* qd_dual_buffer_queue_hpp */
