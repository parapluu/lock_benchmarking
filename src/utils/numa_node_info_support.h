#ifndef NUMA_NODE_INFO_SUPPORT_H
#define NUMA_NODE_INFO_SUPPORT_H

#include <sched.h>
#include "smp_utils.h"

typedef union CPUToNodeMapWrapperImpl {
    char padding[64];
    char value[NUMBER_OF_NUMA_NODES * NUMBER_OF_CPUS_PER_NODE];
    char pad[64 - ((sizeof(char) * NUMBER_OF_NUMA_NODES * NUMBER_OF_CPUS_PER_NODE) % 64)];
} CPUToNodeMapWrapper;

extern CPUToNodeMapWrapper CPUToNodeMap __attribute__((aligned(64)));

static inline
int numa_node_id(){
    return CPUToNodeMap.value[sched_getcpu()];
}

#endif
