RCPPLock* rcpplock_new() {
	RCPPLock* x = (RCPPLock*) std::malloc(sizeof(RCPPLock) + sizeof(locktype)-1+1024);
	new (&x->lock) locktype;
	return x;
}

void rcpplock_init(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	new (l) locktype;
}
void rcpplock_free(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->~locktype();
	std::free(x);
}
	
void rcpplock_delegate(RCPPLock* x, void (*delgateFun)(void*, void* *), void* data) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->delegate_n([](void (*fun)(void*, void* *), void* d) {fun(d, nullptr);}, delgateFun, data);
}
#if 1
void* rcpplock_delegate_and_wait(RCPPLock* x, void (*delgateFun)(void*, void* *), void* data) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	void* resp;
	std::atomic<bool> flag(false);
	l->delegate_n([](void (*fun)(void*, void* *), void* d , void** r, std::atomic<bool>* f) { fun(d, r);  f->store(true, std::memory_order_release); }, delgateFun, data, &resp, &flag);
	while(!flag.load(std::memory_order_acquire)) {
		qd::pause();
	}
	return resp;
}
#endif
#if 0
int rcpplock_delegate_and_wait(RCPPLock* x, void (*delgateFun)(int, int *), int data) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	static const int reserved = -999999;
	std::atomic<int> resp(reserved);
	l->delegate_n([](void (*fun)(int, int *), int d , std::atomic<int>* r) { int v = -1; fun(d, &v);  r->store(v, std::memory_order_release);}, delgateFun, data, &resp);
	while(resp.load(std::memory_order_acquire) == reserved) {
		qd::pause();
	}
	return resp;
}
#endif
void rcpplock_lock(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->lock();
}
void rcpplock_unlock(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->unlock();
}
void rcpplock_rlock(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->rlock();
}
void rcpplock_runlock(RCPPLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->runlock();
}
