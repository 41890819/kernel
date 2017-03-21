#ifndef __TIMER_H___
#define __TIMER_H___




struct wakeup_timer {
	int null;

};

int request_wakeup_timer(struct wakeup_timer *timer);

int timer_irq_handler(struct wakeup_timer *timer);

int mod_wakeup_timer(struct wakeup_timer *timer, int ms);


void stoptimer(void);
int get_timer_count(struct wakeup_timer *timer);


unsigned long ms_to_count(unsigned long ms);
#endif
