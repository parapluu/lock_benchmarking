#ifndef _CLH_H_

#define _CLH_H_

#include <stdlib.h>
#include <pthread.h>
#include "synch_algs_config.h"
#include "synch_algs_primitives.h"

#ifdef POSIX_LOCKS

typedef pthread_mutex_t CLHLockStruct;


static inline void clhLock(LockStruct *l, int pid) {
    pthread_mutex_lock(l);
}

static inline void clhUnlock(LockStruct *l, int pid) {
    pthread_mutex_unlock(l);
}

LockStruct *clhLockInit(void) {
    LockStruct *l, tmp = PTHREAD_MUTEX_INITIALIZER;
    int error;

    error = posix_memalign((void *)&l, CACHE_LINE_SIZE, sizeof(CLHLockStruct));
    *l = tmp;
    return l;
}

#else
typedef union CLHLockNode {
    bool locked;
    char align[CACHE_LINE_SIZE];
} CLHLockNode;

typedef struct CLHLockStruct {
    volatile CLHLockNode *Tail CACHE_ALIGN;
    char pad1[128];
    //    volatile CLHLockNode *MyNode[N_THREADS] CACHE_ALIGN;
    //    volatile CLHLockNode *MyPred[N_THREADS] CACHE_ALIGN;
} CLHLockStruct;

typedef struct CLHThreadLocalData {
    char pad1[128];
    volatile CLHLockNode *MyNode CACHE_ALIGN;
    volatile CLHLockNode *MyPred CACHE_ALIGN;
    char pad2[128 - 2*sizeof(CLHLockNode *)];
} CLHThreadLocalData;

__thread CLHThreadLocalData threadLocalData __attribute__((aligned(64)));


static inline void clhLock(CLHLockStruct *l, int pid) {
    threadLocalData.MyNode->locked = true;
    threadLocalData.MyPred = (CLHLockNode *)__SWAP(&l->Tail, (void *)threadLocalData.MyNode);
    while (threadLocalData.MyPred->locked == true) {
#if N_THREADS > USE_CPUS
        sched_yield();
#else
        ;
#endif
    }
}

static inline void clhUnlock(CLHLockStruct *l, int pid) {
    threadLocalData.MyNode->locked = false;
    threadLocalData.MyNode = threadLocalData.MyPred;
#ifdef sparc
    StoreFence();
#endif
}

void clhThreadLocalInit(){
    threadLocalData.MyNode = getAlignedMemory(CACHE_LINE_SIZE, sizeof(CLHLockNode));
    threadLocalData.MyPred = null;
}

CLHLockStruct *clhLockInit(void) {
    CLHLockStruct *l;
    //    int j;

    l = getAlignedMemory(CACHE_LINE_SIZE, sizeof(CLHLockStruct));
    l->Tail = getAlignedMemory(CACHE_LINE_SIZE, sizeof(CLHLockNode));
    l->Tail->locked = false;

    //    for (j = 0; j < N_THREADS; j++) {
    //    l->MyNode[j] = getAlignedMemory(CACHE_LINE_SIZE, sizeof(CLHLockNode));
    //    l->MyPred[j] = null;
    //}

    return l;
}

void clhLockInitExisting(CLHLockStruct * l) {
    l->Tail = getAlignedMemory(CACHE_LINE_SIZE, sizeof(CLHLockNode));
    l->Tail->locked = false;
}

#endif

#endif
