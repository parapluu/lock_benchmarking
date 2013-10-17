#ifndef LOCKS_H
#define LOCKS_H

#include "locks/tatas_lock.h"
#include "locks/qd_lock.h"

typedef enum {TATAS_LOCK, QD_LOCK} LL_lock_type_name;

#define LL_initialize(X) _Generic((X),      \
     TATASLock * : tatas_initialize((TATASLock *)X), \
     QDLock * : qd_initialize((QDLock *)X)   \
                                )

void * LL_create(LL_lock_type_name llLockType){
    if(TATAS_LOCK == llLockType){
        TATASLock * l = aligned_alloc(CACHE_LINE_SIZE, sizeof(TATASLock));
        LL_initialize(l);
        return l;
    } else if (QD_LOCK == llLockType){
        QDLock * l = aligned_alloc(CACHE_LINE_SIZE, sizeof(QDLock));
        LL_initialize(l);
        return l;
    }
    return NULL;/* Should not be reachable */
}

#define LL_free(X) _Generic((X),\
     default : free(X) \
     )

#define LL_lock(X) _Generic((X),         \
     TATASLock *: tatas_lock((TATASLock *)X),     \
     QDLock * : tatas_lock(&((QDLock *)X)->mutexLock)  \
                              )

#define LL_unlock(X) _Generic((X),    \
    TATASLock *: tatas_unlock((TATASLock *)X), \
    QDLock * : tatas_unlock(&((QDLock *)X)->mutexLock) \
    )

#define LL_is_locked(X) _Generic((X),    \
    TATASLock *: tatas_is_locked((TATASLock *)X), \
    QDLock * : tatas_is_locked(&((QDLock *)X)->mutexLock) \
    )

#define LL_try_lock(X) _Generic((X),    \
    TATASLock *: tatas_try_lock(X), \
    QDLock * : tatas_try_lock(&((QDLock *)X)->mutexLock) \
    )

void ________TATAS_DELEGATE(TATASLock* l,
                            void (*funPtr)(unsigned int, void *), 
                            unsigned int messageSize,
                            void * messageAddress){
    tatas_lock(l);
    funPtr(messageSize, messageAddress);
    tatas_unlock(l);
}

#define LL_delegate(X, funPtr, messageSize, messageAddress) _Generic((X),      \
    TATASLock *: ________TATAS_DELEGATE((TATASLock *)X, funPtr, messageSize, messageAddress), \
    QDLock * : qd_delegate((QDLock *)X, funPtr, messageSize, messageAddress)  \
    )

#endif
