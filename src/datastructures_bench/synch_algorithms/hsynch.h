#ifndef _HSYNCH_H_
#define _HSYNCH_H_

#if defined(sun) || defined(_sun)
#    include <schedctl.h>
#endif

#include "synch_algs_config.h"
#include "synch_algs_primitives.h"
#include "clh.h"

#define DEQUEUE_ARG INT_MIN

#define HSYNCH_NCLUSTERS                NUMBER_OF_NUMA_NODES
const int HSYNCH_HELP_BOUND = MAX_NUM_OF_HELPED_OPS;
//3 * N_THREADS;

typedef struct HalfHSynchLockNode {
    struct LockNode *next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
} HalfHSynchLockNode;

typedef struct HSynchLockNode {
    struct HSynchLockNode *next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
    int32_t align[PAD_CACHE(sizeof(HalfHSynchLockNode))];
} HSynchLockNode;

typedef union HSynchLockNodePtr {
    struct HSynchLockNode *ptr;
    char pad[CACHE_LINE_SIZE];
} HSynchLockNodePtr;

typedef struct HSynchThreadState {
    HSynchLockNode *next_node;
#if defined(__sun) || defined(sun)
    schedctl_t *schedule_control;    
#endif
} HSynchThreadState;

typedef struct HSynchStruct {
    CLHLockStruct *central_lock CACHE_ALIGN;
    volatile HSynchLockNodePtr Tail[HSYNCH_NCLUSTERS] CACHE_ALIGN;
#ifdef DEBUG
    volatile int rounds CACHE_ALIGN;
    volatile int counter;
#endif
} HSynchStruct;


inline static RetVal applyOp(HSynchStruct *l, HSynchThreadState *st_thread, ArgVal arg, int my_numa_node) {
    volatile HSynchLockNode *p;
    volatile HSynchLockNode *cur;
    register HSynchLockNode *next_node, *tmp_next;
    register int counter = 0;

    next_node = st_thread->next_node;
    next_node->next = null;
    next_node->locked = true;
    next_node->completed = false;

#if defined(__sun) || defined(sun)
    schedctl_start(st_thread->schedule_control);
#endif
    cur = (volatile HSynchLockNode *)SWAP(&l->Tail[my_numa_node].ptr, next_node);
    cur->arg_ret = arg;
    cur->next = (HSynchLockNode *)next_node;
#if defined(__sun) || defined(sun)
    schedctl_stop(st_thread->schedule_control);
#endif
    st_thread->next_node = (HSynchLockNode *)cur;

    while (cur->locked) {                   // spinning
#if N_THREADS > USE_CPUS
        sched_yield();
#elif defined(__sun) || defined(sun)
        FullFence();
        FullFence();
        FullFence();
        FullFence();
        FullFence();
        FullFence();
#else
        __sync_synchronize();
#endif
    }
#if defined(__sun) || defined(sun)
    schedctl_start(st_thread->schedule_control);
#endif
    p = cur;                                // I am not been helped
    if (cur->completed)                     // I have been helped
        return cur->arg_ret;
    clhLock(l->central_lock, 0 /*NOT USED*/);
#ifdef DEBUG
    l->rounds++;
#endif
    while (p->next != null && counter < HSYNCH_HELP_BOUND) {
        ReadPrefetch(p->next);
        counter++;
#ifdef DEBUG
        l->counter++;
#endif
        tmp_next = p->next;
        if(p->arg_ret==DEQUEUE_ARG){
            p->arg_ret = dequeue_cs();
        }else{
            enqueue_cs(p->arg_ret);
        }
        p->completed = true;
        p->locked = false;
        p = tmp_next;
    }
    p->locked = false;                      // Unlock the next one
    clhUnlock(l->central_lock, 0 /*NOT USED*/);
#if defined(__sun) || defined(sun)
    schedctl_stop(st_thread->schedule_control);
#endif

#ifdef QUEUE_STATS
    helpSeasonsPerformed.value++;
    numberOfDeques.value = numberOfDeques.value + counter;
#endif

    return cur->arg_ret;
}

inline static void HSynchThreadStateInit(HSynchThreadState *st_thread) {
    clhThreadLocalInit();
    st_thread->next_node = getAlignedMemory(CACHE_LINE_SIZE, sizeof(HSynchLockNode));
#ifdef sun
    st_thread->schedule_control = schedctl_init();
#endif
}

void HSynchStructInit(HSynchStruct *l) {
    int i;
    l->central_lock = clhLockInit();
    for (i = 0; i < HSYNCH_NCLUSTERS; i++) {
        l->Tail[i].ptr = getAlignedMemory(CACHE_LINE_SIZE, sizeof(HSynchLockNode));
        l->Tail[i].ptr->next = null;
        l->Tail[i].ptr->locked = false;
        l->Tail[i].ptr->completed = false;
    }
#ifdef DEBUG
    l->rounds = l->counter =0;
#endif
    StoreFence();
}

#endif
