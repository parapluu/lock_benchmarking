#ifndef __SKIPLIST_H__
#define __SKIPLIST_H__

#include "kvset.h"
#include "stdlib.h"

KVSet * new_skiplist(int (*compare_function)(void *, void *), 
                     void (*free_function)(void *),
                     void *(*malloc_function)(size_t),
                     unsigned int key_offset); 

KVSet * new_skiplist_default(void);

#endif
