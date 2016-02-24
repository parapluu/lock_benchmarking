#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <sched.h>
#include "cohort_lock.h"
#include "utils/numa_node_info_support.h"


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
