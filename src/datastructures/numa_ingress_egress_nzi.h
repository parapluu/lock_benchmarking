#include <stdbool.h>
#include "utils/smp_utils.h"
#include "utils/numa_node_info_support.h"

#ifndef READER_GROUPS_NZI_H
#define READER_GROUPS_NZI_H

#define INGRESS_EGRESS_PADDING 32

extern __thread CacheLinePaddedInt myIngressEgressArriveNumaNode __attribute__((aligned(64)));

typedef struct IngressEgressCounterImpl {
    unsigned long ingress;
    char pad1[INGRESS_EGRESS_PADDING - sizeof(unsigned long) % INGRESS_EGRESS_PADDING];
    unsigned long egress;
    char pad2[INGRESS_EGRESS_PADDING - sizeof(unsigned long) % INGRESS_EGRESS_PADDING];
    char pad3[64];
} IngressEgressCounter;

typedef struct NUMAIngressEgressNZIImpl {
    IngressEgressCounter readerCounters[NUMBER_OF_NUMA_NODES];
} NUMAIngressEgress;

static inline
void nienzi_initialize(NUMAIngressEgress * nzi){
    for(int i = 0; i < NUMBER_OF_NUMA_NODES; i++){
        nzi->readerCounters[i].ingress = 0;
        nzi->readerCounters[i].egress = 0;
    }
    __sync_synchronize();
}

static inline
void nienzi_arrive(NUMAIngressEgress * nzi){
    int myNumaNode = numa_node_id();
    myIngressEgressArriveNumaNode.value = myNumaNode;
    __sync_fetch_and_add(&nzi->readerCounters[myNumaNode].ingress, 1);
}

static inline
void nienzi_depart(NUMAIngressEgress * nzi){
    int myNumaNode = myIngressEgressArriveNumaNode.value;
    __sync_fetch_and_add(&nzi->readerCounters[myNumaNode].egress, 1);
}


static inline
bool nienzi_query(NUMAIngressEgress * nzi){
    for(int i = 0; i < NUMBER_OF_NUMA_NODES; i++){
        if(ACCESS_ONCE(nzi->readerCounters[i].ingress) !=
           ACCESS_ONCE(nzi->readerCounters[i].egress)){
            return false;
        }
    }
    return true;
}

static inline
void nienzi_wait_unil_empty(NUMAIngressEgress * nzi){
    int ingressCount;
    int egressCount;
    for(int i = 0; i < NUMBER_OF_NUMA_NODES; i++){
        load_acq(ingressCount, nzi->readerCounters[i].ingress); 
        load_acq(egressCount, nzi->readerCounters[i].egress);
        while(ingressCount != egressCount){
            __sync_synchronize();
            load_acq(ingressCount, nzi->readerCounters[i].ingress); 
            load_acq(egressCount, nzi->readerCounters[i].egress);
        }
    }
}

#endif
