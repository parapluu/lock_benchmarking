#ifndef SUPPORT_MANY_NON_ZERO_INDICATOR_TYPES_H
#define SUPPORT_MANY_NON_ZERO_INDICATOR_TYPES_H

#ifdef NZI_TYPE_ReaderGroups
//***********************************
//ReaderGroups
//***********************************
#include "datastructures/reader_groups_nzi.h"

#define NZI_DATATYPE_NAME ReaderGroupsNZI
#define NZI_FUN_PREFIX rgnzi

#elif defined (NZI_TYPE_NUMAIngressEgressCounter)
//***********************************
//NUMA Ingress Egress Counter
//***********************************
#include "datastructures/numa_ingress_egress_nzi.h"

#define NZI_DATATYPE_NAME NUMAIngressEgress
#define NZI_FUN_PREFIX nienzi

#else

#define NZI_DATATYPE_NAME NoNZIDatatypeSpecified
#define NZI_FUN_PREFIX no_such_nzi_type_prefix

#endif

#ifdef NZI_FUN_PREFIX

#define MY_NZI_CONCAT(a,b) a ## _ ## b                                                
#define MY_NZI_EVAL_CONCAT(a,b) MY_NZI_CONCAT(a,b)                                        
#define MY_NZI_FUN(name) MY_NZI_EVAL_CONCAT(NZI_FUN_PREFIX, name)                              

#define NZI_INITIALIZE(nzi) MY_NZI_FUN(initialize)(nzi)
#define NZI_ARRIVE(nzi) MY_NZI_FUN(arrive)(nzi)
#define NZI_DEPART(nzi) MY_NZI_FUN(depart)(nzi)
#define NZI_QUERY(nzi) MY_NZI_FUN(query)(nzi)
#define NZI_WAIT_UNIL_EMPTY(nzi) MY_NZI_FUN(wait_unil_empty)(nzi)

#endif

#endif
