/*
 * list.h
 *
 *  Created on: Apr 20, 2017
 *      Author: Paul
 */

#ifndef XV6CPP_LIST_H_
#define XV6CPP_LIST_H_


#include <functional>
#include <sys\queue.h>
#include <memory>

#include "sem.h"

	// helper templates for sys\queue and c++, need to change the macros up
	template<typename T>
	struct slist_head  {
		using type = T;
		type *slh_first;	/* first element */
		slist_head() : slh_first(nullptr) {}
		bool empty() const { return slh_first == nullptr; }
	};
	template<typename T>
	struct slist_entry  {
		using type = T;
		type *sle_next;	/* next element */
		slist_entry() : sle_next(nullptr) {}
	};
	template<typename T>
	struct stailq_entry  {
		using type = T;
		type *stqe_next;	/* next element */
		stailq_entry() : stqe_next(nullptr) {}
	};
	template<typename T>
	struct stailq_head  {
		using type = T;
		type *stqh_first;	/* first element */			\
		type **stqh_last;	/* addr of last next element */
		stailq_head() : stqh_first(nullptr), stqh_last(&stqh_first) {}
		bool empty() const { return stqh_first == nullptr; }
	};
	template<typename T>
	struct list_head  {
		using type = T;
		type *lh_first;	/* first element */
		TRACEBUF
		list_head() : lh_first(nullptr) {}
	};
	template<typename T>
	struct list_entry  {
		using type = T;
		type *le_next;	/* next element */
		type **le_prev;	/* address of previous next element */
		list_entry() : le_next(nullptr), le_prev(&le_next) {}
		inline bool invalid() const { return le_prev == &le_next && le_next == nullptr; }
		inline void reset() { le_prev = & le_next; le_next = nullptr; }
	};
	template<typename T>
	struct tailq_head  {
		using type = T;
		type *tqh_first;	/* first element */
		type **tqh_last;	/* addr of last next element */
		tailq_head() : tqh_first(nullptr), tqh_last(&tqh_first) {}
		bool empty() const { return tqh_first == nullptr; }
		TRACEBUF
	};
	template<typename T>
	struct tailq_entry  {
		using type = T;
		type *tqe_next;	/* next element */
		type **tqe_prev;	/* address of previous next element */
		tailq_entry() : tqe_next(nullptr), tqe_prev(&tqe_next) {}
		bool linked() const { return &tqe_next == tqe_prev && tqe_next == nullptr; }
		void unlink() { tqe_next = nullptr;  tqe_prev = &tqe_next; }
		TRACEBUF
	};

namespace xv6 {

	template<typename T> class hash_entry;
	template<typename T, size_t _COUNT> class hash_head;


	template<typename T, size_t N>
	list_head<T>* hash_bucket(list_head<T>(*buckets)[N], size_t h){
		return &buckets[h % N];
	}
	template<typename T, typename HASHF, typename EQUALF,size_t N,typename...Args>
	T* hash_search(list_head<T>(*buckets)[N], Args...args){
		HASHF hashf;
		EQUALF equalf;
		size_t h = hashf(std::forward<Args>(args)...);
		auto bucket = hash_bucket(buckets,h);
		T* r;
		LIST_FOREACH(r, bucket,_hash)
			if(equalf(*r,std::forward<Args>(args)...)) return r;
		return nullptr;
	}
	template<typename T, typename HASHF, typename EQUALF,size_t N>
	bool hash_insert(list_head<T>(*buckets)[N], T* n){
		HASHF hashf;
		EQUALF equalf;
		size_t h = hashf(n);
		auto bucket = hash_bucket(buckets,h);
		T* r;
		LIST_FOREACH(r, bucket,_hash)
			if(equalf(*r,*n)) return false;
		LIST_INSERT_HEAD(bucket, n, _hash);
		return true;
	}
	template<typename T, typename HASHF, typename EQUALF,size_t N>
	bool hash_remove(list_head<T>(*buckets)[N], T* n){
		LIST_REMOVE(n, _hash);
		return true;
	}


#if 0
	// union of tailq, you know, the way this is set up we could have one elemet
	// hanlde it all

	// so... simple object ownership here
	// if _owner == object, then the object is in a list
	// this is done becauese we already have 3 pointers in this
	// anyway.  otherwise I will have to get rid of the union
	// we will see if I have to do this latter
	template<typename T>
	class object {
		class iterator {
			iterator(tailq_object* c) : _current (c) {}
			iterator operator++ () {
				if(_current) _current = _current->tqe_next;
				return iterator(_current);
			}
			iterator operator++ (int) {
				iterator tmp(_current);
				if(_current) _current =_current->tqe_next;
				return tmp;
			}
			iterator operator-- () {
				if(_current) _current = *_current->tqe_prev;
				return iterator(_current);
			}
			iterator operator-- (int) {
				iterator tmp(_current);
				if(_current) _current = *_current->tqe_prev;
				return tmp;
			}
			operator tailq_object*() { return _current; }
			bool operator==(const iterator& r) const { return _current == r._current; }
			bool operator!=(const iterator& r) const { return _current != r._current; }
		private:
			tailq_object* _current;
		};
		iterator begin() {
			assert(_owner == nullptr);
			return iterator(tqh_first);
		}
		iterator end() {
			assert(_owner == nullptr);
			return iterator(*tqh_last);
		}
		tailq_object* owner() const { return _owner; }

		void insert_head(tailq_object* o){
			assert(o && _owner == nullptr);
			if(o->_owner) o->remove();
			if((o->tqe_next = tqh_first)  != nullptr)
				tqh_first->tqe_prev = &o->tqe_next;
			else
				tqh_last = &o->tqe_next;
			tqh_first = o;
			o->tqe_prev = &tqh_first;
			o->_owner = this;
		}
		void insert_tail(tailq_object* o){
			assert(o && _owner == nullptr);
			if(o->_owner) o->remove();
			o->tqe_next = nullptr;
			o->tqe_prev = tqh_last;
			*tqh_last = o;
			tqh_last = &o->tqe_next;
			o->_owner = this;
		}
		void remove() {
			if(_owner) {
				if(tqe_next != nullptr)
					tqe_next->tqe_prev = tqe_prev;
				else
					_owner->tqh_last = tqe_prev;
				*tqe_prev = tqe_next;
				_owner = nullptr;
			}
		}
		void add_refrence(object* o) {

		}
		object() : _owner(nullptr), tqh_first(nullptr), tqh_last(&tqh_first) {}
		~object() {
			if(_owner == nullptr) {
				for(auto o: *this) o.remove();
			} else remove();
		}
	private:
		object* _parent;
		int _ref; // number of owners
		time_t _atime; // moified time, for tracking
		time_t _ctime; // created time
		tailq_entry<object> _children;
		tailq_entry<object> _freelist; // chain of free objects
		object *_next_peer;	/* next element */
		object *_prev_peer;	/* address of previous next element */
	};

#endif
#if 0
	// HASH_FUNC is size_t hash(args...) and equal is bool(const T*, args...)
	template<typename T, typename EQUALF, typename HASHF,  size_t STATIC_CACHE_SIZE=64>
	class static_cache {
	public:
		constexpr static size_t BUCKETS_SHIFT = 2;
		constexpr static size_t CACHE_SIZE = STATIC_CACHE_SIZE;
		constexpr static size_t BUCKETS = STATIC_CACHE_SIZE >> BUCKETS_SHIFT;
		//static_assert(BUCKETS%2 == 0, "Needs to be divisiable by 2");
		using type = T;
		//using hash_entry = hash_head::list_entry;
		enum obj_state {
			C_FREE = 0,
			C_ALLOC = 1<<0,
			C_LOCKED = 1<<1,
			C_WANTED = 1<<2,
		};
		struct hash_entry : public T {
			int ref;
			enum_helper<obj_state> state;
			tailq_entry<hash_entry> free; // on free list
			list_entry<hash_entry> hash; // hash list
		};
		using hash_head = list_head<hash_entry>;

		static_cache() : u(nullptr) ,_prio(0),_want_freelist(false) {
			for(size_t i=0; i < STATIC_CACHE_SIZE; i++){
				hash_entry* e = &_entrys[i];
				e->ref = 0;
				e->state = C_FREE;
				TAILQ_INSERT_TAIL(&_freelist, e, free);
			}
		}
		void setup_cache(user& u, int prio){
			u = &u;
			_prio = prio;
		}
		// pushes the entry to the back because its being returned
		int inc_ref(hash_entry* e){
			int r = e->ref;
			++e->ref;
			TAILQ_REMOVE(&_freelist,e,free);
			TAILQ_INSERT_TAIL(&_freelist, e, free); // reinsert into back cause its being used
			return r;
		}
		int dec_ref(hash_entry* e){
			int r = e->ref;
			--e->ref;
			TAILQ_REMOVE(&_freelist,e,free);
			TAILQ_INSERT_TAIL(&_freelist, e, free); // reinsert into back cause its being used
			return r;
		}
		template<typename... Args>
		T* aquire(Args&&... args){
			//if(u) irq_lock lck(*u,_prio);
			do {
				HASHF hashf;
				EQUALF equalsf;
				size_t h = hashf(std::forward<Args>(args)...);
				hash_head* bucket = &_buckets[h % BUCKETS];
				hash_entry* e;
				LIST_FOREACH(e,bucket,hash) {
					if(equalsf(*e,std::forward<Args>(args)...)) {
						while(e->state == C_LOCKED) {
							e->state |= C_WANTED;
							if(u) u->sleep(&e,_prio);
						}
						e->state |= C_LOCKED;
						inc_ref(e);
						return e;
					}
				}
	#if 0
				for(hash_entry& e : bucket) {
					T* r = static_cast<T*>(&e);
					if(equalsf(*r,std::forward<Args>(args)...)) {
						e.inc_ref();
						if(e.state & C_LOCKED){// need to make sure its not locked
							if(!u) return nullptr;
							while(e.state & C_LOCKED) {
								e.state |= C_WANTED;
								u->sleep(&e,_prio);
							}
							e.state &= ~C_WANTED;
						}
						return static_cast<T*>(&e);
					}
				}
	#endif
				if(TAILQ_EMPTY(&_freelist)) {
					_want_freelist = true;
					if(u) u->sleep(&_freelist,_prio);
					continue;
				}
				_want_freelist = false;
				e = TAILQ_FIRST(&_freelist);
				inc_ref(e);
				LIST_REMOVE(e,hash);
				LIST_INSERT_HEAD(bucket, e, hash); // reinsert into back cause its being used
				e->state |= C_ALLOC;
				return e;
			} while(1);
		}
		void release(T* o) {
			//if(u) irq_lock lck(*u,_prio);
			hash_entry *e = static_cast<hash_entry*>(o);
			while(!lock(o));
			if(dec_ref(e) <= 0) e->state &= ~C_ALLOC; // should be free
			unlock(o);
		}
		bool lock(T* o) {
			hash_entry *e = static_cast<hash_entry*>(o);
			if(e->state == C_LOCKED){
				if(!u) return false;
				while(e->state == C_LOCKED) {
					e->state |= C_WANTED;
					if(u) u->sleep(o,_prio);
				}
				e->state &= ~C_WANTED;
			}
			e->state |= C_LOCKED;
			return true;
		}
		void unlock(T* o) {
			hash_entry *e = static_cast<hash_entry*>(o);
			if(e->state & C_LOCKED) {
				e->state &= ~C_LOCKED;
				if(u && (e->state == C_WANTED)) u->wakeup(o);
			}
		}
		T& operator[](size_t i) { return _entrys[i]; }
		T& begin() { return _entrys[0]; }
		T& end() { return _entrys[STATIC_CACHE_SIZE]; }
	protected:
		hash_entry _entrys[STATIC_CACHE_SIZE];
		tailq_head<hash_entry> _freelist;
		hash_head _buckets[BUCKETS];
		user*u;
		int _prio;
		bool _want_freelist;
	};

#if 0
// generic link list illterator
// requires next() for forward and prev() for backward
template<typename T, bool reverse, bool is_const>
class generic_link_iterator {
public:
	using type = T;
#if 0
	// redundent
	using type = typename std::conditional<is_const,
			const T,
			T>::type;
#endif
	using type_pointer = typename std::conditional<is_const,
			const type*,
			type*>::type;
	generic_link_iterator(type_pointer start=nullptr) : _current(start) {}
	operator type_pointer() { return _current; }
	operator typename std::enable_if<(!is_const)>::type() const { return _current; }
	// overloaded prefix ++ operator
	generic_link_iterator operator++ () {
		if(_current != nullptr) _current = reverse ? _current->prev() : _current->next();
		return entry_iterator(_current);
	}
	generic_link_iterator operator++ (int) {
		generic_link_iterator t(_current);
		if(_current != nullptr) _current = reverse ? _current->prev() : _current->next();
		return t;
	}
	generic_link_iterator operator-- () {
		if(_current != nullptr) _current = reverse ? _current->next() : _current->prev();
		return entry_iterator(_current);
	}
	generic_link_iterator operator-- (int) {
		generic_link_iterator t(_current);
		if(_current != nullptr) _current = reverse ? _current->next() : _current->prev();
		return t;
	}
	bool operator==(const generic_link_iterator& r) const { return _current == r._current; }
	bool operator!=(const generic_link_iterator& r) const { return _current != r._current; }
private:
	type_pointer _current;
};
#if 0
template<typename T> class list_entry;
template<typename T> class list_head;
template<typename T>
struct  list_traits {
	using type =  T;
	using refrence =  T&;
	using const_refrence =  const  T&;
	using pointer =  T*;
	using const_pointer =  const  T*;
	using entry =  list_entry<T>;
	using const_entry =  const list_entry<T>;
	using entry_pointer = entry*;
	using const_entry_pointer = const entry*;
};
#endif
class list_entry {
public:
	using refrence =  list_entry&;
	using const_refrence =  const  list_entry&;
	using pointer =  list_entry*;
	using const_pointer =  const list_entry*;
	using iterator = generic_link_iterator<list_entry,false,false>;
	using const_iterator = generic_link_iterator<list_entry,false,true>;
	using riterator = generic_link_iterator<list_entry,true,false>;
	using const_riterator = generic_link_iterator<list_entry,true,true>;


	void insert_before(pointer elm) {
		elm->le_prev = le_prev;
		elm->le_next = this;
		*le_prev = elm;
		le_prev = &elm->le_next;
	}
	void insert_after(pointer elm) {
		if((elm->le_next = le_next)!=nullptr)
			le_next->le_prev = & elm->next;
		le_next = elm;
		elm->le_prev = &le_next;
	}
	pointer next() { return le_next; }
	pointer prev() { return *le_prev; }
	void remove() {
		if(le_next != nullptr)
			le_next->le_prev = le_prev;
		*le_prev = le_next;
	}
	iterator begin() { return iterator(this); }
	iterator end() { return iterator(nullptr); }
	const_iterator begin() const { return const_iterator(this); }
	const_iterator end() const { return const_iterator(nullptr); }
	riterator rbegin() { return riterator(this); }
	riterator rend() { return riterator(nullptr); }
	const_riterator rbegin() const { return const_riterator(this); }
	const_riterator rend() const { return const_riterator(nullptr); }
	list_entry() :  le_next(nullptr), le_prev(&le_next)  {}
protected:
	pointer le_next;	/* next element */
	pointer *le_prev;	/* address of previous next element */
	friend class list_head;
};
class list_head {
public:
	using entry =  list_entry;
	using const_entry =  const list_entry;
	using entry_pointer = list_entry*;
	using const_entry_pointer = const list_entry*;
	using iterator = generic_link_iterator<entry,false,false>;
	using const_iterator = generic_link_iterator<entry,false,true>;
	using riterator = generic_link_iterator<entry,true,false>;
	using const_riterator = generic_link_iterator<entry,true,true>;
	list_head() : lh_first(nullptr) {}
	entry_pointer first() { return lh_first; }
	void insert_head(entry_pointer elm) {
		if((elm->le_next = lh_first) != nullptr)
			lh_first->le_prev = &elm->le_next;
		lh_first = elm;
		elm->le_prev = &lh_first;
	}
	iterator begin() { return iterator(lh_first); }
	iterator end() { return iterator(nullptr); }
	const_iterator begin() const { return const_iterator(lh_first); }
	const_iterator end() const { return const_iterator(nullptr); }
protected:
	entry_pointer lh_first;
	friend class list_entry;
};
#endif
#endif
} /* namespace xv6 */

#endif /* XV6CPP_LIST_H_ */
