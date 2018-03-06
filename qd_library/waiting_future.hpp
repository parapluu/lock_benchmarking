#ifndef waiting_future_hpp
#define waiting_future_hpp waiting_future_hpp

#include<future>

template<typename T>
class waiting_future : public std::future<T> {
	public:
		waiting_future() {}
		waiting_future(waiting_future& rhs) : std::future<T>(rhs) {}
		waiting_future(waiting_future&& rhs) : std::future<T>(std::move(rhs)) {}
		waiting_future(std::future<T>& rhs) : std::future<T>(rhs) {}
		waiting_future(std::future<T>&& rhs) : std::future<T>(std::move(rhs)) {}
		~waiting_future() {
			if(this->valid()) {
				this->wait();
			}
		}
		waiting_future& operator=(waiting_future& rhs) {
			std::future<T>::operator=(rhs);
			return *this;
		}
		waiting_future& operator=(waiting_future&& rhs) {
			std::future<T>::operator=(std::move(rhs));
			return *this;
		}
		void discard() {
			std::future<T> tmp;
			std::swap(tmp, *this);
		}
};

#endif // waiting_future_hpp
