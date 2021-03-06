/*
 * ktimer.cpp
 *
 *  Created on: Apr 30, 2017
 *      Author: warlo
 */

#include <os/ktimer.hpp>
#include "bitmap.hpp"
#include <stm32f7xx.h>


#define dbg_printf(VAL,...) trace_printf(__VA_ARGS__)
namespace os {
using ktimer_head_t = list::head<ktimer_event_t,&ktimer_event_t::link>;
#define CONFIG_MAX_KT_EVENTS 30
	bitops::bitmap_table_t<ktimer_event_t,CONFIG_MAX_KT_EVENTS> ktimer_event_table;
static bool ktimer_enabled = false;

//DECLARE_KTABLE(ktimer_event_t, ktimer_event_table, CONFIG_MAX_KT_EVENTS);

/* Next chain of events which will be executed */
ktimer_head_t event_queue;
static uint32_t ktimer_delta = 0;
static uint32_t ktimer_time = 0;
static uint32_t ktimer_now = 0;
static void init_systick(uint32_t tick_reload, uint32_t tick_next_reload)
{
	/* 250us at 168Mhz */
	SysTick->LOAD = tick_reload - 1;
	SysTick->VAL = 0;
	SysTick->CTRL = 0x00000007;

	if (tick_next_reload)
		SysTick->LOAD = tick_next_reload - 1;
}
static void systick_disable(){ SysTick->CTRL = 0x00000000; }
static uint32_t systick_now() { return SysTick->VAL;}
static  uint32_t systick_flag_count() { return (SysTick->CTRL & (1 << 16)) >> 16; }
static void ktimer_init(void) { init_systick(os::CONFIG_KTIMER_HEARTBEAT, 0); }

static void ktimer_disable(void) { if (ktimer_enabled) ktimer_enabled = true; }

static void ktimer_enable(uint32_t delta)
{
	if (!ktimer_enabled) {
		ktimer_delta = delta;
		ktimer_time = 0;
		ktimer_enabled = true;

#if defined(CONFIG_KDB) && \
    defined(CONFIG_KTIMER_TICKLESS) && defined(CONFIG_KTIMER_TICKLESS_VERIFY)
		tickless_verify_start(ktimer_now, ktimer_delta);
#endif	/* CONFIG_KDB */
	}
}





static void ktimer_event_recalc(ktimer_event_t& event, uint32_t new_delta)
{
		dbg_printf(DL_KTIMER,
		           "KTE: Recalculated event %p D=%d -> %d\n",
		           &event, event.delta, event.delta - new_delta);
		event.delta -= new_delta;
}

int ktimer_event_schedule(uint32_t ticks, ktimer_event_t *kte)
{
	long etime = 0, delta = 0;

	if (!ticks)
		return -1;
	ticks -= ktimer_time;

	if (event_queue.empty()) {
		/* All other events are already handled, so simply schedule
		 * and enable timer
		 */
		dbg_printf(DL_KTIMER,
		           "KTE: Scheduled dummy event %p on %d\n",
		           kte, ticks);

		kte->delta = ticks;

		event_queue.push_front(kte);
		ktimer_enable(ticks);
	} else {
		/* etime is total delta for event from now (-ktimer_value())
		 * on each iteration we add delta between events.
		 *
		 * Search for event chain until etime is larger than ticks
		 * e.g ticks = 80
		 *
		 * 0---17------------60----------------60---...
		 *                   ^                 ^
		 *                   |           (etime + next_event->delta) =
		 *                   |           = 120 - 17 = 103
		 *                               etime = 60 - 17 =  43
		 *
		 * kte is between event(60) and event(120),
		 * delta = 80 - 43 = 37
		 * insert and recalculate:
		 *
		 * 0---17------------60-------37-------23---...
		 *
		 * */
		auto next_event = event_queue.begin();
		if (ticks < next_event->delta) {
			/* Event should be scheduled before earlier event */
			event_queue.push_front(kte);
			delta = ticks;
			dbg_printf(DL_KTIMER,
				   "KTE: Scheduled early event %p with T=%d\n",
				   kte, ticks);

			/* Reset timer */
			ktimer_enable(ticks);
		} else {
			decltype(next_event) prev;
			do {
				prev = next_event;
				etime += prev->delta;
				delta = ticks - etime;
				++next_event;
			} while (next_event != event_queue.end() &&
			         ticks > (etime + next_event->delta));

			dbg_printf(DL_KTIMER,
			           "KTE: Scheduled event %p [%p:%p] with "
			           "D=%d and T=%d\n",
			           kte, &prev, &next_event, delta, ticks);

			event_queue.insert_after(prev, kte);
		}

		/* Chaining events */
		if (delta < (int)os::CONFIG_KTIMER_MINTICKS) delta = 0;

		//kte->next = next_event;
		kte->delta = delta;

		ktimer_event_recalc(*next_event, delta);
	}

	return 0;
}
ktimer_event_t *ktimer_event_create(uint32_t ticks,
	                                ktimer_event_handler_t handler,
	                                void *data)
{
	ktimer_event_t *kte = NULL;

	if (!handler)
		goto ret;

	kte = ktimer_event_table.construct();


	/* No available slots */
	if (kte == NULL)
		goto ret;

	kte->handler = handler;
	kte->data = data;

	if (ktimer_event_schedule(ticks, kte) == -1) {
		ktimer_event_table.destroy(kte);
		kte = NULL;
	}

ret:
	return kte;
}


void ktimer_event_handler()
{
	auto event = event_queue.begin();
	decltype(event) last_event ;
	decltype(event)  next_event ;

	if (event_queue.empty()) {
		/* That is bug if we are here */
		dbg_printf(DL_KTIMER, "KTE: OOPS! handler found no events\n");

		ktimer_disable();
		return;
	}

	/* Search last event in event chain */
	do {
		++event;
	} while (event !=event_queue.end() && event->delta == 0);

	last_event = event;
	//event = event_queue.begin();
	//	event_queue = last_event;
	/* All rescheduled events will be scheduled after last event */
	event =event_queue.begin();
	/* walk chain */
	do {
		next_event = event_queue.erase(event);
		uint32_t h_retvalue = event->handler(*event);
		if (h_retvalue != 0x0) {
			dbg_printf(DL_KTIMER,
			           "KTE: Handled and rescheduled event %p @%ld\n",
			           event, ktimer_now);
			ktimer_event_schedule(h_retvalue, &(*event));
		} else {
			dbg_printf(DL_KTIMER,
			           "KTE: Handled event %p @%ld\n",
			           event, ktimer_now);
			ktimer_event_table.destroy(&(*event));
		}
		event=next_event;
	} while(event != last_event);

	if (!event_queue.empty()) {
		/* Reset ktimer */
		ktimer_enable(event_queue.front().delta);
	}
}




#ifdef CONFIG_KTIMER_TICKLESS

#define KTIMER_MAXTICKS (SYSTICK_MAXRELOAD / CONFIG_KTIMER_HEARTBEAT)

static uint32_t volatile ktimer_tickless_compensation = CONFIG_KTIMER_TICKLESS_COMPENSATION;
static uint32_t volatile ktimer_tickless_int_compensation = CONFIG_KTIMER_TICKLESS_INT_COMPENSATION;

void ktimer_enter_tickless()
{
	uint32_t tickless_delta;
	uint32_t reload;

	irq_disable();

	if (ktimer_enabled && ktimer_delta <= KTIMER_MAXTICKS) {
		tickless_delta = ktimer_delta;
	} else {
		tickless_delta = KTIMER_MAXTICKS;
	}

	/* Minus 1 for current value */
	tickless_delta -= 1;

	reload = CONFIG_KTIMER_HEARTBEAT * tickless_delta;

	reload += systick_now() - ktimer_tickless_compensation;

	if (reload > 2) {
		init_systick(reload, CONFIG_KTIMER_HEARTBEAT);

#if defined(CONFIG_KDB) && \
    defined(CONFIG_KTIMER_TICKLESS) && defined(CONFIG_KTIMER_TICKLESS_VERIFY)
		tickless_verify_count();
#endif
	}

	wait_for_interrupt();

	if (!systick_flag_count()) {
		uint32_t tickless_rest = (systick_now() / CONFIG_KTIMER_HEARTBEAT);

		if (tickless_rest > 0) {
			int reload_overflow;

			tickless_delta = tickless_delta - tickless_rest;

			reload = systick_now() % CONFIG_KTIMER_HEARTBEAT - ktimer_tickless_int_compensation;
			reload_overflow = reload < 0;
			reload += reload_overflow * CONFIG_KTIMER_HEARTBEAT;

			init_systick(reload, CONFIG_KTIMER_HEARTBEAT);

			if (reload_overflow) {
				tickless_delta++;
			}

#if defined(CONFIG_KDB) && \
    defined(CONFIG_KTIMER_TICKLESS) && defined(CONFIG_KTIMER_TICKLESS_VERIFY)
			tickless_verify_count_int();
#endif
		}
	}

	ktimer_time += tickless_delta;
	ktimer_delta -= tickless_delta;
	ktimer_now += tickless_delta;

	irq_enable();
}
#endif /* CONFIG_KTIMER_TICKLESS */

};
#if 0
extern "C" void SysTick_Handler(void)
{
	++os::ktimer_now;

	if (os::ktimer_enabled && os::ktimer_delta > 0) {
		++os::ktimer_time;
		--os::ktimer_delta;

		if (os::ktimer_delta == 0) {
			os::ktimer_enabled = 0;
			os::ktimer_time = os::ktimer_delta = 0;

#if defined(CONFIG_KDB) && \
    defined(CONFIG_KTIMER_TICKLESS) && defined(CONFIG_KTIMER_TICKLESS_VERIFY)
			tickless_verify_stop(ktimer_now);
#endif	/* CONFIG_KDB */

		//	softirq_schedule(KTE_SOFTIRQ);
		}
	}
}
#endif
