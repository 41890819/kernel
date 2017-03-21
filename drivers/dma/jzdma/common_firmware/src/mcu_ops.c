/*
 * mcu_ops.c  :  IRQ handler
 */

# include <common.h>
# include <asm/jzsoc.h>
# include <pdma.h>
# include <gpio.h>

#if defined(HANDLE_UART)
# include <uart.h>
#endif

void send_mailbox(void)
{
	REG32(PDMAC_DMNMB) = 0xFFFFFFFF; // Create an normal mailbox irq to CPU
	while(REG32(PDMAC_DMINT) & PDMAC_DMINT_N_IP); // wait for CPU handle over
}

void trap_entry(void)
{
	unsigned int intctl;
	unsigned int need_send_mailbox = 0;
	
	__pdma_irq_disable();
	intctl = __pdma_read_cp0(12, 1);
	
	// Handler irq
	if (intctl & MCU_INTC_IRQ) {
#if defined(HANDLE_UART)
		need_send_mailbox += handle_uart_irq();
#endif
	}

	// Send irq to main cpu
	if (need_send_mailbox)
		send_mailbox();

	__pdma_irq_enable();
}

