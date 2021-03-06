#ifndef _ATOMIC_H_
#define _ATOMIC_H_

//#include "conf.h"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <type_traits>


namespace ARM {
	namespace PRIV {
	template<typename T, size_t size> struct EXCLUSIVE {};
	template<typename T> struct EXCLUSIVE<T,1> {
		static constexpr bool type_is_signed = std::is_unsigned<T>::value;
		static constexpr bool type_size = 1;
		using type = T;

		__attribute__((always_inline)) static inline uint32_t STREX(T value, volatile T *addr){
			uint32_t result;
			   __asm volatile ("strexb %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" (value) );
			   return(result);
		}
		__attribute__((always_inline)) static inline T LDREX(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrexb %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline T LDRT(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrbt %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline void CLREX(void)
		{
		  __asm volatile ("clrex" ::: "memory");
		}
		__attribute__((always_inline)) static inline void STRT(T value, volatile T *addr){
			   __asm volatile ("strt %1, %0" : "=Q" (*addr) : "r" (static_cast<uint32_t>(value)) );
		}
		__attribute__((always_inline)) static inline T XCHG(T nvalue, volatile T *ptr) {
			uint32_t tmp;
			uint32_t ret;
			__builtin_prefetch((const void*)ptr,1);
			asm volatile(
			"1:	ldrexb	%0, [%3]\n"
			"	strexb	%1, %2, [%3]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
				: "=&r" (ret), "=&r" (tmp)
				: "r" (nvalue), "r" (ptr)
				: "memory", "cc");
			return static_cast<T>(ret);
		}
		__attribute__((always_inline)) static inline T ADD(T value, T *ptr) {
				T tmp;
				int result;
				__asm__ __volatile__(
						"1:	ldrexb	%0, [%2]\n"
						"	add	%0, %0, %3\n"
						"	strexb	%1, %0, [%2]\n"
						" cmp     %1, #0\n"
						" beq     1b\n"
				: "=&r" (result), "=&r" (tmp)
				: "r" (ptr), "Ir" (value)
				: "cc");
				return result;
			}
			__attribute__((always_inline)) static inline T SUB(T value, T *ptr) {
				T tmp;
				int result;
				__asm__ __volatile__(
						"1:	ldrexb	%0, [%2]\n"
						"	sub	%0, %0, %3\n"
						"	strexb	%1, %0, [%2]\n"
						" cmp     %1, #0\n"
						" beq     1b\n"
				: "=&r" (result), "=&r" (tmp)
				: "r" (ptr), "Ir" (value)
				: "cc");
				return result;
			}

	};

	template<typename T> struct EXCLUSIVE<T,2> {
		static constexpr bool type_is_signed = std::is_unsigned<T>::value;
		static constexpr bool type_size = 2;
		using type = T;

		__attribute__((always_inline)) static inline uint32_t STREX(T value, volatile T *addr){
			uint32_t result;
			   __asm volatile ("strexh %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" (value) );
			   return(result);
		}
		__attribute__((always_inline)) static inline T LDREX(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrexh %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline void CLREX(void)
		{
		  __asm volatile ("clrex" ::: "memory");
		}
		__attribute__((always_inline)) static inline T LDRT(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrht %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline void STRT(T value, volatile T *addr){
			   __asm volatile ("strht %1, %0" : "=Q" (*addr) : "r" (static_cast<uint32_t>(value)) );
		}
		__attribute__((always_inline)) static inline T XCHG(T nvalue, volatile T *ptr) {
			uint32_t tmp;
			uint32_t ret;
			__builtin_prefetch((const void*)ptr,1);
			asm volatile(
			"1:	ldrexh	%0, [%3]\n"
			"	strexh	%1, %2, [%3]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
				: "=&r" (ret), "=&r" (tmp)
				: "r" (nvalue), "r" (ptr)
				: "memory", "cc");
			return static_cast<T>(ret);
		}
		__attribute__((always_inline)) static inline T ADD(T value, volatile T *ptr) {
			T tmp;
			int result;
			__asm__ __volatile__(
					"1:	ldrexh	%0, [%2]\n"
					"	add	%0, %0, %3\n"
					"	strexh	%1, %0, [%2]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
			: "=&r" (result), "=&r" (tmp)
			: "r" (ptr), "Ir" (value)
			: "cc");
			return result;
		}
		__attribute__((always_inline)) static inline T SUB(T value, volatile T *ptr) {
			T tmp;
			int result;
			__asm__ __volatile__(
					"1:	ldrexh	%0, [%2]\n"
					"	sub	%0, %0, %3\n"
					"	strexh	%1, %0, [%2]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
			: "=&r" (result), "=&r" (tmp)
			: "r" (ptr), "Ir" (value)
			: "cc");
			return result;
		}
	};
	template<typename T> struct EXCLUSIVE<T,4> {
		static constexpr bool type_is_signed = std::is_unsigned<T>::value;
		static constexpr bool type_size = 2;
		using type = T;

		__attribute__((always_inline)) static inline uint32_t STREX(T value, volatile T *addr){
			uint32_t result;
			   __asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" (value) );
			   return(result);
		}
		__attribute__((always_inline)) static inline T LDREX(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrex %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline void CLREX(void)
		{
		  __asm volatile ("clrex" ::: "memory");
		}
		__attribute__((always_inline)) static inline T LDRT(volatile T *addr)
		{
		    uint32_t result;
		    __asm volatile ("ldrt %0, %1" : "=r" (result) : "Q" (*addr) );
		   return static_cast<T>(result);
		}
		__attribute__((always_inline)) static inline void STRT(T value, volatile T *addr){
			   __asm volatile ("strt %1, %0" : "=Q" (*addr) : "r" (static_cast<uint32_t>(value)) );
		}
		__attribute__((always_inline))
		static inline T XCHG(T nvalue, volatile T *ptr) {
			register uint32_t tmp __asm("r3");
			 uint32_t ret;
			__builtin_prefetch((const void*)ptr,1);
			asm volatile(
					"1:	ldrex	%0, [%3]\n"
					"	strex	%1, %2, [%3]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
				: "=&r" (ret), "=&r" (tmp)
				: "r" (nvalue), "r" (ptr)
				: "memory", "cc");
			return static_cast<T>(ret);
		}
		__attribute__((always_inline))
		static inline T CMPXCHG(T oval, T nval, volatile T *ptr) {
			T oldval, res;
			__builtin_prefetch((const void*)ptr,1);
			do {
				__asm__ __volatile__("@ atomic_cmpxchg\n"
				"ldrex	%1, [%2]\n"
				"mov	%0, #0\n"
				"teq	%1, %3\n"
				"strexeq %0, %4, [%2]\n"
				    : "=&r" (res), "=&r" (oldval)
				    : "r" (ptr), "Ir" (oval), "r" (nval)
				    : "cc");
			} while (res);
			return static_cast<T>(oldval);
		}
		__attribute__((always_inline)) static inline T ADD(T value, T *ptr) {
			T tmp;
			int result;
			__asm__ __volatile__(
					"1:	ldrex	%0, [%2]\n"
					"	add	%0, %0, %3\n"
					"	strex	%1, %0, [%2]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
			: "=&r" (result), "=&r" (tmp)
			: "r" (ptr), "Ir" (value)
			: "cc");
			return result;
		}
		__attribute__((always_inline)) static inline T SUB(T value, T *ptr) {
			T tmp;
			int result;
			__asm__ __volatile__(
					"1:	ldrex	%0, [%2]\n"
					"	sub	%0, %0, %3\n"
					"	strex	%1, %0, [%2]\n"
					" cmp     %1, #0\n"
					" beq     1b\n"
			: "=&r" (result), "=&r" (tmp)
			: "r" (ptr), "Ir" (value)
			: "cc");
			return result;
		}
	};
	template<typename T>
	using HELPER = EXCLUSIVE<T,sizeof(T)>;

#define CREATE_MSR_MRS_STRUCT(REGNAME) 											\
	struct REGNAME { 															\
		__attribute__( ( always_inline ) ) static inline uint32_t get() { 		\
		  	  uint32_t result;													\
		  	  __asm volatile ("MRS %0," #REGNAME : "=r" (result) );				\
		  	  return(result);													\
		}																		\
		__attribute__( ( always_inline ) ) static inline void set(uint32_t v) { \
			__asm volatile ("MSR " #REGNAME ", %0" : : "r" (v) : "memory");		\
		}																		\
	};
	CREATE_MSR_MRS_STRUCT(CONTROL);
	CREATE_MSR_MRS_STRUCT(IPSR);
	CREATE_MSR_MRS_STRUCT(APSR);
	CREATE_MSR_MRS_STRUCT(PSP);
	CREATE_MSR_MRS_STRUCT(MSP);
	CREATE_MSR_MRS_STRUCT(PRIMASK);
	CREATE_MSR_MRS_STRUCT(FAULTMASK);
	CREATE_MSR_MRS_STRUCT(BASEPRI);
	CREATE_MSR_MRS_STRUCT(BASEPRI_MAX);


	struct FPSCR {
		__attribute__( ( always_inline ) ) static inline uint32_t get() {
		  	  uint32_t result;
				__asm volatile ("");
				__asm volatile ("VMRS %0, fpscr" : "=r" (result) );
				__asm volatile ("");
		  	  return(result);
		}
		__attribute__( ( always_inline ) ) static inline void set(uint32_t v) {
			__asm volatile ("");
			__asm volatile ("VMSR fpscr, %0" : : "r" (v) : "vfpcc");
			__asm volatile ("");
		}
	};
	template<typename T>
	struct REG {
		__attribute__( ( always_inline ) ) static inline uint32_t get() { return T::get(); }
		__attribute__( ( always_inline ) ) static inline void set(uint32_t v) { T::set(v); }
		__attribute__( ( always_inline ) ) inline REG& operator=(uint32_t v) const { T::set(v); return *this; }
		__attribute__( ( always_inline ) ) inline operator uint32_t() const { return T::get(); }
	};
};
__attribute__( ( always_inline ) ) static inline void irq_enable() { __asm volatile ("cpsie i" : : : "memory"); }
__attribute__( ( always_inline ) ) static inline void irq_disable() { __asm volatile ("cpsid i" : : : "memory"); }
__attribute__( ( always_inline ) ) static inline void fault_irq_enable() { __asm volatile ("cpsie f" : : : "memory"); }
__attribute__( ( always_inline ) ) static inline void fault_irq_disable() { __asm volatile ("cpsid f" : : : "memory"); }
__attribute__( ( always_inline ) ) static inline void nop() { __asm volatile ("nop"); }
__attribute__( ( always_inline ) ) static inline void wfi() { __asm volatile ("wfi"); }
__attribute__( ( always_inline ) ) static inline void wfe() { __asm volatile ("wfe"); }
__attribute__( ( always_inline ) ) static inline void sev() { __asm volatile ("sev"); }
__attribute__( ( always_inline ) ) static inline void isb() { __asm volatile ("isb 0xF":::"memory"); }
__attribute__( ( always_inline ) ) static inline void dsb() { __asm volatile ("dsb 0xF":::"memory"); }
__attribute__( ( always_inline ) ) static inline void dmb() { __asm volatile ("dmb 0xF":::"memory"); }

__attribute__( ( always_inline ) ) static inline void dsb_sev(void) { dmb(); nop(); }
__attribute__( ( always_inline ) ) static inline void barrier(void) { dmb();  }

using CONTROL = PRIV::REG<PRIV::CONTROL>;
using IPSR = PRIV::REG<PRIV::IPSR>;
using APSR = PRIV::REG<PRIV::APSR>;
using PSP = PRIV::REG<PRIV::PSP>;
using MSP = PRIV::REG<PRIV::MSP>;
using FAULTMASK = PRIV::REG<PRIV::FAULTMASK>;
using BASEPRI = PRIV::REG<PRIV::BASEPRI>;
using BASEPRI_MAX = PRIV::REG<PRIV::BASEPRI_MAX>;
using FPSCR = PRIV::REG<PRIV::FPSCR>;
using PRIMASK = PRIV::REG<PRIV::PRIMASK>;

struct IRQ_HARDWARE_STACK {
	uint32_t R0;
	uint32_t R1;
	uint32_t R3;
	uint32_t IP;
	uint32_t LR;
	uint32_t PC;
	uint32_t XPSR;
};


template<typename T, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline bool atomic_test_and_set(T *ptr)
{
	return EX::XCHG(1,ptr) == 1;
}

template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T atomic_xchg(volatile T *ptr, U v) {
	return EX::XCHG(static_cast<T>(v),ptr);
}

template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline bool strex(volatile T *ptr,U v){ return EX::STREX(static_cast<T>(v),ptr) != 0; }

template<typename T, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T ldrex(volatile T *addr) { return EX::LDREX(addr);}


template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T xchg(volatile T *ptr, U v) {
	return EX::XCHG(static_cast<T>(v),ptr);
}
template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T atomic_add(volatile T *ptr, U v) {
	return EX::ADD(static_cast<T>(v),ptr);
}
template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T atomic_add_return(volatile T *ptr, U v) {
	return EX::ADDRET(static_cast<T>(v),ptr);
}
template<typename T, typename U, typename V, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T atomic_cmpxchg(volatile T *ptr, U old_value, V new_value) {
	return EX::CMPXCHG(static_cast<T>(old_value),static_cast<T>(new_value), ptr);
}

template<typename T, typename U, typename EX = PRIV::EXCLUSIVE<T,sizeof(T)>>
static inline T atomic_sub(volatile T *ptr, U v) {
	return EX::SUB(static_cast<T>(v),ptr);
}
// dead simple atomic lock
class atomic_lock {
	uint32_t _lock;
public:
	atomic_lock() : _lock(0) {}
	~atomic_lock() { _lock = 0; }
	bool try_lock() { return !atomic_test_and_set(&_lock); }
	void lock() { while(atomic_test_and_set(&_lock)) ; }
	void unlock() { _lock = 0; }
};
class simple_irq_lock {
	uint32_t _mask;
public:
	simple_irq_lock() : _mask(PRIMASK::get()) {PRIMASK::set(1); }
	~simple_irq_lock() { PRIMASK::set(_mask); }
};

template<typename T>
class lock_irq {
	uint8_t _save;
	uint8_t _prio;
public:
	lock_irq(uint8_t s=0) : _save(T::get()), _prio(s) { T::set(0); }
	inline constexpr bool enabled() const { return T::get() !=  _save;  }
	void disable() { T::set(_prio); }
	void restore() { T::set(_save); }
	~lock_irq() { T::set(_save); }
};

using lock_irq_irq = lock_irq<PRIV::PRIMASK>;
using lock_irq_basepri = lock_irq<PRIV::BASEPRI>;
};
#endif
