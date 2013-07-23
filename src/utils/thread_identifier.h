#ifndef THREAD_IDENTIFIER_H
#define THREAD_IDENTIFIER_H

#include "smp_utils.h"

extern __thread CacheLinePaddedInt myId;
extern int myIdCounter;

void assign_id_to_thread();

#endif
