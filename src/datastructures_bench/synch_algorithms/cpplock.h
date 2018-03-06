#ifndef CPPLOCK_H
#define CPPLOCK_H

typedef struct AgnosticDXLockImpl {
    void (*defaultWriter)(int, int *);
    char pad2[64 - (sizeof(void * (*)(void*)) % 64)];
    char lock[256*1024*1024];
} AgnosticDXLock;

AgnosticDXLock* cpplock_new();
void cpplock_free(AgnosticDXLock*);
void cpplock_init(AgnosticDXLock*);
void cpplock_delegate(AgnosticDXLock* lock, void (*delgateFun)(int, int *), int data);
int cpplock_delegate_and_wait (AgnosticDXLock* lock, void (*delgateFun)(int, int *), int data);
void cpplock_lock(AgnosticDXLock*);
void cpplock_unlock(AgnosticDXLock*);
void cpplock_rlock(AgnosticDXLock*);
void cpplock_runlock(AgnosticDXLock*);

static void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *));
static inline AgnosticDXLock * adxlock_create(void (*writer)(int, int *)){
    AgnosticDXLock * lock = cpplock_new();
    return lock;
}

static inline void adxlock_initialize(AgnosticDXLock * lock, void (*defaultWriter)(int, int *)){
    //TODO check if the following typecast is fine
    lock->defaultWriter = defaultWriter;
    cpplock_init(lock);
    __sync_synchronize();
}

static inline void adxlock_free(AgnosticDXLock * lock){
    cpplock_free(lock);
}

static inline void adxlock_register_this_thread(){
}

//int delegate_cpp(void (*delgateFun)(int, int *), int data, int* resp) {
//	int response = cpplock_delegate_wrapper(delegateFun);



static inline
int adxlock_write_with_response_block(AgnosticDXLock *lock, 
                                      void (*delgateFun)(int, int *), 
                                      int data){
    return cpplock_delegate_and_wait(lock, delgateFun, data);
}
static inline
void adxlock_delegate(AgnosticDXLock *lock, 
                      void (*delgateFun)(int, int *), 
                      int data) {
    cpplock_delegate(lock, delgateFun, data);
}

static inline
void adxlock_write(AgnosticDXLock *lock, int writeInfo) {
    adxlock_delegate(lock, lock->defaultWriter, writeInfo);
}

static inline
void adxlock_write_read_lock(AgnosticDXLock *lock) {
    cpplock_lock(lock);
}

static inline
void adxlock_write_read_unlock(AgnosticDXLock * lock) {
    cpplock_unlock(lock);
}

//void adxlock_read_lock(AgnosticDXLock *lock) {
//    cpplock_rlock(lock);
//}

//void adxlock_read_unlock(AgnosticDXLock *lock) {
//    cpplock_runlock(lock);
//}



#endif
