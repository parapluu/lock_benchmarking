#ifndef _CCSYNCH_H_
#define _CCSYNCH_H_

#if defined(sun) || defined(_sun)
#    include <schedctl.h>
#endif

#include "synch_algs_config.h"
#include "synch_algs_primitives.h"
//#include "rand.h"

#define DEQUEUE_ARG INT_MIN

const int CCSIM_HELP_BOUND = MAX_NUM_OF_HELPED_OPS;
    //3 * N_THREADS;

typedef struct HalfLockNode {
    char pad1[128];
    struct LockNode *next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
    char pad2[128];
} HalfLockNode;

typedef struct LockNode {
    char pad1[128];
    struct LockNode *next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
    int32_t align[PAD_CACHE(sizeof(HalfLockNode))];
    char pad2[128];
} LockNode;

typedef struct ThreadState {
    char pad1[128];
    LockNode *next_node;
    int toggle;
#if defined(__sun) || defined(sun)
    schedctl_t *schedule_control;
#endif
    char pad2[128];
} ThreadState;

typedef struct LockStruct {
    char pad1[128];
    volatile LockNode *Tail CACHE_ALIGN;
#ifdef DEBUG
    volatile int rounds CACHE_ALIGN;
    volatile int counter;
    volatile int combiner_counter[N_THREADS];
#endif
    char pad2[128];
} LockStruct;


inline static void threadStateInit(ThreadState *st_thread) {
    st_thread->next_node = getAlignedMemory(CACHE_LINE_SIZE, sizeof(LockNode));
#ifdef sun
    st_thread->schedule_control = schedctl_init();
#endif
}

inline static RetVal applyOp(LockStruct *l, ThreadState *st_thread, ArgVal arg, int pid) {
    volatile LockNode *p;
    volatile LockNode *cur;
    register LockNode *next_node, *tmp_next;
    register int counter = 0;

    next_node = st_thread->next_node;
    next_node->next = null;
    next_node->locked = true;
    next_node->completed = false;

#if defined(__sun) || defined(sun)
    schedctl_start(st_thread->schedule_control);
#endif
    cur = (LockNode * volatile)SWAP(&l->Tail, next_node);
    cur->arg_ret = arg;
    cur->next = (LockNode *)next_node;
    st_thread->next_node = (LockNode *)cur;
#if defined(__sun) || defined(sun)
    schedctl_stop(st_thread->schedule_control);
#endif

    while (cur->locked) {                      // spinning
#if N_THREADS > USE_CPUS
        sched_yield();
#elif defined(sparc)
        FullFence();
        FullFence();
        FullFence();
        FullFence();
#else
        __sync_synchronize();
#endif
    }
    p = cur;                                // I am not been helped
    if (cur->completed)                     // I have been helped
        return cur->arg_ret;
#if defined(__sun) || defined(sun)
    schedctl_start(st_thread->schedule_control);
#endif
#ifdef DEBUG
    l->rounds++;
    l->combiner_counter[pid] += 1;
#endif
    while (p->next != null && counter < CCSIM_HELP_BOUND) {
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
    StoreFence();
#if defined(__sun) || defined(sun)
    schedctl_stop(st_thread->schedule_control);
#endif

#ifdef QUEUE_STATS
    helpSeasonsPerformed.value++;
    numberOfDeques.value = numberOfDeques.value + counter;
#endif

    return cur->arg_ret;
}

void ccsynch_lock_init(LockStruct *l) {
    l->Tail = getAlignedMemory(CACHE_LINE_SIZE, sizeof(LockNode));
    l->Tail->next = null;
    l->Tail->locked = false;
    l->Tail->completed = false;

#ifdef DEBUG
    int i;

    l->rounds = l->counter =0;
    for (i=0; i < N_THREADS; i++)
        l->combiner_counter[i] += 1;
#endif
    StoreFence();
}

#endif
