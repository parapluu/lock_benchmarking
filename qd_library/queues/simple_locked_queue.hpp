#ifndef qd_simple_locked_queue_hpp
#define qd_simple_locked_queue_hpp qd_simple_locked_queue_hpp

#include<array>
#include<cassert>
#include<mutex>
#include<queue>

class simple_locked_queue {
	std::mutex lock;
	std::queue<std::array<char, 128>> queue;
	typedef std::lock_guard<std::mutex> scoped_guard;
	
	typedef void(*ftype)(char*);
	
	/* some constants */
	static const bool CLOSED = false;
	static const bool SUCCESS = true;

	void forwardall(char*, long i) {
		assert(i <= 120);
		if(i > 120) throw "up";
	};
	template<typename P, typename... Ts>
	void forwardall(char* buffer, long offset, P&& p, Ts&&... ts) {
		assert(offset <= 120);
		auto ptr = reinterpret_cast<P*>(&buffer[offset]);
		new (ptr) P(std::forward<P>(p));
		forwardall(buffer, offset+sizeof(p), std::forward<Ts>(ts)...);
	}
	public:
		void open() {
			/* TODO this function should not even be here */
			/* no-op as this is an "infinite" queue that always accepts more data */
		}
		/**
		 * @brief enqueues an entry
		 * @tparam P return type of associated function
		 * @param op wrapper function for associated function
		 * @return SUCCESS on successful storing in queue, CLOSED otherwise
		 */
		template<typename... Ps>
		bool enqueue(ftype op, Ps*... ps) {
			std::array<char, 128> val;
			scoped_guard l(lock);
			queue.push(val);
			forwardall(queue.back().data(), 0, std::move(op), std::move(*ps)...);
			return SUCCESS;
		}

		/** execute all stored operations */
		void flush() {
			scoped_guard l(lock);
			while(!queue.empty()) {
				auto operation = queue.front();
				char* ptr = operation.data();
				ftype* fun = reinterpret_cast<ftype*>(ptr);
				ptr += sizeof(ftype*);
				(*fun)(ptr);
				queue.pop();
			}
		}
		/** execute one stored operation */
		void flush_one() {
			scoped_guard l(lock);
			if(!queue.empty()) {
				char* ptr = queue.front().data();
				ftype* fun = reinterpret_cast<ftype*>(ptr);
				ptr += sizeof(ftype);
				(*fun)(ptr);
				queue.pop();
			}
		}
};

#endif /* qd_simple_locked_queue_hpp */
