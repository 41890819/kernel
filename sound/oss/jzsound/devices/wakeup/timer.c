#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/notifier.h>
#include <linux/cpu.h>

#include <soc/base.h>
#include <soc/extal.h>
#include <soc/tcu.h>
#include <soc/irq.h>
#include <mach/jztcu.h>

#include "timer.h"

#define CLK_DIV			64
#define CSRDIV(x)      ({int n = 0;int d = x; while(d){ d >>= 2;n++;};(n-1) << 3;})

#define tcu_readl(reg)          inl(TCU_IOBASE + reg)
#define tcu_writel(reg,value)   outl(value, TCU_IOBASE + reg)


#define TCU_CHAN2	2



void dump_reg(void)
{

	printk("TCU_TCSR:%x\n", tcu_readl(CH_TCSR(TCU_CHAN2)));
	printk("TCU_TCNT:%x\n", tcu_readl(CH_TCNT(TCU_CHAN2)));
	printk("TCU_TER:%x\n", tcu_readl(TCU_TER));
	printk("TCU_TFR:%x\n", tcu_readl(TCU_TFR));
	printk("TCU_TMR:%x\n", tcu_readl(TCU_TMR));
	printk("TCU_TSR:%x\n", tcu_readl(TCU_TSR));
	printk("TCU_TSTR:%x\n", tcu_readl(TCU_TSTR));

}

void stoptimer(void)
{
	tcu_writel(TCU_TECR , (1 << TCU_CHAN2));
	tcu_writel(TCU_TFCR , (1 << TCU_CHAN2));
	tcu_writel(TCU_TSSR , (1 << TCU_CHAN2));

}
void restarttimer(void)
{
	tcu_writel(TCU_TFCR , (1 << TCU_CHAN2));
	tcu_writel(TCU_TESR , (1 << TCU_CHAN2));

}
static inline void resettimer(int count)
{
	unsigned int tcsr = tcu_readl(CH_TCSR(TCU_CHAN2));
	tcu_writel(CH_TDFR(TCU_CHAN2),count);
	//tcu_writel(CH_TCNT(TCU_CHAN2),0);
	//printk("######TCNT:%d, :TCSR:%x\n", tcu_readl(CH_TCNT(TCU_CHAN2)), tcu_readl(CH_TCSR(TCU_CHAN2)));
	//dump_reg();
	tcu_writel(CH_TCSR(TCU_CHAN2), tcsr | (1<<10));
	//dump_reg();
	//printk("######TCNT:%d, :TCSR:%x\n", tcu_readl(CH_TCNT(TCU_CHAN2)), tcu_readl(CH_TCSR(TCU_CHAN2)));
	tcu_writel(TCU_TMCR , (1 << TCU_CHAN2));
	tcu_writel(TCU_TFCR , (1 << TCU_CHAN2));
	tcu_writel(TCU_TESR , (1 << TCU_CHAN2));

	//tcu_writel(TCU_TSCR , (1 << TCU_CHAN2));

}


unsigned long ms_to_count(unsigned long ms)
{
	unsigned long count;

	count = (ms * 1000 + 1953 - 1) / 1953;
	//count =  (ms * 1000 + 3125 - 1) / 3125;
	//count = (ms * 1000 + 976 - 1) / 976;
	return count;
}

int mod_wakeup_timer(struct wakeup_timer *timer, int timer_count)
{
	int count;
	int current_count;
	//count = ms_to_count(ms);
	count = timer_count;

	//printk("############################ mod wakeup_timer !!!!!!!!\n");

	current_count = tcu_readl(CH_TCNT(TCU_CHAN2));
	//stoptimer();
	tcu_writel(TCU_TSCR , (1 << TCU_CHAN2));
	resettimer(count);


	//printk("current_count:%d\n", current_count);
	return current_count;
	//printk("TCU TDFR:%d\n", tcu_readl(CH_TDFR(TCU_CHAN2)));
	//printk("TCU TCNT:%d\n", tcu_readl(CH_TCNT(TCU_CHAN2)));
}


int timer_irq_handler(struct wakeup_timer *timer)
{
	int ctrlbit = 1 << (TCU_CHAN2);

	if(tcu_readl(TCU_TFR) & ctrlbit) {
		tcu_writel(TCU_TFCR,ctrlbit);
		//stoptimer();
		return ctrlbit;
	}
	return 0;
}

static int config_wakeup_timer(void)
{
	/* RTC CLK, 32768
	 * DIV:		64.
	 * TCOUNT:	1: 1.953125ms
	 * */
	tcu_writel(CH_TCSR(TCU_CHAN2),CSRDIV(CLK_DIV) | CSR_RTC_EN);

	return 0;
}


int request_wakeup_timer(struct wakeup_timer *timer)
{
	/* get one tcu channel, never released */
	struct clk *clk;
	tcu_request(TCU_CHAN2, NULL);
	clk = clk_get(NULL, "tcu");

	clk_enable(clk);

	tcu_writel(TCU_TSCR,(1 << TCU_CHAN2));
	tcu_writel(TCU_TMSR,(1 << TCU_CHAN2)|(1 << (TCU_CHAN2 + 16)));
	tcu_writel(CH_TDHR(TCU_CHAN2), 0xffff);

	config_wakeup_timer();

	return 0;
}
