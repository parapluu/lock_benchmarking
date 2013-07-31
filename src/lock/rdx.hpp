#ifndef rdx_hpp
#define rdx_hpp rdx_hpp
#include <functional>
#include <set>
#include <map>
#include <mutex>
#include <atomic>
#include <utility>
#include <type_traits>
#include <algorithm>

#include <iostream>

#include "padded.hpp"

template<unsigned int CAPACITY = 256>
class RDX_Queue {
	std::array<std::function<void()>*, CAPACITY> elements;
	padded<std::atomic<unsigned int>> current;
	public:
		RDX_Queue() : elements(), current(CAPACITY) {
			for(unsigned int i = 0; i < CAPACITY; i++) {
				elements[i] = nullptr;
			}
		}
		~RDX_Queue() {
		}
		bool push(std::function<void()>* f) {
			unsigned int index = current.fetch_add(1, std::memory_order_relaxed);
			if(index < CAPACITY) {
				elements[index] = f;
				return true;
			} else {
				return false;
			}
		}
		void open() {
			current.store(0, std::memory_order_relaxed);
			//TODO maybe do sth to flush store buffer
		}

		class iterator;
		iterator begin() { return iterator(this); }

		class iterator : public std::iterator<std::forward_iterator_tag, RDX_Queue<CAPACITY>> {
			RDX_Queue<CAPACITY>* base;
			unsigned int index;
			unsigned int max;
			bool closed;
			public:
			iterator(RDX_Queue<CAPACITY>* q)
				: base(q), index(0), max(std::min(q->current.load(std::memory_order_relaxed), CAPACITY)), closed(false) {
			}
			iterator& operator++() {
				++index;
				return *this;
			}
			bool operator==(const iterator& other) {
				return base == other.base && index == other.index;
			}
			bool operator!=(const iterator& other) {
				return base != other.base || index != other.index;
			}
			std::function<void()>* operator*() {
				std::function<void()>* result = base->elements[index];
				while(!result) { // wait for element to be set
					// TODO may need memory barrier here?
					asm volatile("pause");
					result = base->elements[index];
				}
				base->elements[index] = nullptr;
				return result;
			}

			bool more() {
				/* TODO possible micro-optimization for end case */
				if(index == max && !closed) {
					auto old = max;
					max = std::min(
						base->current.load(std::memory_order_relaxed),
						CAPACITY
					);
					if(old == max) {
						max = std::min(base->current.exchange(CAPACITY, std::memory_order_relaxed), CAPACITY);
						closed = true;
					}
				}
				return index != max;
			}
		};


			


};


class RDX_thread_id_store {
	static unsigned long max_id;
	static std::set<unsigned long> orphans;
	static std::mutex mutex;
	typedef std::lock_guard<std::mutex> scoped_lock;
	public:
		static unsigned long get() {
			scoped_lock lock(mutex);
			if(orphans.empty()) {
				max_id++;
				return max_id;
			} else {
				auto first = orphans.begin();
				auto result = *first;
				orphans.erase(first);
				return result;
			}
		}
		static void free(unsigned long idx) {
			scoped_lock lock(mutex);
			if(idx == max_id) {
				max_id--;
				while(orphans.erase(max_id)) {
					max_id--;
				}
			} else {
				orphans.insert(idx);
			}
		}
};
unsigned long RDX_thread_id_store::max_id = 0;
std::set<unsigned long> RDX_thread_id_store::orphans;
std::mutex RDX_thread_id_store::mutex;

class RDX_thread_id {
	unsigned long id;
	public:
		operator unsigned long() {
			return id;
		}
		RDX_thread_id() : id(RDX_thread_id_store::get()) {}
		~RDX_thread_id() {
			RDX_thread_id_store::free(id);
		}
};
thread_local RDX_thread_id thread_id;
class RDX_Node {
	static const unsigned int GROUPS = 64;
	public: 
	RDX_Queue<> delegated;
	std::array<volatile padded<std::atomic<unsigned int>>, GROUPS> reader_groups;
	padded<RDX_Node*> readlock;
	padded<std::atomic<bool>> locked; // TODO does not need to be atomic.
	padded<std::atomic<bool>> spin;
	padded<std::atomic<RDX_Node*>> next;
	
	public:
		RDX_Node() : delegated(), reader_groups(), locked(false), spin(false), next(nullptr) {
			for(unsigned int i = 0; i < GROUPS; i++) {
				reader_groups[i].store(0);
			}
		}
		template<typename F>
		bool delegate(F f);
		void delegation_enable() {
			delegated.open();
		}
		void set_next(RDX_Node* n) {
			next.store(n, std::memory_order_release); // TODO check that is needed together with locked flag
		}
		RDX_Node*  get_next() {
			return next.load(std::memory_order_acquire);
		}
		bool is_locked() {
			return locked.load(std::memory_order_acquire);
		}
		void lock() {
			locked.store(true, std::memory_order_release);
		}
		void unlock() {
			locked.store(false, std::memory_order_release);
		}
		bool spinning() {
			return spin.load(std::memory_order_seq_cst);
		}
		void spin_enable() {
			spin.store(true, std::memory_order_relaxed); // TODO seq_cst may be better
		}
		void spin_disable() {
			spin.store(false, std::memory_order_seq_cst);
			wait_for_readers(); // TODO can this be done by next instead?
		}
		RDX_Node* get_readlock() {
			return readlock;
		}
		void set_readlock(RDX_Node* r) {
			readlock = r;
		}
		void mark_reading() {
			reader_groups[thread_id % GROUPS].fetch_add(1, std::memory_order_relaxed);
		}
		void unmark_reading() {
			reader_groups[thread_id % GROUPS].fetch_sub(1, std::memory_order_relaxed);
		}
		void wait_for_readers() {
			for(unsigned int i = 0; i < GROUPS; i++) {
				while(reader_groups[i].load(std::memory_order_relaxed) > 0) {
					asm volatile("pause");
				}
			}
		}
		void perform_delegated() {
			for(auto i = delegated.begin(); i.more(); ++i) {
				std::function<void()>* d = *i;
				(*d)();
				delete d;
			}
		}
};
template<typename F>
bool RDX_Node::delegate(F f) {
	return delegate(std::function<void()>([f] () {f();}));
}
template<>
bool RDX_Node::delegate(std::function<void()> f) {
	return delegated.push(new std::function<void()>(f));
}



class RDX_Lock;
class RDX_Node_store {
	std::map<RDX_Lock*, RDX_Node*> nodes;
	public:
		RDX_Node_store() : nodes() {}
		~RDX_Node_store();

		RDX_Node* get(RDX_Lock* lock);
		void remove(RDX_Lock* lock) {
			auto node = nodes[lock]; 
			nodes.erase(lock);
			delete node;
		}
};

thread_local RDX_Node_store node_pool;

enum RDX_type {unlocked, R, D, X };
/**
 * Reader-Delegate-Exclusive locking device.
 */
class RDX_Lock {
	public:
		/**
		 * * Default constructor.
		 */
		explicit RDX_Lock() : sentinel(new RDX_Node), last(sentinel) {
			lock_exclusive();
			unlock_exclusive();
		}
		/**
		 * Destructor.
		 */
		~RDX_Lock() {
			lock_exclusive();
			unlock_exclusive();
			for(auto i = used_nodes.begin(); i != used_nodes.end(); i++) {
				(*i)->remove(this);
			}
			delete static_cast<RDX_Node*>(sentinel);
		}
		/**
		 * Get the exclusiver lock.
		 */
		void lock_exclusive() {
			RDX_Node* mynode = node_pool.get(this);
			lock_exclusive(mynode);
		}
		void lock_exclusive(RDX_Node* mynode) {
			RDX_Node* pred;
			mynode->set_next(sentinel);
			auto mysentinel = mynode;
			pred = last.exchange(mysentinel);
			mynode->spin_enable();
			mynode->delegation_enable();
			if(pred != sentinel) {
				mynode->lock();
				pred->set_next(mynode);
				while(mynode->is_locked()) {
					asm volatile ("pause");
				}
			} else {
				sentinel->wait_for_readers();
			}
		}
		bool trylock_exclusive() {
			lock_exclusive();
			return true;
		}

		/**
		 * Get a reader lock.
		 */
		void lock_read() {
			RDX_Node* pred;
			RDX_Node* mynode = node_pool.get(this);
			while(true) {
				pred = last.load(std::memory_order_acquire);
				if(pred == sentinel) {
					sentinel->mark_reading();
					pred = last.load(std::memory_order_acquire);
					if(pred != sentinel) {
						sentinel->unmark_reading();
					} else {
						mynode->set_readlock(sentinel);
						return;
					}
				} else {
					if(pred->spinning()) {
						pred->mark_reading();
						if(pred->spinning()) {
							mynode->set_readlock(pred);
							while(pred->spinning()) {
								asm volatile ("pause");
							}
							return;
						} else {
							pred->unmark_reading();
						}
					}
				}
				asm volatile ("pause");
			}
		}

		/**
		 * Release the lock.
		 */
		void unlock_exclusive()  {
			RDX_Node* mynode = node_pool.get(this);
			unlock_exclusive(mynode);
		}
		void unlock_exclusive(RDX_Node* mynode)  {
			mynode->perform_delegated();
			mynode->spin_disable();
			
			RDX_Node* next = mynode->get_next();
			RDX_Node* mysentinel = mynode;
			if(next == sentinel) {
				if(last.compare_exchange_strong(mysentinel, sentinel, std::memory_order_release)) {
					return;
				} else {
					next = mynode->get_next();
					while(next == sentinel) {
						asm volatile ("pause");
						next = mynode->get_next();
					}
				}
			}
			next->unlock();
		}


		void unlock_read() {
			RDX_Node* mynode = node_pool.get(this);
			mynode->get_readlock()->unmark_reading();
		}

		/**
		 * Do a delegatable operation..
		 */
		template<typename F>
		void lock_delegate(const F f) {
			while(true) {
				RDX_Node* current = last.load(std::memory_order_acquire);
				if(current == sentinel || !current->delegate(f)) {
					if(trylock_exclusive()) {
						f();
						unlock_exclusive();
						break;
					}
				} else {
					break;
				}
			}
		}

		template<typename R, typename... P>
		R lock_exclusive(const std::function<R(P...)> f, P... p) {
			lock_exclusive();
			R result = f(p...);
			unlock_exclusive();
			return result;
		}

		template<typename R, typename... P>
		R lock_read(const std::function<R(P...)> f, P... p) {
			lock_read();
			R result = f(p...);
			unlock_read();
			return result;
		}
		
	private:
		friend class RDX_Node_store;
		std::set<RDX_Node_store*> used_nodes;
		void associate(RDX_Node_store* store) {
			lock_exclusive();
			used_nodes.insert(store);
			unlock_exclusive();
		}
		void unassociate(RDX_Node_store* store, RDX_Node* node) {
			lock_exclusive(node);
			used_nodes.erase(store);
			unlock_exclusive(node);
		}

		/** Dummy constructor to forbid the use. */
		RDX_Lock(const RDX_Lock&);
		/** Dummy Operator to forbid the use. */
		RDX_Lock& operator =(const RDX_Lock&);

		padded<RDX_Node*> sentinel;
		padded<std::atomic<RDX_Node*>> last;
		
		
//		padded<std::atomic<unsigned int>> reader_groups[64];
		
};
RDX_Node_store::~RDX_Node_store() {
	for(auto i = nodes.begin(); i != nodes.end(); i++) {
		i->first->unassociate(this, i->second);
		delete i->second;
	}
}
RDX_Node* RDX_Node_store::get(RDX_Lock* lock) {
	if(nodes.count(lock)) {
		RDX_Node* node = nodes[lock];
		return node;
	} else {
		RDX_Node* node = new RDX_Node;
		nodes[lock] = node;
		lock->associate(this);
		return node;
	}
}
#endif
