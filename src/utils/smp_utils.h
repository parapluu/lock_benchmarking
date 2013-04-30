#ifndef SMP_UTILS_H
#define SMP_UTILS_H

#include <stdbool.h>

//Make sure compiler does not optimize away memory access
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

//Atomic get
#define GET(value_ptr)  __sync_fetch_and_add(value_ptr, 0)

//Compiller barrier
#define barrier() __asm__ __volatile__("": : :"memory")

inline
int get_and_set_int(int * pointerToOldValue, int newValue){
    int x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

typedef union CacheLinePaddedBoolImpl {
    bool value;
    char padding[64];
} CacheLinePaddedBool;

typedef union CacheLinePaddedIntImpl {
    int value;
    char padding[64];
} CacheLinePaddedInt;


#endif
