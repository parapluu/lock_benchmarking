#include "rdx.hpp"
#include "cpprdx.h"

CPPRDXLock* cpprdx_create(void (*writer)(void *)) {
	auto lock = new CPPRDXLock;
	lock->writer = writer;
	return lock;
}

void cpprdx_free(CPPRDXLock* lock) {
	delete lock;
}

void cpprdx_initialize(CPPRDXLock* lock, void (*writer)(void *)) {
	new (&lock->lock) RDX_Lock;
	lock->writer = writer;
}

void cpprdx_register_this_thread() {
	// NOP
}

void cpprdx_write(CPPRDXLock* lock, void* writeInfo) {
	void (*f)(void *) = lock->writer;
	lock->lock.lock_delegate(std::function<void()>( [f, writeInfo] () { (*f)(writeInfo); } ));
}

void cpprdx_write_read_lock(CPPRDXLock* lock) {
	lock->lock.lock_exclusive();
}

void cpprdx_write_read_unlock(CPPRDXLock* lock) {
	lock->lock.unlock_exclusive();
}

void cpprdx_read_lock(CPPRDXLock* lock) {
	lock->lock.lock_read();
}

void cpprdx_read_unlock(CPPRDXLock* lock) {
	lock->lock.unlock_read();
}

