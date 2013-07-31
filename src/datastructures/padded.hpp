#ifndef padded_hpp
#define padded_hpp

template<typename T, unsigned int PADSIZE, bool T_is_class>
class padded_base;

template<typename T, unsigned int PADSIZE>
class padded_base<T, PADSIZE, false> {
	T value;
	char padding[PADSIZE - (sizeof(T)%PADSIZE)];
	typedef padded_base<T, PADSIZE, false> P_type;
	typedef typename std::remove_pointer<T>::type T_dereferenced;
	friend class padded_base<T*, PADSIZE, false>;
	public:
		padded_base() {} /* this is a basic type, it is NOT initialized to 0 */
		padded_base(const T v) : value(v) {}
		padded_base(const P_type& v) : value(v) {}
		padded_base(padded_base<T_dereferenced, PADSIZE, true>* const v) : value(v) {}
		padded_base(padded_base<T_dereferenced, PADSIZE, false>* const v) : value(&v->value) {}

		operator T&() {
			return value;
		}
		operator const T&() const {
			return value;
		}
		P_type& operator=(P_type other) {
			swap(*this, other);
			return *this;
		}
		P_type& operator=(T other) {
			using std::swap;
			swap(value, other);
			return *this;
		}

		bool operator==(const T other) {
			return value == other;
		}
		bool operator!=(const T other) {
			return !(*this == other);
		}
		T_dereferenced& operator*() {
			return *value;
		}
		const T_dereferenced& operator*() const {
			return *value;
		}
		T_dereferenced* operator->() {
			return value;
		}
		const T_dereferenced* operator->() const {
			return value;
		}

		T& get() {
			return value;
		}

		const T& get() const {
			return value;
		}

		friend void swap(P_type& first, P_type& second) {
			using std::swap;
			swap(first.value, second.value);
		}
};

template<typename T, unsigned int PADSIZE>
class padded_base<T, PADSIZE, true> : public T {
	char padding[PADSIZE - (sizeof(T)%PADSIZE)];
	//typedef padded_base<T, PADSIZE, true> P_type;
	//typedef typename std::remove_pointer<T>::type T_dereferenced;
	//friend class padded_base<T*, PADSIZE, false>;
	public:
		using T::T;
		using T::operator=;
//		T_dereferenced& operator*() {
//			return **this;
//		}
//		const T_dereferenced& operator*() const {
//			return *value;
//		}
//		T_dereferenced* operator->() {
//			return value;
//		}
//		const T_dereferenced* operator->() const {
//			return value;
//		}
//
		T& get() {
			return *this;
		}

		const T& get() const {
			return *this;
		}
};

template<typename T, int PADSIZE = 128>
class padded : public padded_base<T, PADSIZE, std::is_class<T>::value> {
	typedef padded_base<T, PADSIZE, std::is_class<T>::value> base_type;
	public:
		using base_type::base_type;
		using base_type::operator=;
};

#endif // padded_hpp
