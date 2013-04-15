
//Make sure compiler does not optimize away memory access
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

//Atomic get
#define GET(value_ptr)  __sync_fetch_and_add(value_ptr, 0)

//Compiller barrier
#define barrier() __asm__ __volatile__("": : :"memory")


inline
int get_and_set_int(int * pointerToOldValue, int newValue){
    while (true) {
        int x = GET(pointerToOldValue);
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
    }
}
