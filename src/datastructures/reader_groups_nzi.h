#include <stdbool.h>
#include "utils/smp_utils.h"
#include "utils/thread_identifier.h"

#ifndef READER_GROUPS_NZI_H
#define READER_GROUPS_NZI_H

typedef struct ReaderGroupsNZIImpl {
    CacheLinePaddedInt readerGroups[NUMBER_OF_READER_GROUPS];
} ReaderGroupsNZI;

static inline
void rgnzi_initialize(ReaderGroupsNZI * nzi){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        nzi->readerGroups[i].value = 0;
    }
    __sync_synchronize();
}

static inline
void rgnzi_arrive(ReaderGroupsNZI * nzi){
    __sync_fetch_and_add(&nzi->readerGroups[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
}

static inline
void rgnzi_depart(ReaderGroupsNZI * nzi){
    __sync_fetch_and_sub(&nzi->readerGroups[myId.value % NUMBER_OF_READER_GROUPS].value, 1);
}


static inline
bool rgnzi_query(ReaderGroupsNZI * nzi){
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        if(ACCESS_ONCE(nzi->readerGroups[i].value) > 0){
            return false;
        }
    }
    return true;
}

static inline
void rgnzi_wait_unil_empty(ReaderGroupsNZI * nzi){
    int count;
    for(int i = 0; i < NUMBER_OF_READER_GROUPS; i++){
        load_acq(count, nzi->readerGroups[i].value); 
        while(count > 0){
            __sync_synchronize();
            load_acq(count, nzi->readerGroups[i].value);
        }
    }
}

#endif
