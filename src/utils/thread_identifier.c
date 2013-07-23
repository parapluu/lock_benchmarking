#include "thread_identifier.h"

__thread CacheLinePaddedInt myId __attribute__((aligned(128)));
int myIdCounter __attribute__((aligned(128))) = 0;

void assign_id_to_thread(){
    myId.value = __sync_fetch_and_add(&myIdCounter, 1);
}
