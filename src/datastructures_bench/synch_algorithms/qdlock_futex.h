
#ifndef QDLOCK_FUTEX_H
#define QDLOCK_FUTEX_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <limits.h>

#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>


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

//Futex Lock
/* tuning parameter */
#define MAGIC_NUMBER_WAIT_IN_TRYLOCK 11
#define MAGIC_NUMBER_ENQUEUE_TRIES 7

/* constants for states */
#define MAGIC_FREE 0
#define MAGIC_TAKEN 1
#define MAGIC_CONTENDED 2

typedef struct FutexLockImpl {
    void (*writer)(int, int *);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    char pad3[64];
    CacheLinePaddedInt lockWord;
} FutexLock;

static inline long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3) {
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

static inline void flock_notify_all(FutexLock* lock) {
    sys_futex((void*)&lock->lockWord.value, FUTEX_WAKE, 65000, NULL, NULL, 0);
}

static inline bool flock_wait(FutexLock* lock) {
    //if(atomic_load_explicit(&lock->lockWord.value, memory_order_acquire) != MAGIC_CONTENDED) {
    if(ACCESS_ONCE(lock->lockWord.value) != MAGIC_CONTENDED) {
        //int c = atomic_exchange_explicit(&lock->lockWord.value, MAGIC_CONTENDED, memory_order_release);
        int c = __sync_lock_test_and_set(&lock->lockWord.value, MAGIC_CONTENDED);
        if(c == MAGIC_FREE) {
            return true;
        }
    }
    sys_futex((void*)&lock->lockWord.value, FUTEX_WAIT, MAGIC_CONTENDED, NULL, NULL, 0);
    return false;
}

void flock_initialize(FutexLock * lock);
void flock_lock(void * lock);
void flock_lock(void * lock) {
    FutexLock *l = (FutexLock*)lock;
    int c = MAGIC_FREE;
    int oldValCAS;
    //if(!atomic_compare_exchange_strong_explicit(&l->lockWord.value, &c, MAGIC_TAKEN, memory_order_acq_rel, memory_order_acquire)) {
    oldValCAS = __sync_val_compare_and_swap(&l->lockWord.value, c, MAGIC_TAKEN);
    if(c != oldValCAS){
        if(oldValCAS != MAGIC_CONTENDED) {
            oldValCAS = __sync_lock_test_and_set(&l->lockWord.value, MAGIC_CONTENDED);
        }
        if(oldValCAS != MAGIC_FREE) {
            while(!flock_wait(l)); /* go to sleep until lock successfully taken */
        }
   }
}
static inline
void flock_unlock(void * lock) {
    FutexLock *l = (FutexLock*)lock;
    //int old = atomic_exchange_explicit(&l->lockWord.value, MAGIC_FREE, memory_order_release);
    int old = __sync_lock_test_and_set(&l->lockWord.value, MAGIC_FREE);
    if(old == MAGIC_CONTENDED) {
        flock_notify_all(l); /* TODO: notify how many? only one? */
    }
}
static inline
bool flock_is_locked(void * lock){
    FutexLock *l = (FutexLock*)lock;
    //return atomic_load_explicit(&l->lockWord.value, memory_order_acquire) != MAGIC_FREE;
    int val;
    load_acq(val, l->lockWord.value);
    return val != MAGIC_FREE;
}

bool flock_try_lock(void * lock) {
    FutexLock *l = (FutexLock*)lock;
    int c = MAGIC_FREE;
    int oldValCAS;
    if(flock_is_locked(l)) {
        return false;
    }
    //if(atomic_compare_exchange_strong_explicit(&l->lockWord.value, &c, MAGIC_TAKEN, memory_order_release, memory_order_relaxed)) {
    oldValCAS = __sync_val_compare_and_swap(&l->lockWord.value, c, MAGIC_TAKEN);
    if(c == oldValCAS){
        return true;
    } else {
        return false;
    }
}

/* TODO: can the tries parameter replaced with a PRNG call? */
static inline
bool flock_try_lock_wait(void * lock, unsigned int tries) {
    FutexLock *l = (FutexLock*)lock;
    int c = MAGIC_FREE;
    int oldValCAS;
    if(flock_is_locked(l)) {
        bool do_wait = tries%MAGIC_NUMBER_WAIT_IN_TRYLOCK == 0;
        return do_wait?flock_wait(l):false;
    }
    //if(atomic_compare_exchange_strong_explicit(&l->lockWord.value, &c, MAGIC_TAKEN, memory_order_release, memory_order_relaxed)) {
    oldValCAS = __sync_val_compare_and_swap(&l->lockWord.value, c, MAGIC_TAKEN);
    if(c == oldValCAS){
        return true;
    } else {
        /* no waiting here: lock was just acquired by another thread */
        return false;
    }
}
void flock_delegate(void * lock,
                    void (*funPtr)(unsigned int, void *), 
                    unsigned int messageSize,
                    void * messageAddress);
void * flock_delegate_or_lock(void * lock, unsigned int messageSize);
FutexLock * plain_flock_create();

void futexlock_initialize(FutexLock * lock, void (*writer)(int, int *)){
    lock->writer = writer;
    lock->lockWord.value = MAGIC_FREE;
    __sync_synchronize();
}

void futexlock_free(FutexLock * lock){
    free(lock);
}

void futexlock_register_this_thread(){
}

static inline
void futexlock_write_read_lock(FutexLock *lock);
static inline
void futexlock_write_read_unlock(FutexLock * lock);
void futexlock_write(FutexLock *lock, int writeInfo) {
    futexlock_write_read_lock(lock);
    lock->writer(writeInfo, 0);
    futexlock_write_read_unlock(lock);
}

void futexlock_write_read_lock(FutexLock *lock) {
    flock_lock(lock);
}

static inline
void futexlock_write_read_unlock(FutexLock * lock) {
    flock_unlock(lock);
}

void futexlock_read_lock(FutexLock *lock) {
    futexlock_write_read_lock(lock);
}

void futexlock_read_unlock(FutexLock *lock) {
    futexlock_write_read_unlock(lock);
}


static inline
bool futexlock_is_locked(FutexLock *lock){
    return flock_is_locked(lock);
}

static inline
bool futexlock_try_write_read_lock(FutexLock *lock) {
    return flock_try_lock_wait(lock, 0); //TODO 0 == retries
}

//Multi write queue

#define MWQ_CAPACITY MAX_NUM_OF_HELPED_OPS

typedef struct DelegateRequestEntryImpl {
    void (*request)(int, int*);
    int data;
    int * responseLocation;
#ifdef PAD_QUEUE_ELEMENTS_TO_TWO_CACHE_LINES
    char pad[128 - ((2*sizeof(void *)) + sizeof(int))];
#endif
} DelegateRequestEntry;

typedef struct DRMWQImpl {
    char padd1[64];
    CacheLinePaddedBool closed;
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
int CAS_fetch_and_add(unsigned long * valueAddress, unsigned long incrementWith){
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
    queue->closed.value = true;
    __sync_synchronize();
    return queue;
}

void drmvqueue_free(DRMWQueue * queue){
    free(queue);
}

#define NEWOFFER
#ifdef NEWOFFER

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
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
    }else{
        return false;
    }
}

#else

static inline
bool drmvqueue_offer(DRMWQueue * queue, DelegateRequestEntry e){
    bool closed;
    load_acq(closed, queue->closed.value);
    if(!closed){
        int index = __sync_fetch_and_add(&queue->elementCount.value, 1);
        if(index < MWQ_CAPACITY){
            store_rel(queue->elements[index].responseLocation, e.responseLocation);
            store_rel(queue->elements[index].data, e.data);
            store_rel(queue->elements[index].request, e.request);
            __sync_synchronize();//Flush
            return true;
        }else{
            store_rel(queue->closed.value, true);
            __sync_synchronize();//Flush
            return false;
        }
    }else{
        return false;
    }
}

#endif


static inline
void drmvqueue_flush(DRMWQueue * queue){
    unsigned long numOfElementsToRead;
    unsigned long newNumOfElementsToRead;
    unsigned long currentElementIndex = 0;
    bool closed = false;
    load_acq(numOfElementsToRead, queue->elementCount.value);
    if(numOfElementsToRead >= MWQ_CAPACITY){
#ifdef NEWOFFER
        store_rel(queue->closed.value, true);
#endif
        closed = true;
        numOfElementsToRead = MWQ_CAPACITY;
    }

    while(true){
        if(currentElementIndex < numOfElementsToRead){
            //There is definitly an element that we should read
            DelegateRequestEntry e;
            load_acq(e.request, queue->elements[currentElementIndex].request);
            load_acq(e.data, queue->elements[currentElementIndex].data);
            load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            while(e.request == NULL) {
                __sync_synchronize();
                load_acq(e.request, queue->elements[currentElementIndex].request);
                load_acq(e.data, queue->elements[currentElementIndex].data);
                load_acq(e.responseLocation, queue->elements[currentElementIndex].responseLocation);
            }
            e.request(e.data, e.responseLocation);
            store_rel(queue->elements[currentElementIndex].request, NULL);
            currentElementIndex = currentElementIndex + 1;
        }else if (closed){
#ifdef QUEUE_STATS
            helpSeasonsPerformed.value++;
            numberOfDeques.value = numberOfDeques.value + currentElementIndex;
#endif
            //The queue is closed and there is no more elements that need to be read:
            return;
        }else{
            //Seems like there are no elements that should be read and the queue is
            //not closed. Check again if there are still no more elements that should
            //be read before closing the queue
#ifdef WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT
            for(int i = 0; i < WAITS_BEFORE_CLOSE_QUEUE_ATTEMPT; i++){
                __sync_synchronize();                
            }
#endif
            load_acq(newNumOfElementsToRead, queue->elementCount.value);
            if(newNumOfElementsToRead == numOfElementsToRead){
                //numOfElementsToRead has not changed. Close the queue.
                numOfElementsToRead = 
                    min(get_and_set_ulong(&queue->elementCount.value, MWQ_CAPACITY + 1), 
                        MWQ_CAPACITY);
#ifdef NEWOFFER
                store_rel(queue->closed.value, true);
#endif
		closed = true;
            }else if(newNumOfElementsToRead < MWQ_CAPACITY){
                numOfElementsToRead = newNumOfElementsToRead;
            }else{
#ifdef NEWOFFER
                store_rel(queue->closed.value, true);
#endif
                closed = true;
                numOfElementsToRead = MWQ_CAPACITY;
            }
        }
    }
}

static inline
void drmvqueue_reset_fully_read(DRMWQueue * queue){
    store_rel(queue->elementCount.value, 0);
    store_rel(queue->closed.value, false);
}



//QD Lock


typedef struct AgnosticDXLockImpl {
    DRMWQueue writeQueue;
    char pad1[128];
    void (*defaultWriter)(int, int *);
    char pad2[64 - sizeof(void * (*)(void*)) % 64];
    char pad3[128];
    FutexLock lock;
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
    futexlock_initialize(&lock->lock, defaultWriter);
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
    int retries;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    /* for guaranteed starvation freedom add a limit here */
    for(retries = 1; /* no limit */; retries++) {
        if(flock_try_lock_wait(&lock->lock, retries)) {
            drmvqueue_reset_fully_read(&lock->writeQueue);
            flock_notify_all(&lock->lock);
            delgateFun(data, responseLocation);
            drmvqueue_flush(&lock->writeQueue);
            futexlock_write_read_unlock(&lock->lock);
            return;
        }
        /* retry enqueueing a couple of times if CLOSED
         * TODO: differentiate CLOSED and FULL states?
         * TODO: magic number */
        for(int i = 1; i <= MAGIC_NUMBER_ENQUEUE_TRIES; i++) {
            if(drmvqueue_offer(&lock->writeQueue, e)){
                return;
            }
	    asm("pause");
            //__sync_synchronize();
            //__sync_synchronize();
        }
    }
    /* TODO: if starvation freedom is desired, insert code here */
#if 0
    int counter = 0;
    DelegateRequestEntry e;
    e.request = delgateFun;
    e.data = data;
    e.responseLocation = responseLocation;
    do{
        if(!futexlock_is_locked(&lock->lock)){
            if(futexlock_try_write_read_lock(&lock->lock)){
#ifdef ACTIVATE_NO_CONTENTION_OPT
	        if(counter > 0){
#endif
                    drmvqueue_reset_fully_read(&lock->writeQueue);
                    delgateFun(data, responseLocation);
                    drmvqueue_flush(&lock->writeQueue);
                    futexlock_write_read_unlock(&lock->lock);
                    return;
#ifdef ACTIVATE_NO_CONTENTION_OPT
	        }else{
                    delgateFun(data, responseLocation);
		    futexlock_write_read_unlock(&lock->lock);
		    return;
                }
#endif
            }
        }else{
            while(futexlock_is_locked(&lock->lock)){
                if(drmvqueue_offer(&lock->writeQueue, e)){
                    return;
                }else{
                    __sync_synchronize();
                    __sync_synchronize();
                }
            }
        }
        if((counter & 7) == 0){
#ifdef USE_YIELD
            sched_yield();
#endif
        }
        counter = counter + 1;
    }while(true);
#endif
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
    futexlock_write_read_lock(&lock->lock);    
    drmvqueue_reset_fully_read(&lock->writeQueue);
    __sync_synchronize();//Flush
}

void adxlock_write_read_unlock(AgnosticDXLock * lock) {
    drmvqueue_flush(&lock->writeQueue);
    futexlock_write_read_unlock(&lock->lock);
}

void adxlock_read_lock(AgnosticDXLock *lock) {
    adxlock_write_read_lock(lock);
}

void adxlock_read_unlock(AgnosticDXLock *lock) {
    adxlock_write_read_unlock(lock);
}



#endif
