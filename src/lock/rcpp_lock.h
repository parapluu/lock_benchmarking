#ifndef RCPPLOCK_H
#define RCPPLOCK_H

//RHQDLock

typedef struct RCPPLockImpl {
    void (*defaultWriter)(void*, void **);
    char pad2[64 - (sizeof(void * (*)(void*)) % 64)];
    char lock[256*1024*1024];
} RCPPLock;

RCPPLock* rcpplock_new();
void rcpplock_free(RCPPLock*);
void rcpplock_init(RCPPLock*);
void rcpplock_delegate(RCPPLock* lock, void (*delgateFun)(void *, void **), void * data);
static void* rcpplock_delegate_and_wait (RCPPLock* lock, void (*delgateFun)(void *, void **), void * data);
void rcpplock_lock(RCPPLock*);
void rcpplock_unlock(RCPPLock*);
void rcpplock_rlock(RCPPLock*);
void rcpplock_runlock(RCPPLock*);

static inline void rcpplock_initialize(RCPPLock * lock, void (*defaultWriter)(void *, void **)) {
    lock->defaultWriter = defaultWriter;
    rcpplock_init(lock);
    __sync_synchronize();
}
static inline RCPPLock * rcpplock_create(void (*writer)(void *, void **)) {
	(void)writer;
	RCPPLock* lock = rcpplock_new();
	return lock;
}

static inline void rcpplock_register_this_thread() {}

//static void rcpplock_write_with_response(RCPPLock *rcpplock, 
//                                  void (*delgateFun)(void *, void **), 
//                                  void * data, 
//                                  void ** responseLocation);
static void * rcpplock_write_with_response_block(RCPPLock *lock, 
                                          void (*delgateFun)(void *, void **), 
                                          void * data);
void rcpplock_delegate(RCPPLock *lock, 
                       void (*delgateFun)(void *, void **), 
                       void * data);
static inline void rcpplock_write(RCPPLock *lock, void * writeInfo) {
	rcpplock_delegate(lock, lock->defaultWriter, writeInfo);
}

static inline void rcpplock_write_read_lock(RCPPLock *lock) {
	rcpplock_lock(lock);
}
static inline void rcpplock_write_read_unlock(RCPPLock * lock) {
	rcpplock_unlock(lock);
}
static inline void rcpplock_read_lock(RCPPLock *lock) {
	rcpplock_rlock(lock);
}
static inline void rcpplock_read_unlock(RCPPLock *lock) {
	rcpplock_runlock(lock);
}


#if 0
static inline
void rcpplock_write_with_response(RHQDLock *rcpplock, 
                                 void (*delgateFun)(void *, void **), 
                                 void * data, 
                                 void ** responseLocation){
}
static inline

void rcpplock_delegate(RHQDLock *lock, 
                      void (*delgateFun)(void *, void **), 
                      void * data) {
    rcpplock_write_with_response(lock, delgateFun, data, NULL);
#endif

static inline
void * rcpplock_write_with_response_block(RCPPLock *lock, 
                                      void (*delgateFun)(void *, void **), 
                                      void * data){
	return rcpplock_delegate_and_wait(lock, delgateFun, data);
}




#endif
