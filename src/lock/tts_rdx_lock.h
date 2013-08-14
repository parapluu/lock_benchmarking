#include <stdbool.h>
#include "datastructures/opti_multi_writers_queue.h"
#include "common_lock_constants.h"
#include "utils/support_many_non_zero_indicator_types.h"

#ifndef TTS_RDX_LOCK_H
#define TTS_RDX_LOCK_H


typedef struct TTSRDXLockImpl {
    OptiMWQueue writeQueue;
    char pad1[64];
    void (*writer)(void *, void **);
    char pad2[64 - sizeof(void (*)(void*)) % 64];
    char pad3[64];
    CacheLinePaddedInt writeBarrier;
    CacheLinePaddedBool lockWord;
    char pad4[64];
    NZI_DATATYPE_NAME nonZeroIndicator;
} TTSRDXLock;



TTSRDXLock * ttsalock_create(void (*writer)(void *, void **));
void ttsalock_free(TTSRDXLock * lock);
void ttsalock_initialize(TTSRDXLock * lock, void (*writer)(void *, void **));
void ttsalock_register_this_thread();
void ttsalock_write(TTSRDXLock *lock, void * writeInfo);
void ttsalock_write_read_lock(TTSRDXLock *lock);
void ttsalock_write_read_unlock(TTSRDXLock * lock);
void ttsalock_read_lock(TTSRDXLock *lock);
void ttsalock_read_unlock(TTSRDXLock *lock);

#endif
