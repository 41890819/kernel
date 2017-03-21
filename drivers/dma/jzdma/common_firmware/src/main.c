/*
 * main.c
 */
#include <common.h>
#include <asm/jzsoc.h>
#include <pdma.h>
#include <delay.h>

#if defined(HANDLE_UART)
#include <uart.h>
#endif

static void handle_init(void)
{
#if defined(HANDLE_UART)
	uart_init();
#endif
}

static noinline void mcu_init(void)
{
	/* clear mailbox irq pending */
	REG32(PDMAC_DMNMB) = 0;
	REG32(PDMAC_DMSMB) = 0;
	REG32(PDMAC_DMINT) = PDMAC_DMINT_S_IMSK;

	/* clear irq mask for channel irq */
	REG32(PDMAC_DCIRQM) = 0;
}

static void mcu_sleep()
{
	__pdma_irq_disable();
	__pdma_mwait();
	__pdma_irq_enable();
}

int main(void)
{
	mcu_init();
	handle_init();

	while(1) {
		mcu_sleep();
	}

	return 0;
}
