#ifndef QDLOCK_H
#define QDLOCK_H

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

//TATAS Lock

typedef struct TATASLockImpl {
    char pad1[64];
    void (*writer)(int, int *);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    char pad3[64];
    CacheLinePaddedBool lockWord;
    char pad4[64];
} TATASLock;


void tataslock_initialize(TATASLock * lock, void (*writer)(int, int *)){
    lock->writer = writer;
    lock->lockWord.value = 0;
    __sync_synchronize();
}

void tataslock_free(TATASLock * lock){
    free(lock);
}

void tataslock_register_this_thread(){
}

static inline
void tataslock_write_read_lock(TATASLock *lock);
static inline
void tataslock_write_read_unlock(TATASLock * lock);
void tataslock_write(TATASLock *lock, int writeInfo) {
    tataslock_write_read_lock(lock);
    lock->writer(writeInfo, 0);
    tataslock_write_read_unlock(lock);
}

void tataslock_write_read_lock(TATASLock *lock) {
    bool currentlylocked;
    while(true){
        load_acq(currentlylocked, lock->lockWord.value);
        while(currentlylocked){
            load_acq(currentlylocked, lock->lockWord.value);
        }
        currentlylocked = __sync_lock_test_and_set(&lock->lockWord.value, true);
        if(!currentlylocked){
            //Was not locked before operation
            return;
        }
        __sync_synchronize();//Pause instruction?
    }
}

static inline
void tataslock_write_read_unlock(TATASLock * lock) {
    __sync_lock_release(&lock->lockWord.value);
}

void tataslock_read_lock(TATASLock *lock) {
    tataslock_write_read_lock(lock);
}

void tataslock_read_unlock(TATASLock *lock) {
    tataslock_write_read_unlock(lock);
}


static inline
bool tataslock_is_locked(TATASLock *lock){
    bool locked;
    load_acq(locked, lock->lockWord.value);
    return locked;
}

static inline
bool tataslock_try_write_read_lock(TATASLock *lock) {
    return !__sync_lock_test_and_set(&lock->lockWord.value, true);
}


//Flat combining algorithm
//From: https://github.com/mit-carbon/Flat-Combining

#define _NUM_REP MAX_NUM_OF_HELPED_OPS
#define _REP_THRESHOLD 1
#define  _NULL_VALUE (0)
#define  _DEQ_VALUE (INT_MIN)

//list inner types ------------------------------
typedef struct SlotInfo {
    int _req_ans;    //here 1 can post the request and wait for answer
    int _time_stamp; //when 0 not connected
    struct SlotInfo* _next; //when null not connected
    void* _custem_info;
    bool _deq_pending;
} SlotInfo;


typedef struct SlotInfoPtr {
    char pad1[128];
    SlotInfo * value;
    char pad2[128 - sizeof(SlotInfo *)];
} SlotInfoPtr;

//list fields -----------------------------------
//TODO add algin


typedef struct FlatComb {
    char pad1[128];
    SlotInfo * _tail_slot;
    char pad2[128 - sizeof(SlotInfo *)];
    int _timestamp;
    char pad3[128 - sizeof(int)];
    TATASLock lock;
} FlatComb;



void init_flat_comb(FlatComb * flatcomb){
    flatcomb->_tail_slot = NULL;
    flatcomb->_timestamp = 0;
    tataslock_initialize(&flatcomb->lock, NULL);
    __sync_synchronize();
}

//list helper function --------------------------

SlotInfo* get_new_slot(FlatComb * flatcomb, SlotInfo * _tls_slot_info) {
    _tls_slot_info = malloc(sizeof(SlotInfo));
    _tls_slot_info->_req_ans     = _NULL_VALUE;
    _tls_slot_info->_time_stamp  = 0;
    _tls_slot_info->_next        = NULL;
    _tls_slot_info->_custem_info = NULL;
    _tls_slot_info->_deq_pending = false;
    SlotInfo* curr_tail;
    do {
        load_acq(curr_tail, flatcomb->_tail_slot);
        store_rel(_tls_slot_info->_next, curr_tail);
    } while(false == __sync_bool_compare_and_swap(&flatcomb->_tail_slot, curr_tail, _tls_slot_info));

    return _tls_slot_info;
}

/* void enq_slot(FlatComb * flatcomb, SlotInfo* p_slot) { */
/*     SlotInfo* curr_tail; */
/*     do { */
/*         //TODO make sure atomic */
/*         curr_tail = flatcomb->_tail_slot; */
/*         p_slot->_next = curr_tail; */
/*     } while(false == __sync_bool_compare_and_swap(&flatcomb->_tail_slot, curr_tail, p_slot)); */
/* } */

/* void enq_slot_if_needed(FlatComb * flatcomb, SlotInfo* p_slot) { */
/*     if(NULL == p_slot->_next) { */
/*         enq_slot(flatcomb, p_slot); */
/*     } */
/* } */

static inline void flat_combining(FlatComb * flatcomb) {
#ifdef QUEUE_STATS
    helpSeasonsPerformed.value++;
#endif
    int total_changes = 0;
    for (int iTry=0;iTry<_NUM_REP; ++iTry) {
        //Memory::read_barrier();
        int num_changes=0;
        SlotInfo* curr_slot;
        load_acq(curr_slot, flatcomb->_tail_slot);
        while(NULL != curr_slot) {
            int curr_value;
            load_acq(curr_value, curr_slot->_req_ans);
            if(curr_value == _DEQ_VALUE) {
                ++num_changes;
                int writeOut = -dequeue_cs();
                store_rel(curr_slot->_req_ans, writeOut);
                store_rel(curr_slot->_time_stamp, 0);
            } else if(curr_value > _NULL_VALUE){
                ++num_changes;
                enqueue_cs(curr_value);
                store_rel(curr_slot->_req_ans, _NULL_VALUE);
                store_rel(curr_slot->_time_stamp, 0);
            }

            load_acq(curr_slot, curr_slot->_next);
        }//while on slots
        total_changes = total_changes + num_changes;
        if((num_changes < _REP_THRESHOLD) || 
           (total_changes >= MAX_NUM_OF_HELPED_OPS))
            break;
        //Memory::write_barrier();
    }//for repetition
#ifdef QUEUE_STATS
    numberOfDeques.value = numberOfDeques.value + total_changes;
#endif
}


int do_op(FlatComb * flatcomb, SlotInfoPtr * _tls_slot_info, int op_code) {
    SlotInfo* my_slot = _tls_slot_info->value;
    if(NULL == my_slot){
        my_slot = _tls_slot_info->value = get_new_slot(flatcomb, my_slot);
    }

    store_rel(my_slot->_req_ans, op_code);
    do {
        //TODO what is this? Never dequed?
        //if(NULL == my_next)
        //    enq_slot(my_slot);

        //        boolean is_cas = false;
        
        if(tataslock_try_write_read_lock(&flatcomb->lock)) {
            //            machine_start_fc(iThread);
            flat_combining(flatcomb);
            int returnValue = my_slot->_req_ans;

            tataslock_write_read_unlock(&flatcomb->lock);
            //_fc_lock.set(0);
            //            machine_end_fc(iThread);

            return (-returnValue) - 2;
        } else {
            //            Memory::write_barrier();
            //__sync_synchronize();
            int currentValue = op_code;
            do {
                load_acq(currentValue, my_slot->_req_ans);
                //            Memory::read_barrier();
                if(op_code != currentValue) {
                    return (-currentValue) - 2;
                }
            }while(tataslock_is_locked(&flatcomb->lock));
        }
    } while(true);
}



#endif
