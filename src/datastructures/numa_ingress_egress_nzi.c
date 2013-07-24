#include "numa_ingress_egress_nzi.h"

__thread CacheLinePaddedInt myIngressEgressArriveNumaNode __attribute__((aligned(64)));
