/*
 * proc.cpp
 *
 *  Created on: May 11, 2017
 *      Author: Paul
 */

#include "proc.hpp"
#include <os\atomic.h>
#include <os\irq.hpp>
#define _SYSTEM
#include <errno.h>
#include <cstring>
#include <os\printk.hpp>
#include <os\hash.hpp>
#include <signal.h>
#define OK 0
//#if 0
/* The following error codes are generated by the kernel itself. */
#define E_BAD_DEST        -1	/* destination address illegal */
#define E_BAD_SRC         -2	/* source address illegal */
#define E_TRY_AGAIN       -3	/* can't send-- tables full */
#define E_OVERRUN         -4	/* interrupt for task that is not waiting */
#define E_BAD_BUF         -5	/* message buf outside caller's addr space */
#define E_TASK            -6	/* can't send to task */
#define E_NO_MESSAGE      -7	/* RECEIVE failed: no message present */
#define E_NO_PERM         -8	/* ordinary users can't send to tasks */
#define E_BAD_FCN         -9	/* only valid fcns are SEND, RECEIVE, BOTH */
#define E_BAD_ADDR       -10	/* bad address given to utility routine */
#define E_BAD_PROC       -11	/* bad proc number given to utility */
//#endif
mimx::proc *g_proc_current=nullptr;     // Current process on this cpu.

__attribute__((weak)) void mimx_idle_task() {

}

struct sigaction  signal_actions[NSIG];

void __attribute__ (( naked )) handle_signals() {
#if 0
	__asm__ __volatile__ ("push {r0-r15}" : : "r"(pc_return) ); // have to save eveything
	while(g_proc_current->p_sig){
		int n = g_proc_current->fsig();
		g_proc_current->p_sig &= (1<<n);
		printk("Pid %d handled signal %d\n", g_proc_current->pid, n);
	}
	__asm__ __volatile__ ("pop {lr}");
	__asm__ __volatile__ ("bx lr");
#endif
}

//globals, FIX
namespace mimx{


	__attribute__((always_inline)) 	static  inline void request_schedule(){
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
	using proc_queue = tailq::head<proc,&proc::link>;
	using proc_hash_queue = tailq::head<proc,&proc::hash_lookup>;

	struct proc_chan_hasher  {
	//	template<typename T>
		constexpr size_t operator()(const void* x) const { return hash::int_hasher<void*>()(x);  }
		constexpr size_t operator()(const proc& x) const { return operator()(x.p_wchan); }
	};
	struct proc_chan_equals  {
		constexpr bool operator()(const proc& a, const proc& b) const { return a.p_wchan== b.p_wchan; }
		//template<typename T>
		constexpr bool operator()(const proc& a,  const void* b) const { return a.p_wchan == b; }
	};
	struct proc_hasher  {
		constexpr size_t operator()(const pid_t x) const { return hash::int_hasher<pid_t>()(x);  }
		constexpr size_t operator()(const proc& x) const { return operator()(x.p_pid); }
	};
	struct proc_equals  {
		constexpr bool operator()(const proc& a, const proc& b) const { return a.p_pid == b.p_pid; }
		//template<typename T>
		constexpr bool operator()(const proc& a, const pid_t b) const { return a.p_pid == b; }
	};
	using pid_lookup_t = hash::table<proc,proc_hash_queue,16,proc_hasher,proc_equals>;
	using chan_lookup_t = hash::table<proc,proc_queue,16,proc_chan_hasher,proc_chan_equals>;
	static pid_lookup_t 	proc_lookup;
	static chan_lookup_t 	procs_sleeping;
	static proc_queue 		procs_running;
	static proc_queue 		procs_zombie;	// unkonwn status
	static inline pid_t nextpid() {
		static pid_t s_nextpid = 1000;
		pid_t ret = s_nextpid++;
		if(ret > 50000) s_nextpid = ret = 1000;
		return ret;
	}

	struct idle_proc_t : public proc {
		uint8_t idle_proc_stack[256];
		static void idle_task() {
			while(1) {
				__WFI(); // just wait for an interrupt
				SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // try  to recheduale
			}
		}
		idle_proc_t() {
			// uint8_t* proc_mem = reinterpret_cast<uint8_t*>(idle_proc_stack);
			// proc* p = new(proc_mem) proc;
			 p_pid = nextpid();
			 stack_base = reinterpret_cast<uint32_t>(idle_proc_stack);
			 stack_size = sizeof(idle_proc_stack);
			 heap_end =  stack_base + sizeof(proc);
			 ctx.init(stack_base + stack_size, &idle_task);
			 p_flag |= SLOAD; // we are always loaded
			// assert(proc_lookup.insert(p) == hash::status::ok);
		}
	};
	static idle_proc_t idle_proc;


	void proc::setrq() {
		// diag
		for(auto& p : procs_running){
			if(&p == this) panic("proc on q\n");
		}
		procs_running.push_front(this);
	}
	void proc::setrun() {
		void* w;

        if (p_stat==0 || p_stat==SZOMB)
                panic("Running a dead proc");
        /*
         * The assignment to w is necessary because of
         * race conditions. (Interrupt between test and use)
         */

        if ((w = p_wchan) != nullptr) {
                wakeup(w);
                return;
        }
        p_stat = SRUN;
        setrq();
       // if(p->p_pri < curpri) runrun++;
#if 0
       // if(queue empty, and not in core, start the file swaper)
        if(runout != 0 && (p_flag&SLOAD) == 0) {
                runout = 0;
                wakeup((caddr_t)&runout);
        }
#endif
	}
	int proc::setpri() {
        uint32_t p= (p_cpu & 0377)/16;
        p += PUSER + p_nice - NZERO;
        if(p > 127) p = 127;
        if(p < g_proc_current->p_pri)  request_schedule();;
        p_pri = p;
        return(p);
	}
	proc* proc::schedule_select() {
		// switches the process to the next runable
		if(procs_running.empty()) return &idle_proc;
		proc* pp = nullptr;
		uint32_t n = 128;
		for(auto& p : procs_running){
			if(p.p_stat == SRUN && (p.p_flag & SLOAD)) {
				if(p.p_pri < n) {
					pp = &p;
					n = p.p_pri;
				}
			}
		}
		return pp != nullptr ? pp : &idle_proc ;
	}

	 proc* proc::create(uintptr_t pc, uintptr_t stack, size_t stack_size) {
		 uint8_t* proc_mem = reinterpret_cast<uint8_t*>(stack);
		 proc* p = new(proc_mem) proc;
		 p->p_flag |= SLOAD; // we are always loaded
	     p->p_stat = SIDL;
		 p->p_pid = nextpid();
		 p->stack_base = stack;
		 p->stack_size = stack_size;
		 p->heap_end = stack + sizeof(proc);
		 assert(proc_lookup.insert(p) == hash::status::ok);
		 p->ctx.init(stack + stack_size, pc);
		 return p;
	}

	proc* proc::vfork(uintptr_t stack, size_t nstack_size){
		assert(nstack_size == stack_size);
		uint8_t* proc_mem = reinterpret_cast<uint8_t*>(stack);
		uint8_t* parent_mem = reinterpret_cast<uint8_t*>(stack_base);
		std::copy(parent_mem,parent_mem+stack_size,proc_mem);

		proc* p = new(proc_mem) proc;
		p->p_stat = SRUN;
		p->p_flag = SLOAD;
		p->p_nice = p_nice;
		p->p_pid = nextpid();
		p->p_clktim = p->p_cpu = p->p_time = 0;		// reset run times
		g_proc_current->p_children.push_front(p);
		assert(proc_lookup.insert(p) == hash::status::ok);
		uint32_t current_stack_size = stack_base -     reinterpret_cast<uintptr_t>(ctx.sp);
		p->stack_base = stack;
		p->stack_size = nstack_size;
		p->ctx = ctx;
		p->ctx.sp = reinterpret_cast<uint32_t*>(current_stack_size + stack);
		p->ctx.at(f9::REG::R0) = 0;
	//	ctx.at(REG::R0) = p->p_pid;
		return p;
	}
	bool proc::growproc(int n) {
		// with no virtual memory we only grow in the stack location
		uintptr_t new_heap = heap_end + n;
	//	uintptr_t memory_end = stack_base + stack_size;
	//	if(new_heap > ctx.sp) return false; // new heap is beyond stack
		heap_end = new_heap;
		return true;
	}
	  void proc::sleep(void* chan,int prio) {
		  if(chan == nullptr) panic("sleep");
		  // we are current and running soo..
		  p_stat = SSLEEP;
		  p_wchan = chan;
		  p_pri = prio;
		  if(prio> PZERO) {
			// if(issig()) // remove from run que and SRUN it
			//else normal switch
		  }
		  procs_running.remove(g_proc_current);
		  assert(procs_sleeping.insert(g_proc_current,true) == hash::status::ok);
		  request_schedule();
	  }
	  void proc::wakeup(void* chan){
		  proc* p;

		  while((p=procs_sleeping.search(chan)) != nullptr) {
			  if(p->p_stat != SZOMB){
				  procs_sleeping.remove(p);
				  p->p_wchan = nullptr;
				  p->setrun();
			  }
		  }
	  }
	  void proc::yield(){
		  p_nice = 127; // make the prioity so bad that it must be the last in the queue
		  request_schedule();
	  }
	void proc::sched(){

	}
#if 0
	proc proc::procs[NR_TASKS+NR_PROCS];
	proc *proc::proc_ptr=nullptr;	/* &proc[cur_proc] */
	proc *proc::bill_ptr=nullptr;	/* ptr to process to bill for clock ticks */
	int proc::prev_proc = 0;
	int proc::cur_proc = 0;
	uint32_t proc::busy_map=0;		/* bit map of busy tasks */
	message proc::task_mess[NR_TASKS+1];	/* ptrs to messages for busy tasks */
	proc::proc_queue_t proc::rdy_queue[NQ];	/* pointers to ready list tails */
	message_hash_t proc::message_lookup;
	int proc::interrupt(int task, message& m_ptr) {
		/* An interrupt has occurred.  Schedule the task that handles it. */

		  uint32_t old_map;
		  int ret = 0; // not busy
		  /* Try to send the interrupt message to the indicated task. */
		  uint32_t this_bit = 1 << (-task);
		  if (mini_send(HARDWARE, task, m_ptr) != OK) {
			/* The message could not be sent to the task; it was not waiting. */
			old_map = busy_map;	/* save original map of busy tasks */
			///if (task == CLOCK) {
			//	lost_ticks++;
			//} else {
				busy_map |= this_bit;		/* mark task as busy */
				task_mess[-task] = m_ptr;	/* record message pointer */
			//}
			ret = -1; // error
		  } else {
			/* Hardware interrupt was successfully sent as a message. */
			busy_map &= ~this_bit;	/* turn off the bit in case it was on */
			old_map = busy_map;
		  }

		  /* See if any tasks that were previously busy are now listening for msgs. */
		  if (old_map != 0) {
			for (int i = 2; i <= NR_TASKS; i++) {
				/* Check each task looking for one with a pending interrupt. */
				if ( (old_map>>i) & 1) {
					/* Task 'i' has a pending interrupt. */
					int n = mini_send(HARDWARE, -i, task_mess[i]);
					if (n == OK) busy_map &= ~(1 << i);
				}
			}
		  }

		  /* If a task has just been readied and a user is running, run the task. */
		  if (!rdy_queue[TASK_Q].empty()&& (cur_proc >= 0 || cur_proc == IDLE))
			pick_proc();
		  return ret;
	}
	int proc::sys_call(int function, int caller, int src_dest, message& m_ptr){
		/* The only system calls that exist in MINIX are sending and receiving
		 * messages.  These are done by trapping to the kernel with an INT instruction.
		 * The trap is caught and sys_call() is called to send or receive a message (or
		 * both).
		 */

		  int n;

		  /* Check for bad system call parameters. */
		  proc* rp = proc_addr(caller);
		  if (src_dest < -NR_TASKS || (src_dest >= NR_PROCS && src_dest != ANY) )
			  return E_BAD_SRC;
		//	  rp->ctx[REG::R0] = E_BAD_SRC;


		  if (function != BOTH && caller >= LOW_USER)
			  return E_NO_PERM;	/* users only do BOTH */
			  //rp->ctx[REG::R0]= E_NO_PERM;	/* users only do BOTH */
		  /* The parameters are ok. Do the call. */
		  if (function & SEND) {
				/* func = SEND or BOTH */
			if ((n=mini_send(caller, src_dest, m_ptr)) != OK) return n;  // rp->p_reg[RET_REG] = n;
		  }
		  return (function & RECEIVE) ? mini_rec(caller, src_dest, m_ptr) : OK;      /* func = RECEIVE or BOTH */
	}

	int proc::mini_send(int caller, int dest, message& m_ptr){
		/* Send a message from 'caller' to 'dest'.  If 'dest' is blocked waiting for
		 * this message, copy the message to it and unblock 'dest'.  If 'dest' is not
		 * waiting at all, or is waiting for another source, queue 'caller'.
		 */

		 proc *caller_ptr, *dest_ptr, *next_ptr;

		  /* User processes are only allowed to send to FS and MM.  Check for this. */
		  if (caller >= LOW_USER && (dest != FS_PROC_NR && dest != MM_PROC_NR))
			return(E_BAD_DEST);
		  caller_ptr = proc_addr(caller);	/* pointer to source's proc entry */
		  dest_ptr = proc_addr(dest);	/* pointer to destination's proc entry */
		  if (dest_ptr->p_flags & P_SLOT_FREE) return(E_BAD_DEST);	/* dead dest */


		  /* Check to see if 'dest' is blocked waiting for this message. */

		  if ( (dest_ptr->p_flags & RECEIVING) &&
				(dest_ptr->p_getfrom == ANY || dest_ptr->p_getfrom == caller) ) {
			  /* Destination is indeed waiting for this message. */
			  // don't bother putting it on the que
			  dest_ptr->p_messageq.push_front(&m_ptr); // either case the message is queded
			  dest_ptr->p_flags &= ~RECEIVING;	/* deblock destination */
			  if (dest_ptr->p_flags == 0)  {
				  // insted of encusing the message we return it
				  dest_ptr->ctx.set_return_value(&m_ptr);
				  dest_ptr->ready(); // push front
			  }
		  } else {
			/* Destination is not waiting.  Block and queue caller. */
			 if (caller == HARDWARE) return(E_OVERRUN);
			auto status = message_lookup.insert(&m_ptr);
				if(status == list::status::exists)  // need to block here somehow
					panic("message already is in queue somewhere");
				dest_ptr->p_messageq.push_front(&m_ptr);
				caller_ptr->p_flags |= SENDING;
				caller_ptr->unready(); // tail push

		  }

		  return(OK);


	}
	int proc::mini_rec(int caller, int src, message &m_ptr){
		/* A process or task wants to get a message.  If one is already queued,
		 * acquire it and deblock the sender.  If no message from the desired source
		 * is available, block the caller.  No need to check parameters for validity.
		 * Users calls are always sendrec(), and mini_send() has checked already.
		 * Calls from the tasks, MM, and FS are trusted.
		 */
		  proc *caller_ptr = proc_addr(caller);	/* pointer to caller's proc structure */
		  /* Check to see if a message from desired source is already available. */
		  for(auto it= caller_ptr->p_callerq.begin(); it != caller_ptr->p_callerq.end(); it++){
			  proc *sender_ptr = &(*it);
			  int sender = sender_ptr->proc_number();
			if (src == ANY || src == sender) {
				/* An acceptable message has been found. */
				caller_ptr->p_messbuf = message(sender, caller_ptr->p_messbuf);
				sender_ptr->p_flags &= ~SENDING;	/* deblock sender */
				if (sender_ptr->p_flags == 0) sender_ptr->ready();
				caller_ptr->p_callerq.erase(it);
				return(OK);
			}
		  }

		  /* No suitable message is available.  Block the process trying to receive. */
		  caller_ptr->p_getfrom = src;
		  caller_ptr->p_messbuf = &m_ptr;
		  if((caller_ptr->p_flags & RECEIVING) == 0) {
			  caller_ptr->p_flags |= RECEIVING;
			  caller_ptr->unready();
		  }

		  /* If MM has just blocked and there are kernel signals pending, now is the
		   * time to tell MM about them, since it will be able to accept the message.
		   */
		 // if (sig_procs > 0 && caller == MM_PROC_NR && src == ANY) inform(MM_PROC_NR);
		  return(OK);
	}

	void  proc::sched(){
		irq_simple_lock lock;
		if(rdy_queue[USER_Q].empty()) return;
		auto p = rdy_queue[USER_Q].first_entry();
		rdy_queue[USER_Q].pop_front();
		rdy_queue[USER_Q].push_back(p);
		pick_proc();
	}
	void proc::ready(){
		/* Add 'rp' to the end of one of the queues of runnable processes. Three
		 * queues are maintained:
		 *   TASK_Q   - (highest priority) for runnable tasks
		 *   SERVER_Q - (middle priority) for MM and FS only
		 *   USER_Q   - (lowest priority) for user processes
		 */
		irq_simple_lock lock;
		assert(p_flags == 0);
		auto& q = proc_queue();
		q.push_back(this);
	}
	void proc::unready(){
		/* A process has blocked. */
		irq_simple_lock lock;
		assert(p_flags != 0);
		auto& q = proc_queue();
		q.remove(this);
	}
	void proc::pick_proc(){
		/* Decide who to run now. */

		/* which queue to use */
		  proc::proc_queue_t& q = !rdy_queue[TASK_Q].empty() ? rdy_queue[TASK_Q] : !rdy_queue[SERVER_Q].empty() ? rdy_queue[SERVER_Q] :rdy_queue[USER_Q];

		  /* Set 'cur_proc' and 'proc_ptr'. If system is idle, set 'cur_proc' to a
		   * special value (IDLE), and set 'proc_ptr' to point to an unused proc table
		   * slot, namely, that of task -1 (HARDWARE), so save() will have somewhere to
		   * deposit the registers when a interrupt occurs on an idle machine.
		   * Record previous process so that when clock tick happens, the clock task
		   * can find out who was running just before it began to run.  (While the
		   * clock task is running, 'cur_proc' = CLOCKTASK. In addition, set 'bill_ptr'
		   * to always point to the process to be billed for CPU time.
		   */
		  prev_proc = cur_proc;
		  if (!q.empty()) {
			/* Someone is runnable. */
			  proc_ptr = q.first_entry();

			cur_proc = proc_ptr - proc - NR_TASKS;
			if (cur_proc >= LOW_USER) bill_ptr = proc_ptr;
		  } else {
			/* No one is runnable. */
			cur_proc = IDLE;
			proc_ptr = proc_addr(HARDWARE);
			bill_ptr = proc_ptr;
		  }
		  if(prev_proc!=cur_proc){
			  if(!proc_ptr->p_messageq.empty()){
				  // get a message and return it
				  auto m_ptr  = proc_ptr->p_messageq.first_entry();
				  message_lookup.remove(m_ptr);
				  proc_ptr->p_messageq.remove(m_ptr);
				  proc_ptr->ctx.set_return_value(m_ptr);
			  } else {
				  proc_ptr->ctx.set_return_value(0);
			  }
			  request_schedule();
		  }


	}
#endif
#if 0
  	static proc *proc_ptr;	/* &proc[cur_proc] */
  	static uint32_t curpriority = 0;
    void proc::resetpriority(){
		uint32_t newpriority;

		newpriority = PUSER + p_estcpu / 4 + 2 * p_nice;
		newpriority = std::min(newpriority, MAXPRI);
		p_usrpri = newpriority;
		if (newpriority < curpriority)
			request_schedule();
    }
	void proc::update_priority(){
		auto loadfac = loadfactor(ldavg[0]);
		uint32_t newcpu = p_estcpu;
	    if (p_slptime > (5 * loadfactor(ldavg[0])))
			p_estcpu = 0;
		else {
			p_slptime--; /* the first time was done in schedcpu */
			while (newcpu && --p_slptime)
				newcpu = (int) decay_cpu(loadfac, newcpu);
			p_estcpu = std::min(newcpu, (uint32_t)UCHAR_MAX);
		}
	    resetpriority();
	}
  	//static proc *prev_proc_ptr; // previous process
  	static proc *bill_ptr;	/* ptr to process to bill for clock ticks */
  	using softirq_queue_t = tailq::head<softirq,&softirq::_link> ;
  	static softirq_queue_t softirq_queue;
  	using proc_queue_t = tailq::head<proc,&proc::p_link>;
  	static proc_queue_t ready_queue[proc::NQ]; /* pointers to ready list headers */

  	static bitops::bitmap_t<NR_TASKS+1> busy_map;		/* bit map of busy tasks */
  	static message *task_mess[NR_TASKS+1];	/* ptrs to messages for busy tasks */
  	static softirq* SOFTIRQ_MARK = reinterpret_cast<softirq*>(0x123456789);
  	static proc* PROC_MARK = reinterpret_cast<proc*>(0x123456789);
	static proc _idle_proc;

	void softirq::execute() {
		while(!softirq_queue.empty()) { // do nothing if nothing is queueed
			irq_simple_lock lock;
			softirq_queue_t tmp(std::move(softirq_queue));
			while(!tmp.empty()){
				auto it = tmp.pop_front();
				it->_link.tqe_next = SOFTIRQ_MARK;
				lock.disable();
				it->handler();
				lock.enable();
			}
#if 0
			softirq* _current = _head;
			_head = _tail = nullptr;
			while(_current) {
				_current = _head;
				_head = _head->_next;
				_current->_next = nullptr; // remove from queue
				lock.disable();
				_current->handler();
				lock.enable();
			}
#endif
		}
	}
	void softirq::schedule() {
		irq_simple_lock lock;
		if(_link.tqe_next == SOFTIRQ_MARK){
			_link.tqe_next = nullptr;
			softirq_queue.push_back(this);
		}

	}
	class syscall_softirq : softirq {
		proc* caller;
		void sys_thread_control(uint32_t dest, uint32_t space, uint32_t pager, void* utcb)
		{
			(void)dest,(void)space,(void)pager,(void)utcb;
			assert(0); // never get here
		}
		void execute() {
			auto& ctx = caller->ctx;
			uint32_t svc_num = ctx.svc_number();
			if(svc_num ==0) { 	// task create
				int nr = 0;
				for(mimx::proc* p = mimx::proc::BEG_USER_ADDR(); p != mimx::proc::END_PROC_ADDR(); p++) {
					if(p->p_not_a_zombie){
						p->p_not_a_zombie = true;
						p->p_nr = nr;
						uintptr_t pc = ctx.get_call_arg(0);
						uint32_t* stack = ctx.get_call_arg<uint32_t*>(1);
						size_t size = ctx.get_call_arg(2);
						uint32_t* sp = stack + (size/4);
						p->ctx.init(sp,pc);
						p->ready();
						ctx[REG::R0] = nr;
						if(caller == &_idle_proc) mimx::proc::pick_proc();
						return; // done
						// first task so lets switch to it right now
					}
					nr++;
				}
				assert(0); // no procs?
			} else if(svc_num == 1 || svc_num == 2 || svc_num == 3){
				int src_dst = ctx.get_call_arg(0);
				message* m_ptr = ctx.get_call_arg<message*>(1);
				size_t msgsize = ctx.get_call_arg(2);
				int ret = proc::sys_call(svc_num,src_dst,m_ptr,msgsize);
				ctx[REG::R0] = ret;
			} else {
				assert(0); // unkonwn svc call
			}
			if(caller == &_idle_proc) mimx::proc::pick_proc();
		}


	};


	/* Global variables used in the kernel. */
	/* Low level interrupt communications. */
	proc *held_head=nullptr;	/* head of queue of held-up interrupts */
	proc *held_tail=nullptr;	/* tail of queue of held-up interrupts */
	unsigned char k_reenter=0;	/* kernel reentry count (entry count less 1)*/
	static void debug_syscall(f9_context_t& ctx) {
		uint32_t *svc_param1 = (uint32_t *) ctx.sp;
		uint32_t svc_num = ((char *) svc_param1[static_cast<uint32_t>(REG::PC)])[-2];
	}
	static void proc_syscall() {
		proc* prev = proc_ptr;
		auto& ctx = prev->ctx;
		uint32_t svc_num = ctx.svc_number();
		//debug_syscall(ctx);
		dbg::arg_debug("SVC Call",svc_num, ctx.get_call_arg<void*>(0),ctx.get_call_arg(1),ctx.get_call_arg(2),"test" );

		if(svc_num ==0) { 	// task create
			uintptr_t pc = ctx.get_call_arg(0);
			uint32_t* stack = ctx.get_call_arg<uint32_t*>(1);
			size_t size = ctx.get_call_arg(2);
			uint32_t* sp = stack + (size/4);
			for(mimx::proc* p = mimx::proc::BEG_USER_ADDR(); p != mimx::proc::END_PROC_ADDR(); p++) {
				if(!p->p_not_a_zombie){
					p->p_not_a_zombie = true;

					p->ctx.init(sp,pc);
					p->ready();
					if(proc_ptr == &_idle_proc) mimx::proc::pick_proc();
					return; // done
					// first task so lets switch to it right now
				}
			}
			assert(0); // no procs?
		} else if(svc_num == 1 || svc_num == 2 || svc_num == 3){
			int src_dst = ctx.get_call_arg(0);
			message* m_ptr = ctx.get_call_arg<message*>(1);
			size_t msgsize = ctx.get_call_arg(2);
			int ret = proc::sys_call(svc_num,src_dst,m_ptr,msgsize);
			ctx[REG::R0] = ret;
		} else {
			assert(0); // unkonwn svc call
		}

		if(proc_ptr == &_idle_proc) mimx::proc::pick_proc();
	}
};



extern "C" {
	void __attribute__ (( naked ))SVC_Handler();
	void __attribute__ (( naked ))SVC_Handler(){
		if(!mimx::proc_ptr) mimx::proc_ptr = &mimx::_idle_proc; // give it a fake context
		mimx::proc_ptr->ctx.save(); // save the current context
		//register uint32_t* regs __asm__ ("r0");
		//__asm__ __volatile__ ("ite eq");
		//__asm__ __volatile__ ("mrseq r0, msp"::: "r0");
		//__asm__ __volatile__ ("mrsne r0, psp"::: "r0");
		mimx::proc_syscall();
		mimx::proc_ptr->ctx.restore();
		if(mimx::proc_ptr == &mimx::_idle_proc) mimx::proc::pick_proc();
		//if(mimx::proc::proc_ptr) mimx::proc::proc_ptr->p_ctx.restore(); // srestore
	}


};

#if 0
extern "C" void __attribute__ (( naked ))SVC_Handler() {
    // Core with FPU (cortex-M4F)
    asm volatile (
        "    CPSID     I                 \n" // Prevent interruption during syscall
        "    MRS       R3, PSP           \n" // PSP is process stack pointer
      //  "    TST       LR, #0x10         \n" // exc_return[4]=0? (it means that current process
      //  "    IT        EQ                \n" // has active floating point context)
      //  "    VSTMDBEQ  R3!, {S16-S31}    \n" // if so - save it.
        "    STMDB     R3!, {R4-R11, LR} \n" // save remaining regs r4-11 and LR on process stack

        // At this point, entire context of process has been saved
        "    LDR     R1, =_syscall  \n"   // call os_context_switch_hook();
        "    BLX     R1                \n"

        // R0 is new process SP;
        "    LDMIA     R0!, {R4-R11, LR} \n" // Restore r4-11 and LR from new process stack
    //    "    TST       LR, #0x10         \n" // exc_return[4]=0? (it means that new process
    //    "    IT        EQ                \n" // has active floating point context)
    //    "    VLDMIAEQ  R0!, {S16-S31}    \n" // if so - restore it.
        "    MSR       PSP, R0           \n" // Load PSP with new process SP
        "    CPSIE     I                 \n"
        "    BX        LR                \n" // Return to saved exc_return. Exception return will restore remaining context
        : :
    );
}
#endif
#if 0

// notes for svc handler
void __attribute__ (( naked )) sv_call_handler(void)
{
    asm volatile(
      "movs r0, #4\t\n"
      "mov  r1, lr\t\n"
      "tst  r0, r1\t\n" /* Check EXC_RETURN[2] */
      "beq 1f\t\n"
      "mrs r0, psp\t\n"
      "ldr r1,=sv_call_handler_c\t\n"
      "bx r1\t\n"
      "1:mrs r0,msp\t\n"
      "ldr r1,=sv_call_handler_c\t\n"
      : /* no output */
      : /* no input */
      : "r0" /* clobber */
  );
}
sv_call_handler_c(unsigned int * hardfault_args)
{
    unsigned int stacked_r0;
    unsigned int stacked_r1;
    unsigned int stacked_r2;
    unsigned int stacked_r3;
    unsigned int stacked_r12;
    unsigned int stacked_lr;
    unsigned int stacked_pc;
    unsigned int stacked_psr;
    unsigned int svc_parameter;

    //Exception stack frame
    stacked_r0 = ((unsigned long) hardfault_args[0]);
    stacked_r1 = ((unsigned long) hardfault_args[1]);
    stacked_r2 = ((unsigned long) hardfault_args[2]);
    stacked_r3 = ((unsigned long) hardfault_args[3]);

    stacked_r12 = ((unsigned long) hardfault_args[4]);
    stacked_lr  = ((unsigned long) hardfault_args[5]);
    stacked_pc  = ((unsigned long) hardfault_args[6]);
    stacked_psr = ((unsigned long) hardfault_args[7]);

    svc_parameter = ((char *)stacked_pc)[-2]; /* would be LSB of PC is 1. */

    switch(svc_parameter){
    // each procesure call for the parameter
    }
}
#endif

namespace mimx {



	/* Process table.  Here to stop too many things having to include proc.h. */
	//proc *proc_ptr;	/* pointer to currently running process */

	/* Signals. */
	int sig_procs;		/* number of procs with p_pending != 0 */

	/* Clocks and timers */
	time_t realtime;	/* real time clock */
	time_t lost_ticks;		/* incremented when clock int can't send mess*/

	/* Processes, signals, and messages. */
	pid_t cur_proc;		/* current process */
	pid_t prev_proc;		/* previous process */
//	size_t sig_procs;		/* number of procs with p_pending != 0 */
	message int_mess;	/* interrupt routines build message here */


	/* The kernel and task stacks. */
	struct t_stack {
	  int stk[1024/sizeof(int)];
	} t_stack[NR_TASKS - 1];	/* task stacks; task = -1 never really runs */

	char k_stack[1024];	/* The kernel stack. */

	//proc *proc::proc_ptr=nullptr;	/* &proc[cur_proc] */
	//proc *proc::bill_ptr=nullptr;	/* ptr to process to bill for clock ticks */
	//proc *proc::rdy_head[NQ];	/* pointers to ready list headers */
	//proc *proc::rdy_tail[NQ];	/* pointers to ready list tails */
	//bitops::bitmap_t<NR_TASKS+1> busy_map;		/* bit map of busy tasks */
//	message *proc::task_mess[NR_TASKS+1];	/* ptrs to messages for busy tasks */
	proc *proc::pproc_addr[NR_TASKS + NR_PROCS];
	volatile bool proc::switching = false;
	proc proc::procs[NR_TASKS+NR_PROCS];
  	//constexpr static inline proc* proc_addr(pid_t n) { return &proc::procs[NR_TASKS + n]; }

	#define CopyMess(s,sp,sm,dp,dm,sz) cp_mess(s,sp,sm,dp,dm,sz)



	/*===========================================================================*
	 *				interrupt				     *
	 *===========================================================================*/
	void proc::interrupt(int task)
	//int task;			/* number of task to be started */
	{
	/* An interrupt has occurred.  Schedule the task that handles it. */

	  proc *rp;	/* pointer to task's proc entry */

	  rp = proc_addr(task);

	  /* If this call would compete with other process-switching functions, put
	   * it on the 'held' queue to be flushed at the next non-competing restart().
	   * The competing conditions are:
	   * (1) k_reenter == (typeof k_reenter) -1:
	   *     Call from the task level, typically from an output interrupt
	   *     routine.  An interrupt handler might reenter interrupt().  Rare,
	   *     so not worth special treatment.
	   * (2) k_reenter > 0:
	   *     Call from a nested interrupt handler.  A previous interrupt handler
	   *     might be inside interrupt() or sys_call().
	   * (3) switching != 0:
	   *     Some process-switching function other than interrupt() is being
	   *     called from the task level, typically sched() from CLOCK.  An
	   *     interrupt handler might call interrupt and pass the k_reenter test.
	   */
	  if (k_reenter != 0 || switching) {
		  ARM::irq_disable();
		if (!rp->p_int_held) {
			rp->p_int_held = true;
			if (held_head != NIL_PROC)
				held_tail->p_nextheld = rp;
			else
				held_head = rp;
			held_tail = rp;
			rp->p_nextheld = NIL_PROC;
		}
		 ARM::irq_enable();
		return;
	  }
	  switching = true;

	  /* If task is not waiting for an interrupt, record the blockage. */
	  if ( (rp->p_flags & (PSTATE::RECEIVING | PSTATE::SENDING)) != PSTATE::RECEIVING ||
	      !isrxhardware(rp->p_getfrom)) {
		rp->p_int_blocked = true;
		switching = false;
		return;
	  }

	  /* Destination is waiting for an interrupt.
	   * Send it a message with source HARDWARE and type HARD_INT.
	   * No more information can be reliably provided since interrupt messages
	   * are not queued.
	   */
	  rp->p_messbuf->_source = HARDWARE;
	  rp->p_messbuf->_type = MSG::HARD_INT;
	  rp->p_flags &= ~PSTATE::RECEIVING;
	  rp->p_int_blocked = false;

	  rp->_ready();
	  switching = false;
	}

	/*===========================================================================*
	 *				sys_call				     *
	 *===========================================================================*/
	int proc::sys_call(int function, int src_dest, message* m_ptr, size_t msg_size)
	//int function;			/* SEND, RECEIVE, or BOTH */
	//int src_dest;			/* source to receive from or dest to send to */
	//message *m_ptr;			/* pointer to message */
	{
	/* The only system calls that exist in MINIX are sending and receiving
	 * messages.  These are done by trapping to the kernel with an INT instruction.
	 * The trap is caught and sys_call() is called to send or receive a message
	 * (or both). The caller is always given by proc_ptr.
	 */

	  proc *rp;
	  int n;

	  /* Check for bad system call parameters. */
	  if (!isoksrc_dest(src_dest)) return(E_BAD_DEST);
	  rp = proc_ptr;

	  if (rp->isuser() && function != BOTH) return(E_NO_PERM);

	  /* The parameters are ok. Do the call. */
	  if (function & SEND) {
		/* Function = SEND or BOTH. */
		n = rp->_mini_send(src_dest, m_ptr,msg_size);
		if (function == SEND || n != OK)
			return(n);	/* done, or SEND failed */
	  }

	  /* Function = RECEIVE or BOTH.
	   * We have checked user calls are BOTH, and trust 'function' otherwise.
	   */
	  return (rp->_mini_rec(src_dest, m_ptr,msg_size));
	}
	/*==========================================================================*
	 *				cp_mess					    *
	 *==========================================================================*/
	void proc::cp_mess(int src,  proc *src_p, message *src_m,  proc *dst_p, message *dst_m, size_t msg_size)
	{
		  ::memcpy(dst_m, src_m, msg_size);
		  dst_m->_source = src;
		  dst_p->p_msgsize = msg_size;
	}

	/*===========================================================================*
	 *				mini_send				     *
	 *===========================================================================*/
	int proc::_mini_send(int dest, message *m_ptr, size_t msg_size)
	//register struct proc *caller_ptr;	/* who is trying to send a message? */
	//int dest;			/* to whom is message being sent? */
	//message *m_ptr;			/* pointer to message buffer */
	{
	/* Send a message from 'caller_ptr' to 'dest'. If 'dest' is blocked waiting
	 * for this message, copy the message to it and unblock 'dest'. If 'dest' is
	 * not waiting at all, or is waiting for another source, queue 'caller_ptr'.
	 */
	//	 proc *caller_ptr = this; // just for now

	  register struct proc *dest_ptr, *next_ptr;


	  /* User processes are only allowed to send to FS and MM.  Check for this. */
	  if (isuser() && !issysentn(dest)) return(E_BAD_DEST);
	  dest_ptr = proc_addr(dest);	/* pointer to destination's proc entry */
	  if (dest_ptr->isempty()) return(E_BAD_DEST);	/* dead dest */


	  /* Check for deadlock by 'caller_ptr' and 'dest' sending to each other. */
	  if(dest_ptr->p_flags % PSTATE::SENDING) {
		next_ptr = proc_addr(dest_ptr->p_sendto);
		while (true) {
			if (next_ptr == this) return(ELOCKED);
			if (next_ptr->p_flags % PSTATE::SENDING)
				next_ptr = proc_addr(next_ptr->p_sendto);
			else
				break;
		}
	  }

	  /* Check to see if 'dest' is blocked waiting for this message. */
	  if ( (dest_ptr->p_flags & (PSTATE::RECEIVING | PSTATE::SENDING)) == PSTATE::RECEIVING &&
	       (dest_ptr->p_getfrom == ANY ||
	        dest_ptr->p_getfrom == this->proc_number())) {
		/* Destination is indeed waiting for this message. */
		CopyMess(this->proc_number(), this, m_ptr, dest_ptr, dest_ptr->p_messbuf,msg_size);
		dest_ptr->p_flags &= ~PSTATE::RECEIVING;	/* deblock destination */
		if (dest_ptr->p_flags == PSTATE::UNBLOCKED) dest_ptr->_ready();
	  } else {
		/* Destination is not waiting.  Block and queue caller. */
		  this->p_messbuf = m_ptr;
		if (this->p_flags == PSTATE::UNBLOCKED) this->unready();
		this->p_flags |= PSTATE::SENDING;
		this->p_sendto= dest;

		/* Process is now blocked.  Put in on the destination's queue. */
		if ( (next_ptr = dest_ptr->p_callerq) == NIL_PROC)
			dest_ptr->p_callerq = this;
		else {
			while (next_ptr->p_sendlink != NIL_PROC)
				next_ptr = next_ptr->p_sendlink;
			next_ptr->p_sendlink = this;
		}
		this->p_sendlink = NIL_PROC;
	  }
	  return(OK);
	}

	/*===========================================================================*
	 *				mini_rec				     *
	 *===========================================================================*/
	int proc::_mini_rec(int src, message * m_ptr, size_t msg_size)
	//register struct proc *caller_ptr;	/* process trying to get message */
	//int src;			/* which message source is wanted (or ANY) */
	//message *m_ptr;			/* pointer to message buffer */
	{
	/* A process or task wants to get a message.  If one is already queued,
	 * acquire it and deblock the sender.  If no message from the desired source
	 * is available, block the caller.  No need to check parameters for validity.
	 * Users calls are always sendrec(), and mini_send() has checked already.
	 * Calls from the tasks, MM, and FS are trusted.
	 */

	   proc *sender_ptr;
	  proc *previous_ptr=nullptr;

	  /* Check to see if a message from desired source is already available. */
	  if (!(this->p_flags % PSTATE::SENDING)) {
		/* Check caller queue. */
	    for (sender_ptr = this->p_callerq; sender_ptr != NIL_PROC;
		 previous_ptr = sender_ptr, sender_ptr = sender_ptr->p_sendlink) {
		if (src == ANY || src == sender_ptr->proc_number()) {
			/* An acceptable message has been found. */
			CopyMess(sender_ptr->proc_number(), sender_ptr,
				 sender_ptr->p_messbuf, this, m_ptr,msg_size);
			if (sender_ptr == this->p_callerq)
				this->p_callerq = sender_ptr->p_sendlink;
			else
				previous_ptr->p_sendlink = sender_ptr->p_sendlink;
			if ((sender_ptr->p_flags &= ~PSTATE::SENDING) == PSTATE::UNBLOCKED)
				sender_ptr->ready();	/* deblock sender */
			return(OK);
		}
	    }

	    /* Check for blocked interrupt. */
	    if (this->p_int_blocked && isrxhardware(src)) {

		m_ptr->_source = HARDWARE;
		m_ptr->_type = MSG::HARD_INT;
		this->p_int_blocked = false;
		return(OK);
	    }
	  }

	  /* No suitable message is available.  Block the process trying to receive. */
	  this->p_getfrom = src;
	  this->p_messbuf = m_ptr;
	  if (this->p_flags == PSTATE::UNBLOCKED) this->unready();
	  this->p_flags |= PSTATE::RECEIVING;

	  /* If MM has just blocked and there are kernel signals pending, now is the
	   * time to tell MM about them, since it will be able to accept the message.
	   */
	  if (sig_procs > 0 && this->proc_number() == MM_PROC_NR && src == ANY)
		inform();
	  return(OK);
	}
	/*===========================================================================*
	 *				pick_proc				     *
	 *===========================================================================*/
	void proc::pick_proc()
	{
	/* Decide who to run now.  A new process is selected by setting 'proc_ptr'.
	 * When a fresh user (or idle) process is selected, record it in 'bill_ptr',
	 * so the clock task can tell who to bill for system time.
	 */

	  proc *rp;	/* process to run */

	  if ( (rp = &(*ready_queue[TASK_Q].begin())) != NIL_PROC) {
		proc_ptr = rp;
		return;
	  }
	  if ( (rp = &(*ready_queue[SERVER_Q].begin())) != NIL_PROC) {
		proc_ptr = rp;
		return;
	  }
	  if ( (rp = &(*ready_queue[USER_Q].begin())) != NIL_PROC) {
		proc_ptr = rp;
		bill_ptr = rp;
		return;
	  }
	  /* No one is ready.  Run the idle task.  The idle task might be made an
	   * always-ready user task to avoid this special case.
	   */
	  bill_ptr = proc_ptr = proc_addr(IDLE);
	}

	/*===========================================================================*
	 *				ready					     *
	 *===========================================================================*/
	void proc::_ready() {
		proc_queue_t& queue = istask()? ready_queue[TASK_Q] :
			  isserv() ? ready_queue[SERVER_Q] : ready_queue[USER_Q];
		if(p_link.tqe_next == PROC_MARK){
			p_link.tqe_next = nullptr;
			queue.push_back(this);
		}
	}
	/*===========================================================================*
	 *				unready					     *
	 *===========================================================================*/
	void proc::_unready()
	//register struct proc *rp;	/* this process is no longer runnable */
	{
	/* A process has blocked. */
		proc_queue_t& queue = istask()? ready_queue[TASK_Q] :
			  isserv() ? ready_queue[SERVER_Q] : ready_queue[USER_Q];
		if(istask() && *this->p_stguard != STACK_GUARD)
			panic("stack overrun by task", this->proc_number());
		if(queue.empty()) return;
		if(p_link.tqe_next != PROC_MARK){
			queue.remove(this);
			p_link.tqe_next = PROC_MARK;
		}
	}

	void proc::_sched() {
		/* The current process has run too long.  If another low priority (user)
		 * process is runnable, put the current process on the end of the user queue,
		 * possibly promoting another user to head of the queue.
		 */

		  if (ready_queue[USER_Q].empty()) return;
		  auto tmp = ready_queue[USER_Q].pop_back();
		  ready_queue[USER_Q].push_front(tmp);
		  pick_proc();
	}
	void proc::unhold() {
		/* Flush any held-up interrupts.  k_reenter must be 0.  held_head must not
		 * be NIL_PROC.  Interrupts must be disabled.  They will be enabled but will
		 * be disabled when this returns.
		 */

		  proc *rp;	/* current head of held queue */

		  if (switching) return;
		  rp = held_head;
		  do {
			if ( (held_head = rp->p_nextheld) == NIL_PROC) held_tail = NIL_PROC;
			rp->p_int_held = false;
			ARM::irq_disable(); /* reduce latency; held queue may change! */
			interrupt(rp->proc_number());
			ARM::irq_enable(); /* protect the held queue again */
		  }
		  while ( (rp = held_head) != NIL_PROC);
	}


#endif
}; /* namespace mimx */
