#ifndef qd_buffer_queue_hpp
#define qd_buffer_queue_hpp qd_buffer_queue_hpp

#include<algorithm>
#include<array>
#include<atomic>
#include<map>
#include<stack>
#include "util/type_tools.hpp"

/**
 * @brief a buffer-based tantrum queue
 * @tparam ARRAY_SIZE the buffer's size in bytes
 */
template<long ARRAY_SIZE>
class buffer_queue {
	/** type for sizes of queue entries */
	typedef long s_type;
	/** type for the size field for queue entries, loads must not be optimized away in flush */
	typedef std::atomic<s_type> sizetype;

	/** type for function pointers to be stored in this queue */
//	typedef std::function<void(char*)> ftype;
	typedef void(*ftype)(char*);

	struct entry_t {
		std::atomic<int> fun;
		char buf[28];
	};
	public:
		/** constants for current state of the queue */
		enum class status { OPEN=0, SUCCESS=0, FULL, CLOSED };

		buffer_queue() : counter(ARRAY_SIZE), closed(status::CLOSED), sizes(new std::map<ftype, int>), functions(new std::map<int, ftype>), rev_functions(new std::map<ftype, int>), function_idx(1) {
			std::fill(&buffer_array[0], &buffer_array[ARRAY_SIZE], 0);
		}
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
			closed.store(status::OPEN, std::memory_order_relaxed);
		}

		void forwardall(char*) {};
		template<typename P, typename... Ts>
		void forwardall(char* offset, P&& p, Ts&&... ts) {
			auto ptr = reinterpret_cast<P*>(offset);
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
			constexpr s_type size = aligned(sizeof(std::atomic<int>) + sumsizes<Ps...>::size);
			if(!rev_functions.load(std::memory_order_relaxed)->count(op)) {
				auto new_sizes = new std::map<ftype, int>;
				auto current_sizes = sizes.load(std::memory_order_relaxed);
				auto new_funs = new std::map<int, ftype>;
				auto new_rfuns = new std::map<ftype, int>;
				auto idx = function_idx.fetch_add(1);
				auto current_funs = functions.load(std::memory_order_relaxed);
				auto current_rfuns = rev_functions.load(std::memory_order_relaxed);
				do {
					*new_sizes = *current_sizes;
					(*new_sizes)[op] = size;
				} while(!sizes.compare_exchange_weak(current_sizes, new_sizes, std::memory_order_relaxed));
				do {
					*new_funs = *current_funs;
					(*new_funs)[idx] = op;
				} while(!functions.compare_exchange_weak(current_funs, new_funs, std::memory_order_relaxed));
				do {
					*new_rfuns = *current_rfuns;
					(*new_rfuns)[op] = idx;
				} while(!rev_functions.compare_exchange_weak(current_rfuns, new_rfuns, std::memory_order_relaxed));

				std::lock_guard<std::mutex> l(to_free_lock);
				to_free.push(current_sizes);
				to_free.push(current_rfuns);
				to_free2.push(current_funs);
			}

			auto current_status = closed.load(std::memory_order_relaxed);
			if(current_status != status::OPEN) {
				return current_status;
			}
			/* get memory in buffer */
			s_type index = counter.fetch_add(size, std::memory_order_relaxed);
			auto entryp = reinterpret_cast<entry_t*>(&buffer_array[index]);
			if(index <= ARRAY_SIZE - size) {
				/* enough memory available: move op, p and parameters to buffer, then set size of entry */
				forwardall(&entryp->buf[0], std::move(*ps)...);

				entryp->fun.store((*rev_functions.load(std::memory_order_relaxed))[op], std::memory_order_release);
				return status::SUCCESS;
			} else {
				/* not enough memory available: avoid deadlock in flush by setting special value */
				if(index < static_cast<s_type>(ARRAY_SIZE - sizeof(std::atomic<int>))) {
					entryp->fun.store(-1, std::memory_order_release); // TODO MEMORDER
				}
				closed.store(status::FULL, std::memory_order_relaxed);
				return status::FULL;
			}
		}

		/** execute all stored operations, leave queue in closed state */
		void flush() {
			s_type todo = 0;
			bool open = true;
			while(open) {
				s_type done = todo;
				todo = counter.load(std::memory_order_relaxed);
				if(todo == done) { /* close queue */
					todo = counter.exchange(ARRAY_SIZE, std::memory_order_relaxed);
					open = false;
					closed.store(status::CLOSED, std::memory_order_relaxed);
				}
				if(todo >= static_cast<s_type>(ARRAY_SIZE)-sizeof(std::atomic<int>)) { /* queue closed */
					constexpr auto todomax = ARRAY_SIZE-static_cast<s_type>(sizeof(std::atomic<int>));
					todo = todomax;
					closed.store(status::CLOSED, std::memory_order_relaxed);
					open = false;
				}
				s_type last_size = 0;
				for(s_type index = done; index < todo; index+=last_size) {
					/* synchronization on entry size field: 0 until entry available */
					int fun_idx = 0;
					auto entryp = reinterpret_cast<entry_t*>(&buffer_array[index]);
					do {
						fun_idx = entryp->fun.load(std::memory_order_acquire);
					} while(fun_idx == 0);
					/* buffer full signal: everything done */
					if(fun_idx == -1) {
						std::fill(&buffer_array[index], &buffer_array[index+sizeof(std::atomic<int>)], 0);
						goto end;
					}
					ftype fun = (*functions.load(std::memory_order_relaxed))[fun_idx];
					/* get functor from buffer */ // TODO: move-construct?
					last_size = (*sizes.load(std::memory_order_relaxed))[fun];

					/* call functor with pointer to promise (of unknown type) */
					fun(&entryp->buf[0]);

					/* cleanup: call destructor of (now empty) functor and clear buffer area */
//					fun->~ftype();
					std::fill(&buffer_array[index], &buffer_array[index+last_size], 0);
				}
			}
end:
			std::lock_guard<std::mutex> l(to_free_lock);
			while(!to_free.empty()) {
				auto del = to_free.top();
				to_free.pop();
				delete del;
			}
			while(!to_free2.empty()) {
				auto del = to_free2.top();
				to_free2.pop();
				delete del;
			}
		}
	private:
		/** counter for how much of the buffer is currently in use; offset to free area in buffer_array */
		std::atomic<s_type> counter;
		char pad[128];
		/** optimization flag: no writes when queue in known-closed state */
		std::atomic<status> closed;
		char pad2[128];
		/** the buffer for entries to this queue */
		std::array<char, ARRAY_SIZE> buffer_array;
		char pad4[128];
		std::atomic<std::map<ftype, int>*> sizes;
		char pad45[128];
		std::atomic<std::map<int, ftype>*> functions;
		char pad5[128];
		std::atomic<std::map<ftype, int>*> rev_functions;
		char pad55[128];
		std::mutex to_free_lock;
		char pad6[128];
		std::stack<std::map<ftype, int>*> to_free;
		char pad65[128];
		std::stack<std::map<int, ftype>*> to_free2;
		char pad7[128];
		std::atomic<int> function_idx;

		static constexpr s_type aligned(s_type size) {
			return size;
			//return ((size + sizeof(sizetype) - 1) / sizeof(sizetype)) * sizeof(sizetype); /* TODO: better way of computing a ceil? */
		}
};

#endif /* qd_buffer_queue_hpp */
