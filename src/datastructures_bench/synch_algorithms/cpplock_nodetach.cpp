AgnosticDXLock* cpplock_new() {
	AgnosticDXLock* x = (AgnosticDXLock*) std::malloc(sizeof(AgnosticDXLock) + sizeof(locktype)-1+1024);
	new (&x->lock) locktype;
	return x;
}

void cpplock_init(AgnosticDXLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	new (l) locktype;
}
void cpplock_free(AgnosticDXLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->~locktype();
	std::free(x);
}
	
void cpplock_delegate(AgnosticDXLock* x, void (*delgateFun)(int, int *), int data) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	std::atomic<bool> flag(false);
	l->delegate_n([](void (*fun)(int, int *), int d , std::atomic<bool>* f) { fun(d, nullptr);  f->store(true, std::memory_order_release); }, delgateFun, data, &flag);
	while(!flag.load(std::memory_order_acquire)) {
		qd::pause();
	}
}
#if 1
int cpplock_delegate_and_wait(AgnosticDXLock* x, void (*delgateFun)(int, int *), int data) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	int resp;
	std::atomic<bool> flag(false);
	l->delegate_n([](void (*fun)(int, int *), int d , int* r, std::atomic<bool>* f) { fun(d, r);  f->store(true, std::memory_order_release); }, delgateFun, data, &resp, &flag);
	while(!flag.load(std::memory_order_acquire)) {
		qd::pause();
	}
	return resp;
}
#endif
#if 0
int cpplock_delegate_and_wait(AgnosticDXLock* x, void (*delgateFun)(int, int *), int data) {
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
void cpplock_lock(AgnosticDXLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->lock();
}
void cpplock_unlock(AgnosticDXLock* x) {
	locktype* l = reinterpret_cast<locktype*>(&x->lock);
	l->unlock();
}
//void cpplock_rlock(AgnosticDXLock* x) {
//	locktype* l = reinterpret_cast<locktype*>(&x->lock);
//	l->rlock();
//}
//void cpplock_runlock(AgnosticDXLock* x) {
//	locktype* l = reinterpret_cast<locktype*>(&x->lock);
//	l->runlock();
//}
