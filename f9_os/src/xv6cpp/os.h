/*
 * os.h
 *
 *  Created on: Apr 18, 2017
 *      Author: warlo
 */

#ifndef XV6CPP_OS_H_
#define XV6CPP_OS_H_

#include <cstdint>
#include <sys\queue.h>
#include <sys\types.h>
#include <algorithm>
//#include <macros.h>
#include <cassert>
#include "list.h"
#include "bitmap.h"

namespace xv6 {

#define ALIGNED(size, align) (size / align) + ((size & (align - 1)) != 0)
#define __ALLIGNED(X) __attribute__((aligned(X)))
#define __ALLIGNED32 __ALLIGNED(sizeof(uint32_t))
#define BIT(x) (1<<(x))
#define __NAKED __attribute__((naked))
using click_t = int32_t;

template<typename T, uintptr_t ALIGNBYTES = (sizeof(void*) -1)>
static constexpr T ALIGN(T p) { return reinterpret_cast<T>((reinterpret_cast<uintptr_t>(p) + ALIGNBYTES)  &~ ALIGNBYTES); }

	// constants when it comes to memory
// page constants
static constexpr size_t NBPG     =   4096;        /* bytes/page */
static constexpr size_t PGOFSET     =   (NBPG-1);    /* byte offset into page */
static constexpr size_t PGSHIFT     =   12;      /* LOG2(NBPG) */

// device and buffer constants
static constexpr size_t DEV_BSIZE       =512;
static constexpr size_t DEV_BSHIFT      =9  ;     /* log2(DEV_BSIZE) */
static constexpr size_t BLKDEV_IOSIZE   =2048;
static constexpr size_t MAXPHYS         =(64 * 1024); /* max raw I/O transfer size */
/* Core clicks (4096 bytes) to disk blocks */
static constexpr daddr_t 	ctod(click_t x) { return x<<(PGSHIFT-DEV_BSHIFT); }
static constexpr click_t 	dtoc(daddr_t x) { return x>>(PGSHIFT-DEV_BSHIFT); }
static constexpr uintptr_t 	dtob(daddr_t x) { return x<<DEV_BSHIFT; }
/* clicks to bytes */
static constexpr uintptr_t ctob(click_t x) { return x<<PGSHIFT; }

/* bytes to clicks */
static constexpr click_t btoc(uintptr_t x) { return (static_cast<uintptr_t>(x)+(NBPG-1))>>PGSHIFT; }

// simple lock
class simplelock {
	uint32_t _data;
	// this is mostly atomic eveywhere
	// though we assum all int assignments are
	uint32_t test_and_set() {
		uint32_t tmp = _data;
		_data = 1;
		return tmp;
	}
public:
	inline simplelock() : _data(0) {}
	inline void lock() { while(test_and_set()); }
	inline void unlock() { _data = 0; }
	inline bool lock_try() { return test_and_set()==0; }
};
// helper functions for enums

template<typename E>
class enum_helper {
	E _flag;
public:
	using type = enum_helper<E>;
	using U = typename std::underlying_type<E>::type;
	constexpr  enum_helper() : _flag(E{}) {}
	//constexpr flag(E flag) : _flag(flag) {}
	constexpr operator E() const { return _flag; }
	template<typename... Args>
	constexpr enum_helper(Args&&... args) : _flag(cast_to(combine_flags(std::forward<Args>(args)... ))){}
	template<typename... Args>
	void set(Args&&... args) { _flag = cast_to(cast_from(_flag) | combine_flags(std::forward<Args>(args)... ));  }
	template<typename... Args>
	void clear(Args&&... args) { _flag = cast_to(cast_from(_flag) & ~combine_flags(std::forward<Args>(args)... )); }
	template<typename... Args>
	constexpr bool contains(Args&&... args) const {
		return mask_flags(_flag,combine_flags(std::forward<Args>(args)...)) ==
				combine_flags(std::forward<Args>(args)...);
	}
	enum_helper& operator|=(const enum_helper& r) { _flag = cast_to(combine_flags(_flag,r._flag)); return *this; }
	enum_helper& operator&=(const enum_helper& r) { _flag = cast_to(mask_flags(_flag,r._flag)); return *this; }
	enum_helper& operator|=(const E r) { _flag = cast_to(combine_flags(_flag,r)); return *this; }
	enum_helper& operator&=(const E r) { _flag = cast_to(mask_flags(_flag,r)); return *this; }


	constexpr enum_helper operator~() const { return enum_helper(cast_to(~combine_flags(_flag))); }

private: // helpers
	constexpr static inline E cast_to() { return static_cast<E>(U{}); }
	constexpr static inline E cast_to(U&& val) { return static_cast<E>(val); }
	constexpr static inline U cast_from() { return static_cast<U>(E{}); }
	constexpr static inline U cast_from(E&& val) { return static_cast<U>(val); }
	constexpr static inline U cast_from(type&& val) { return static_cast<U>(val._flag); }
	//constexpr static inline U combine_flags() { return cast_to(); }
	constexpr static inline U combine_flags() { return cast_from(); }
	constexpr static inline U mask_flags() { return cast_from(); }
	template<typename Q, typename... Args>
	constexpr static inline U combine_flags(Q &&left,Args&&... args) {
		return cast_from(left) | combine_flags(std::forward<Args>(args)...);
	}
	template<typename Q, typename... Args>
	constexpr static inline U mask_flags(Q &&left,Args&&... args) {
		return cast_from(left) & mask_flags(std::forward<Args>(args)...);
	}
	template<typename Q, typename FT> friend constexpr bool operator==(const enum_helper<Q>& l, FT&&r);
	template<typename Q, typename FT> friend constexpr bool operator==(FT&&r, const enum_helper<Q>& l);

	template<typename Q, typename FT> friend constexpr bool operator!=(const enum_helper<Q>& l,FT&&r);
	template<typename Q, typename FT> friend constexpr bool operator!=( FT&&r,const enum_helper<Q>& l);

	template<typename Q, typename FT> friend constexpr enum_helper<Q> operator|(const enum_helper<Q>& l, FT&&r);
	template<typename Q, typename FT> friend constexpr enum_helper<Q> operator|( FT&&r,const enum_helper<Q>& l);

	template<typename Q, typename FT> friend constexpr enum_helper<Q> operator&(const enum_helper<Q>& l, FT&&r);
	template<typename Q, typename FT> friend constexpr enum_helper<Q> operator&(FT&&r,const enum_helper<Q>& l);

};
template<typename Q, typename FT> constexpr bool operator==(const enum_helper<Q>& l, FT&&r){ return l.contains(r); }
template<typename Q, typename FT> constexpr bool operator==(FT&&r, const enum_helper<Q>& l){ return l.contains(r); }

template<typename Q, typename FT> constexpr bool operator!=(const enum_helper<Q>& l, FT&&r){ return !l.contains(r); }
template<typename Q, typename FT> constexpr bool operator!=(FT&&r, const enum_helper<Q>& l){ return !l.contains(r); }

template<typename Q, typename FT> constexpr enum_helper<Q> operator|(const enum_helper<Q>& l, FT&&r)
	{ return enum_helper<Q>(enum_helper<Q>::cast_to(enum_helper<Q>::combine_flags(l,r))); }
template<typename Q, typename FT> constexpr enum_helper<Q> operator|( FT&&r,const enum_helper<Q>& l)
	{ return enum_helper<Q>(enum_helper<Q>::cast_to(enum_helper<Q>::combine_flags(l,r))); }

template<typename Q, typename FT> constexpr enum_helper<Q> operator&(const enum_helper<Q>& l, FT&&r)
	{ return enum_helper<Q>(enum_helper<Q>::cast_to(enum_helper<Q>::mask_flags(l,r))); }
template<typename Q, typename FT> constexpr enum_helper<Q> operator&( FT&&r,const enum_helper<Q>& l)
	{ return enum_helper<Q>(enum_helper<Q>::cast_to(enum_helper<Q>::mask_flags(l,r))); }


class buf; // forward
enum class irq_prio {
	Critical = 0,
	Clock = 1,
	Tty = 2,
	Bio = 3,
	Normal = 15,
};
class user {
public:
	void sleep(void* ptr,int priority);
	void wakeup(void* ptr);
	irq_prio spl(irq_prio prio);
};



class irq_lock {
	user& u;
	irq_prio pri;
public:
	irq_lock(user &u, irq_prio pri = irq_prio::Clock) :
		u(u), pri(u.spl(pri)) {}
	~irq_lock() { u.spl(pri); }
	irq_lock& operator=(irq_prio p) { u.spl(p); return *this; }
	bool operator==(const irq_lock& lck) { return pri == lck.pri; }
};
// this class lets you pre allocate a count number of items
// but be sure you use THIS class for new and delete then

class os {
public:
	os();
	virtual ~os();
};
extern void wakeup(void *);
extern void sleep(void*, irq_prio pri);
// eveything in the unix os seem to be able to be locked, wanted, and/or has a ref count
// so why not make a class that handles this all instead of
// writing code for eveything in there

#if 0
template<typename T, typename HASHF, typename EQUALF, size_t _COUNT, bool never_destroy=false>
class gcobject {
	using type = T;
	using gobject_t = gcobject<T,HASHF,EQUALF,_COUNT,never_destroy>;
	static constexpr size_t COUNT = _COUNT;
	using pointer_type = type*;
	using const_pointer_type = const pointer_type;
	using id_t = size_t;

	///using static_allocator_type = static_allocator<T>;
	bool locked() const { return _ref ? _ref->lock.locked() : false; }
	void lock() { if(_ref) _ref->lock.lock(); }
	void unlock() {if(_ref) _ref->lock.unlock(); }
	template<typename...Args>
	static T* aquire(Args...args) {
		HASHF hashf;
		EQUALF equalf;
		auto bucket = storage.getbucket(hashf(std::forward<Args>(args)...));
		T* r;
		LIST_FOREACH(r, bucket,hash) {
			if(equalf(*r,std::forward<Args>(args)...)) {
				if(++r->_ref == 1){
					auto id = storage.atable.getid(r);
					if(!never_destroy) {
						// run constructor in place
						storage.atable.create_id(id,std::forward<Args>(args)...);
					} else {
						storage.atable.alloc_id(id);// just tell the allocater its back
					}
				}
				return r;
			}
		}
		// dosn't exist so create a new one
		r = storage.atable.create(std::forward<Args>(args)...);
		if(r == nullptr) return nullptr;
		++r->_ref;
		return r;
	}
	static void release(T* obj) {
		ref_t* ref = static_cast<ref_t>(obj);
		assert(ref);
		obj->lock();
		if(--ref->ref == 0){
			if(!never_destroy) {
				ref->destroy();
			}
			storage.atable.free(ref);
		}
		obj->unlock(); // we can safely unlock here.  While the memory is unallocated it still exists
	}
	void attach(gcobject& obj) {
		assert(obj._ref && _ref && obj._ref != _ref); // valid objects
		lock();
		obj.lock(); // lock both objects
		ref_t* r = obj._ref;
		if(!r->link.invalid()) {
			LIST_REMOVE(&obj._ref, link);
			_ref->link.reset(); // make sure the link is clear
		} else obj._ref->ref++;
		LIST_INSERT_HEAD(&_ref->head, &obj._ref, link);
		unlock();
		obj.unlock(); // lock both objects
	}
	void detatch() {
		// posable raice condition here?
		assert(_ref); // valid object
		if(!_ref->link.invalid()) {
			LIST_REMOVE(_ref, link);
			_ref->link.reset(); // make sure the link is clear
			release(_ref);
		}
	}
	template<typename...Args>
	gcobject(Args...args) : _ref(aquire(std::forward<Args>(args)...)) {}
	gcobject() : _ref(nullptr) {}
	gcobject(const gcobject& copy) : _ref(copy._ref) { if(_ref) _ref->ref++; }
	gcobject& operator=(const gcobject& copy) { _ref=copy._ref; if(_ref) _ref->ref++; return *this; }
	gcobject(gcobject&& move) : _ref(move._ref) { move._ref = nullptr; }
	gcobject& operator=(gcobject&& move) { _ref=move._ref; move._ref = nullptr; return *this; }
	~gcobject() {
		if(_ref) {
			if(!LIST_EMPTY(&_ref->head)){
				ref_t* n, *p;
				LIST_FOREACH_SAFE(n,&_ref->head,link,p)n->detach();
			}
			release(_ref);
		}
	}
	T* operator->() { return _ref; }
	const T* operator->() const { return _ref; }
protected:
	struct ref_t : public T{
		int ref;
		simple_lock lock;
		tailq_entry<ref_t> hash; // simple hash table for object lookup
		// link list of ownership of objects?
		list_entry<ref_t> link;
		list_head<ref_t> link_head;
		template<typename...Args>
		ref_t(Args...args) : T(std::forward<Args>(args)...), ref(0) {}
		void destroy() { ~T(); }
	};
	ref_t* _ref;
	struct storage_t {
		using bucket_t = list_head<ref_t> ;
		constexpr static size_t BUCKETS_SHIFT = 2;
		constexpr static size_t BUCKET_SIZE = COUNT >> BUCKETS_SHIFT;
		//tailq_head<static_allocator> freelist; // list of allocated but free
		bucket_t buckets[BUCKET_SIZE];
		bitmap_table_t<ref_t,COUNT> atable;  // static table of objects
		bucket_t* getbucket(size_t hash) { return &buckets[hash % BUCKET_SIZE]; }
	};
	static storage_t storage;
	friend class object_list;
};

class object_list {
	// HASH_FUNC is size_t hash(args...) and equal is bool(const T*, args...)
	template<typename T, typename EQUALF, typename HASHF,  size_t STATIC_CACHE_SIZE=64>
	class static_cache {
	public:
		constexpr static size_t BUCKETS_SHIFT = 2;
		constexpr static size_t CACHE_SIZE = STATIC_CACHE_SIZE;
		constexpr static size_t BUCKETS = STATIC_CACHE_SIZE >> BUCKETS_SHIFT;
};
	// sleep wakeup fuctions, fake ones are in the os.cpp
#endif
} /* namespace xv6 */

#endif /* XV6CPP_OS_H_ */
