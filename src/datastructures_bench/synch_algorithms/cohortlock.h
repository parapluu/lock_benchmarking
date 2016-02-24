#ifndef COHORTLOCK_H
#define COHORTLOCK_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <limits.h>


#ifndef SMP_UTILS_H
#define SMP_UTILS_H

//SMP Utils

//Make sure compiler does not optimize away memory access
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

//Atomic get
#define GET(value_ptr)  __sync_fetch_and_add(value_ptr, 0)

//Compiller barrier
#define barrier() __asm__ __volatile__("": : :"memory")

//See the following URL for explanation of acquire and release semantics:
//http://preshing.com/20120913/acquire-and-release-semantics

//Load with acquire barrier
#if __x86_64__
#define load_acq(assign_to,load_from) \
    assign_to = ACCESS_ONCE(load_from)
#else
#define load_acq(assign_to,load_from)           \
    do {                                        \
        barrier();                              \
        assign_to = ACCESS_ONCE(load_from);     \
        __sync_synchronize();                   \
    } while(0)
#endif


//Store with release barrier
#if __x86_64__
#define store_rel(store_to,store_value) \
    do{                                 \
        barrier();                      \
        store_to = store_value;        \
        barrier();                      \
    }while(0);
#else
#define store_rel(store_to,store_value) \
    do{                                 \
        __sync_synchronize();           \
        store_to = store_value;        \
        barrier();                      \
    }while(0);
#endif

//Intel pause instruction
#if __x86_64__
#define pause_instruction() \
  __asm volatile ("pause")
#else
#define pause_instruction() \
  __sync_synchronize()
#endif

static inline
int get_and_set_int(int * pointerToOldValue, int newValue){
    int x = ACCESS_ONCE(*pointerToOldValue);
    while (true) {
        if (__sync_bool_compare_and_swap(pointerToOldValue, x, newValue))
            return x;
        x = ACCESS_ONCE(*pointerToOldValue);
    }
}

static inline
unsigned long get_and_set_ulong(unsigned long * pointerToOldValue, unsigned long newValue){
    unsigned long x = ACCESS_ONCE(*pointerToOldValue);
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
    char padding[128];
} CacheLinePaddedInt;


typedef union CacheLinePaddedULongImpl {
    unsigned long value;
    char padding[128];
} CacheLinePaddedULong;

typedef union CacheLinePaddedDoubleImpl {
    double value;
    char padding[128];
} CacheLinePaddedDouble;

typedef union CacheLinePaddedPointerImpl {
    void * value;
    char padding[64];
} CacheLinePaddedPointer;

#endif

#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"
#include "mcs_lock.h"


#ifndef TICKET_LOCK_H
#define TICKET_LOCK_H

typedef struct TicketLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedInt inCounter;
    CacheLinePaddedInt outCounter;
} TicketLock;


TicketLock * ticketlock_create(void (*writer)(void *, void **));
void ticketlock_free(TicketLock * lock);
void ticketlock_initialize(TicketLock * lock, void (*writer)(void *, void **));
void ticketlock_register_this_thread();
void ticketlock_write(TicketLock *lock, void * writeInfo);
void ticketlock_write_read_lock(TicketLock *lock);
void ticketlock_write_read_unlock(TicketLock * lock);
void ticketlock_read_lock(TicketLock *lock);
void ticketlock_read_unlock(TicketLock *lock);


#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

TicketLock * ticketlock_create(void (*writer)(void *, void **)){
    TicketLock * lock = malloc(sizeof(TicketLock));
    ticketlock_initialize(lock, writer);
    return lock;
}

void ticketlock_initialize(TicketLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->inCounter.value = 0;
    lock->outCounter.value = 0;
    __sync_synchronize();
}

void ticketlock_free(TicketLock * lock){
    free(lock);
}


void ticketlock_register_this_thread(){
}

void ticketlock_write(TicketLock *lock, void * writeInfo) {
    ticketlock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    ticketlock_write_read_unlock(lock);
}

void ticketlock_write_read_lock(TicketLock *lock) {
    int outCounter;
    int myTicket = __sync_fetch_and_add(&lock->inCounter.value, 1);
    load_acq(outCounter, lock->outCounter.value);
    while(outCounter != myTicket){
        load_acq(outCounter, lock->outCounter.value);
        __sync_synchronize();
    }
}

void ticketlock_write_read_unlock(TicketLock * lock) {
    __sync_fetch_and_add(&lock->outCounter.value, 1);
}

void ticketlock_read_lock(TicketLock *lock) {
    ticketlock_write_read_lock(lock);
}

void ticketlock_read_unlock(TicketLock *lock) {
    ticketlock_write_read_unlock(lock);
}


#endif

#include <stdbool.h>
#include "utils/smp_utils.h"


#ifndef ATICKET_LOCK_H
#define ATICKET_LOCK_H

typedef struct ATicketLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    CacheLinePaddedInt inCounter;
    CacheLinePaddedInt outCounter;
    CacheLinePaddedInt spinAreas[ARRAY_SIZE];
} ATicketLock;


ATicketLock * aticketlock_create(void (*writer)(void *, void **));
void aticketlock_free(ATicketLock * lock);
void aticketlock_initialize(ATicketLock * lock, void (*writer)(void *, void **));
void aticketlock_register_this_thread();
void aticketlock_write(ATicketLock *lock, void * writeInfo);
void aticketlock_write_read_lock(ATicketLock *lock);
void aticketlock_write_read_unlock(ATicketLock * lock);
void aticketlock_read_lock(ATicketLock *lock);
void aticketlock_read_unlock(ATicketLock *lock);

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "aticket_lock.h"

 
ATicketLock * aticketlock_create(void (*writer)(void *, void **)){
    ATicketLock * lock = malloc(sizeof(ATicketLock));
    aticketlock_initialize(lock, writer);
    return lock;
}

void aticketlock_initialize(ATicketLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    lock->inCounter.value = 0;
    lock->outCounter.value = 0;
    for(int i = 0; i < ARRAY_SIZE; i++){
        lock->spinAreas[i].value = 0;
    }
    __sync_synchronize();
}

void aticketlock_free(ATicketLock * lock){
    free(lock);
}


void aticketlock_register_this_thread(){
}

void aticketlock_write(ATicketLock *lock, void * writeInfo) {
    aticketlock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    aticketlock_write_read_unlock(lock);
}

void aticketlock_write_read_lock(ATicketLock *lock) {
    int waitTicket;
    int myTicket = __sync_fetch_and_add(&lock->inCounter.value, 1);
    int spinPosition = myTicket % ARRAY_SIZE;
    load_acq(waitTicket, lock->spinAreas[spinPosition].value);
    while(waitTicket != myTicket){
        __sync_synchronize();
        load_acq(waitTicket, lock->spinAreas[spinPosition].value);
    }
}

void aticketlock_write_read_unlock(ATicketLock * lock) {
    lock->outCounter.value = lock->outCounter.value + 1;
    int nextPosition = lock->outCounter.value % ARRAY_SIZE;
    store_rel(lock->spinAreas[nextPosition].value, lock->outCounter.value);
    __sync_synchronize();//Push change
}

void aticketlock_read_lock(ATicketLock *lock) {
    aticketlock_write_read_lock(lock);
}

void aticketlock_read_unlock(ATicketLock *lock) {
    aticketlock_write_read_unlock(lock);
}

#endif

#include <stdbool.h>
#include "utils/smp_utils.h"
#include "common_lock_constants.h"
#include "ticket_lock.h"
#include "aticket_lock.h"

#define MAXIMUM_NUMBER_OF_HAND_OVERS MAX_NUM_OF_HELPED_OPS

typedef struct NodeLocalLockDataImpl {
    char pad1[64];
    TicketLock lock;
    CacheLinePaddedInt numberOfHandOvers;
    CacheLinePaddedBool needToTakeGlobalLock;
} NodeLocalLockData;

typedef struct CohortLockImpl {
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    ATicketLock globalLock;
    NodeLocalLockData localLockData[NUMBER_OF_NUMA_NODES];
} CohortLock;


CohortLock * cohortlock_create(void (*writer)(void *, void **));
void cohortlock_free(CohortLock * lock);
void cohortlock_initialize(CohortLock * lock, void (*writer)(void *, void **));
void cohortlock_register_this_thread();
void cohortlock_write(CohortLock *lock, void * writeInfo);
bool cohortlock_write_read_lock(CohortLock *lock);
void cohortlock_write_read_unlock(CohortLock * lock);
void cohortlock_read_lock(CohortLock *lock);
void cohortlock_read_unlock(CohortLock *lock);

static inline
bool cohortlock_is_locked(CohortLock *lock){
    int inCounter;
    int outCounter;
    load_acq(inCounter, lock->globalLock.inCounter.value);
    load_acq(outCounter, lock->globalLock.outCounter.value);
    return (inCounter != outCounter);
}

extern __thread CacheLinePaddedInt myLocalNode __attribute__((aligned(64)));

static inline
bool cohortlock_is_local_locked(CohortLock *lock){
    int inCounter;
    int outCounter;
    NodeLocalLockData * localData = &lock->localLockData[myLocalNode.value]; 
    load_acq(inCounter, localData->lock.inCounter.value);
    load_acq(outCounter, localData->lock.outCounter.value);
    return (inCounter != outCounter);
}

static inline
bool cohortlock_try_write_read_lock(CohortLock *lock) {
    if(!cohortlock_is_locked(lock) && 
       !cohortlock_is_local_locked(lock)){
        cohortlock_write_read_lock(lock);
        return true;
    }else{
        return false;
    }

}

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <sched.h>

__thread CacheLinePaddedInt myLocalNode __attribute__((aligned(64)));

static inline
bool nodeHasWaitingThreads(TicketLock * localLock){
    int localLockInCounter;
    int localLockOutCounter;
    load_acq(localLockInCounter, localLock->inCounter.value); 
    load_acq(localLockOutCounter, localLock->outCounter.value);
    return (localLockInCounter - localLockOutCounter) > 1;
}
 
CohortLock * cohortlock_create(void (*writer)(void *, void **)){
    CohortLock * lock = malloc(sizeof(CohortLock));
    cohortlock_initialize(lock, writer);
    return lock;
}

typedef union CPUToNodeMapWrapperImpl {
    char padding[64];
    char value[NUMBER_OF_NUMA_NODES * NUMBER_OF_CPUS_PER_NODE];
    char pad[64 - ((sizeof(char) * NUMBER_OF_NUMA_NODES * NUMBER_OF_CPUS_PER_NODE) % 64)];
} CPUToNodeMapWrapper;


CPUToNodeMapWrapper CPUToNodeMap __attribute__((aligned(64)));

static inline
int numa_node_id(){
    return CPUToNodeMap.value[sched_getcpu()];
}

void cohortlock_initialize(CohortLock * lock, void (*writer)(void *, void **)){
    lock->writer = writer;
    aticketlock_initialize(&lock->globalLock, writer);
    for(int i = 0; i < NUMBER_OF_NUMA_NODES; i++){
        ticketlock_initialize(&lock->localLockData[i].lock, writer);
        lock->localLockData[i].numberOfHandOvers.value = 0;
        lock->localLockData[i].needToTakeGlobalLock.value = true;
    }
    //Initialize CPUToNodeMap
    int numaStructure[NUMBER_OF_NUMA_NODES][NUMBER_OF_CPUS_PER_NODE] = NUMA_STRUCTURE;
    for(char node = 0; node < NUMBER_OF_NUMA_NODES; node++){
        for(int i = 0; i < NUMBER_OF_CPUS_PER_NODE; i++){
            CPUToNodeMap.value[numaStructure[(int)node][i]] = node;
        }
    }

    __sync_synchronize();
}

void cohortlock_free(CohortLock * lock){
    free(lock);
}


void cohortlock_register_this_thread(){
}

void cohortlock_write(CohortLock *lock, void * writeInfo) {
    cohortlock_write_read_lock(lock);
    lock->writer(writeInfo, NULL);
    cohortlock_write_read_unlock(lock);
}

//Returns true if it is taken over from another writer and false otherwise
bool cohortlock_write_read_lock(CohortLock *lock) {
#ifdef PINNING
    NodeLocalLockData * localData = &lock->localLockData[numa_node.value];
#else
    myLocalNode.value = numa_node_id();
    NodeLocalLockData * localData = &lock->localLockData[myLocalNode.value];
#endif
    ticketlock_write_read_lock(&localData->lock);
    if(localData->needToTakeGlobalLock.value){
        aticketlock_write_read_lock(&lock->globalLock);
        return false;
    }else{
        return true;
    }
}

void cohortlock_write_read_unlock(CohortLock * lock) {
#ifdef PINNING
    NodeLocalLockData * localData = &lock->localLockData[numa_node.value];
#else
    NodeLocalLockData * localData = &lock->localLockData[myLocalNode.value];
#endif
    if(nodeHasWaitingThreads(&localData->lock) && 
       (localData->numberOfHandOvers.value < MAXIMUM_NUMBER_OF_HAND_OVERS)){
        localData->needToTakeGlobalLock.value = false;
        localData->numberOfHandOvers.value++;
        ticketlock_write_read_unlock(&localData->lock);

    }else{
        localData->needToTakeGlobalLock.value = true;
#ifdef QUEUE_STATS
        helpSeasonsPerformed.value++;
        numberOfDeques.value =  numberOfDeques.value + localData->numberOfHandOvers.value;
#endif
        localData->numberOfHandOvers.value = 0;
        aticketlock_write_read_unlock(&lock->globalLock);
        ticketlock_write_read_unlock(&localData->lock);
    }
}

void cohortlock_read_lock(CohortLock *lock) {
    cohortlock_write_read_lock(lock);
}

void cohortlock_read_unlock(CohortLock *lock) {
    cohortlock_write_read_unlock(lock);
}

#endif
