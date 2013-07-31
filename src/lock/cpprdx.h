#ifndef cpprdx_h
#define cpprdx_h cpprdx_h

#ifdef __cplusplus
#include "rdx.hpp"
extern "C" {
#endif

typedef struct CPPRDXLockImpl {
	void (*writer)(void *);
#ifdef __cplusplus
	RDX_Lock lock;
#else
	char lock[16000];
#endif
} CPPRDXLock;
CPPRDXLock* cpprdx_create(void (*writer)(void *));
void cpprdx_free(CPPRDXLock* lock);
void cpprdx_initialize(CPPRDXLock* lock, void (*writer)(void *));
void cpprdx_register_this_thread();
void cpprdx_write(CPPRDXLock* lock, void* writeInfo);
void cpprdx_write_read_lock(CPPRDXLock* lock);
void cpprdx_write_read_unlock(CPPRDXLock* lock);
void cpprdx_read_lock(CPPRDXLock* lock);
void cpprdx_read_unlock(CPPRDXLock* lock);

#ifdef __cplusplus
}
#endif

#endif
