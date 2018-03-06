#ifndef qd_type_tools_hpp
#define qd_type_tools_hpp qd_type_tools_hpp

template<typename... Ps>
struct sumsizes;
template<typename T, typename... Ps>
struct sumsizes<T, Ps...> {
	static constexpr long size = sizeof(T) + sumsizes<Ps...>::size;
};
template<>
struct sumsizes<> {
	static constexpr long size = 0;
};

/* structure to create lists of types */
template<typename... Ts>
class types;
template<typename T, typename... Ts>
class types<T, Ts...> {
	public:
		typedef T type;
		typedef types<Ts...> tail;
};
template<>
class types<> {};

#endif /* qd_type_tools_hpp */
