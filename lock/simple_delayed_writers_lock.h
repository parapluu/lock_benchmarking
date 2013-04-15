#include <stdbool.h>

#ifndef SIMPLE_DELAYED_WRITERS_LOCK_H
#define SIMPLE_DELAYED_WRITERS_LOCK_H



typedef struct SimpleDelayedWritesLockImpl SimpleDelayedWritesLock;

SimpleDelayedWritesLock * sdwlock_create(void (*writer)(void *));
void sdwlock_free(SimpleDelayedWritesLock * lock);
void sdwlock_register_this_thread();
void sdwlock_write(SimpleDelayedWritesLock *lock, void * writeInfo);
void sdwlock_write_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_write_read_unlock(SimpleDelayedWritesLock * lock);
void sdwlock_read_lock(SimpleDelayedWritesLock *lock);
void sdwlock_read_unlock(SimpleDelayedWritesLock *lock);

#endif
