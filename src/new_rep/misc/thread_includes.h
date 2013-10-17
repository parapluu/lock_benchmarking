#ifndef THREAD_INCLUDES_H
#define THREAD_INCLUDES_H

#include <time.h>
#include <stdlib.h>
#include <pthread.h>//Until c11 threads.h is available
#include <time.h>
#include <sched.h>

static inline void thread_yield(){
    sched_yield();
}

#endif
