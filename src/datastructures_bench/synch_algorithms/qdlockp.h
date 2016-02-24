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
    bool locked;
    load_acq(locked, lock->lockWord.value);
    if(!locked){
        return !__sync_lock_test_and_set(&lock->lockWord.value, true);
    }else{
        return false;
    }
}

//Multi write queue

#define MWQ_CAPACITY MAX_NUM_OF_HELPED_OPS

typedef struct DelegateRequestEntryImpl {
    void (*request)(int, int*);
    int data;
    int * responseLocation;
} DelegateRequestEntry;

typedef struct DRMWQImpl {
    char padd2[64];
    CacheLinePaddedULong elementCount;
    DelegateRequestEntry elements[MWQ_CAPACITY];
    char padd3[64 - ((sizeof(DelegateRequestEntry)*MWQ_CAPACITY) % 64)];
} DRMWQueue;

DRMWQueue * drmvqueue_create();
DRMWQueue * drmvqueue_initialize(DRMWQueue * queue);
static void drmvqueue_free(DRMWQueue * queue);
static bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e);
static void drmvqueue_flush(DRMWQueue * queue);
static void drmvqueue_reset_fully_read(DRMWQueue *  queue);


static inline
int CAS_fetch_and_add(int * valueAddress, int incrementWith){
    int oldValCAS;
    int oldVal = ACCESS_ONCE(*valueAddress);
    while(true){
        oldValCAS = __sync_val_compare_and_swap(valueAddress, oldVal, oldVal + incrementWith);
        if(oldVal == oldValCAS){
            return oldVal;
        }else{
            oldVal = oldValCAS;
        }
    }
}

#ifdef CAS_FETCH_AND_ADD
#define FETCH_AND_ADD(valueAddress, incrementWith) CAS_fetch_and_add(valueAddress, incrementWith) 
#else
#define FETCH_AND_ADD(valueAddress, incrementWith) __sync_fetch_and_add(valueAddress, incrementWith) 
#endif 

static inline
unsigned long min(unsigned long i1, unsigned long i2){
    return i1 < i2 ? i1 : i2;
}

DRMWQueue * drmvqueue_create(){
    DRMWQueue * queue = (DRMWQueue *)malloc(sizeof(DRMWQueue));
    return drmvqueue_initialize(queue);
}

DRMWQueue * drmvqueue_initialize(DRMWQueue * queue){
    for(int i = 0; i < MWQ_CAPACITY; i++){
        queue->elements[i].request = NULL;
        queue->elements[i].data = 0;
        queue->elements[i].responseLocation = NULL;
    }
    queue->elementCount.value = MWQ_CAPACITY;
    __sync_synchronize();
    return queue;
}

void drmvqueue_free(DRMWQueue * queue){
    free(queue);
}

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    int check;
    load_acq(check, queue->elementCount.value);
    if(check >= MWQ_CAPACITY){
        return false;
    }
    int index = FETCH_AND_ADD(&queue->elementCount.value, 1);
    if(index < MWQ_CAPACITY){
        store_rel(queue->elements[index].responseLocation, e.responseLocation);
        store_rel(queue->elements[index].data, e.data);
        store_rel(queue->elements[index].request, e.request);
        __sync_synchronize();//Flush
        return true;
    }else{
        return false;
    }

}
//#define QUEUE_STATS
#ifdef QUEUE_STATS
__thread int flush_dequeus = 0;
__thread int flushs = 0;
#endif 

#define CAS_FLUSH
//#define TACAS_FLUSH
#ifdef CAS_FLUSH

static inline
void drmvqueue_flush(DRMWQueue * queue){
    int todo = 0;
    bool open = true;
    while (open) {
        int done = todo;
        load_acq(todo, queue->elementCount.value);
        if (todo == done) { /* close queue */
            todo =get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY);
            open = false;
        }else if(todo >= MWQ_CAPACITY){
            todo = MWQ_CAPACITY;
            open = false;
        }
        for (int index = done ; index < todo ; index++) {
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[index].request);
            load_acq(e.data, queue->elements[index].data);
            load_acq(e.responseLocation, queue->elements[index].responseLocation);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[index].request);
                load_acq(e.data, queue->elements[index].data);
                load_acq(e.responseLocation, queue->elements[index].responseLocation);
            }
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[index].request, NULL);
        }
    }
#ifdef QUEUE_STATS
    helpSeasonsPerformed.value++;
    numberOfDeques.value = numberOfDeques.value + todo;
#endif
}

#elif defined (TACAS_FLUSH)

static inline
void drmvqueue_flush(DRMWQueue * queue){
    int closed = false;
    int index = 0;
    int todo = 0;
    int done = todo;
    while (done < MWQ_CAPACITY) {
        done = todo;
        load_acq(todo, queue->elementCount.value);
        if(done == todo){
            todo = __sync_val_compare_and_swap(&queue->elementCount.value, done, MWQ_CAPACITY);
        }
        if (todo == done) { /* close queue */
            done = MWQ_CAPACITY;
        }else if(todo >= MWQ_CAPACITY){
            todo = MWQ_CAPACITY;
            done = MWQ_CAPACITY;
        }
        for ( ; index < todo ; index++) {
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[index].request);
            load_acq(e.data, queue->elements[index].data);
            load_acq(e.responseLocation, queue->elements[index].responseLocation);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[index].request);
                load_acq(e.data, queue->elements[index].data);
                load_acq(e.responseLocation, queue->elements[index].responseLocation);
            }
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[index].request, NULL);
        }
    }
}

#else

static inline
void drmvqueue_flush(DRMWQueue * queue){
    int closed = false;
    int index = 0;
    int todo = 0;
    int done = todo;
    while (done < MWQ_CAPACITY) {
        done = todo;
        load_acq(todo, queue->elementCount.value);
        if (todo == done) { /* close queue */
            todo = get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY);
            done = MWQ_CAPACITY;
        }
        if(todo >= MWQ_CAPACITY){
            todo = MWQ_CAPACITY;
            done = MWQ_CAPACITY;
        }
        for ( ; index < todo ; index++) {
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[index].request);
            load_acq(e.data, queue->elements[index].data);
            load_acq(e.responseLocation, queue->elements[index].responseLocation);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[index].request);
                load_acq(e.data, queue->elements[index].data);
                load_acq(e.responseLocation, queue->elements[index].responseLocation);
            }
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[index].request, NULL);
        }
    }
}

#endif
    


static inline
void drmvqueue_reset_fully_read(DRMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
}



//QD Lock


typedef struct AgnosticDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(int, int *);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    TATASLock lock;
    char pad4[64];
} AgnosticDXLock;

void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *));
AgnosticDXLock * adxlock_create(void (*writer)(int, int *)){
    AgnosticDXLock * lock = (AgnosticDXLock *)malloc(sizeof(AgnosticDXLock));
    adxlock_initialize(lock, writer);
    return lock;
}

void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    tataslock_initialize(&lock->lock, defaultWriter);
    drmvqueue_initialize(&lock->writeQueue);
    __sync_synchronize();
}

void adxlock_free(AgnosticDXLock * lock){
    free(lock);
}

void adxlock_register_this_thread(){
}

static inline
void adxlock_write_with_response(AgnosticDXLock *lock, 
                                 void (*delgateFun)(int, int *), 
                                 int data, 
                                 int * responseLocation){
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    while (true) {
        if(tataslock_try_write_read_lock(&lock->lock)){
            drmvqueue_reset_fully_read(&lock->writeQueue);
            delgateFun(data, responseLocation);
            drmvqueue_flush(&lock->writeQueue);
            tataslock_write_read_unlock(&lock->lock);
            return;
        } else if (drmvqueue_offer(&lock->writeQueue, e)){
            return ;
        }
        sched_yield();
    }
}

static inline
int adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                      void (*delgateFun)(int, int *), 
                                      int data){
    int counter = 0;
    int returnValue = INT_MIN;
    int currentValue;
    adxlock_write_with_response(lock, delgateFun, data, &returnValue);
    load_acq(currentValue, returnValue);
    while(currentValue == INT_MIN){
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }else{
            __sync_synchronize();
        }
        counter = counter + 1;
        load_acq(currentValue, returnValue);
    }
    return currentValue;
}

static inline
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(int, int *), 
                      int data) {
    adxlock_write_with_response(lock, delgateFun, data, NULL);
}

void adxlock_write(AgnosticDXLock *lock, int writeInfo) {
    adxlock_delegate(lock, lock->defaultWriter, writeInfo);
}

void adxlock_write_read_lock(AgnosticDXLock *lock) {
    tataslock_write_read_lock(&lock->lock);    
    drmvqueue_reset_fully_read(&lock->writeQueue);
    __sync_synchronize();//Flush
}

void adxlock_write_read_unlock(AgnosticDXLock * lock) {
    drmvqueue_flush(&lock->writeQueue);
    tataslock_write_read_unlock(&lock->lock);
}

void adxlock_read_lock(AgnosticDXLock *lock) {
    adxlock_write_read_lock(lock);
}

void adxlock_read_unlock(AgnosticDXLock *lock) {
    adxlock_write_read_unlock(lock);
}



#endif
