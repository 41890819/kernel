#include <common.h>
#include <tcsm.h>
#include <uart.h>
#include <intc.h>

// volatile unsigned int *debug = (volatile unsigned int *)TCSM_UART_DEBUG;
void uart_init(void)
{
	*(volatile unsigned int *)TCSM_UART_TBUF_COUNT = 0;
	*(volatile unsigned int *)TCSM_UART_TBUF_WP = 0;
	*(volatile unsigned int *)TCSM_UART_TBUF_RP = 0;

	*(volatile unsigned int *)TCSM_UART_RBUF_COUNT = 0;
	*(volatile unsigned int *)TCSM_UART_RBUF_WP = 0;
	*(volatile unsigned int *)TCSM_UART_RBUF_RP = 0;
}

int handle_uart_irq(void)
{
	struct mailbox_pend_addr_s *mailbox_pend_addr = (struct mailbox_pend_addr_s *)MAILBOX_PEND_ADDR;
	unsigned int dpr1 = INTC_REG32(INTC_DPR1);
	mailbox_pend_addr->uart[0] = 0;

	if (dpr1 & INTC_UART0) { // UART0 interrupt
		unsigned int iir = UART_REG32(UART0_IIR);
		unsigned int lsr = UART_REG32(UART0_LSR);

		if (iir & IIR_NO_INT)
			return 0;

		if (lsr & LSR_DRY) { // read
			volatile unsigned char *buf = (volatile unsigned char *)TCSM_UART_RBUF_ADDR;
			unsigned int waddr = *(volatile unsigned int *)TCSM_UART_RBUF_WP;
			unsigned int count = *(volatile unsigned int *)TCSM_UART_RBUF_COUNT;

			unsigned char status = UART_REG8(UART0_LSR);
			while ((status & LSR_DRY) && (count < TCSM_UART_BUF_LEN)) {
				buf[waddr] = UART_REG8(UART0_RBR); // data
				buf[waddr + 1] = status; // flag
				waddr = (waddr + 2) & (TCSM_UART_BUF_LEN - 1);
				count = count + 2;
				status = UART_REG8(UART0_LSR);
			}

			*(volatile unsigned int *)TCSM_UART_RBUF_COUNT = count;
			*(volatile unsigned int *)TCSM_UART_RBUF_WP = waddr;

			if (count > (TCSM_UART_BUF_LEN - 64)) // full data int to CPU
				mailbox_pend_addr->uart[0] |= TCSM_UART_NEED_READ;

			if ((iir & IIR_INID) == IIR_RECEIVE_TIMEOUT)  // read over, send int to CPU
				mailbox_pend_addr->uart[0] |= TCSM_UART_NEED_READ;
		}

		if (lsr & LSR_TDRQ) { // trans
			volatile unsigned char *buf = (volatile unsigned char *)TCSM_UART_TBUF_ADDR;
			unsigned int raddr = *(volatile unsigned int *)TCSM_UART_TBUF_RP;
			unsigned int count = *(volatile unsigned int *)TCSM_UART_TBUF_COUNT;

			int i = 0;
			while ( count != 0 && i < 32 ) {
				UART_REG8(UART0_THR) = buf[raddr];
				i++;
				count--;
				raddr = (raddr + 1) & (TCSM_UART_BUF_LEN - 1);
			}

			*(volatile unsigned int *)TCSM_UART_TBUF_RP = raddr;
			*(volatile unsigned int *)TCSM_UART_TBUF_COUNT = count;

			if (count < 32)
				mailbox_pend_addr->uart[0] |= TCSM_UART_NEED_WRITE; // data request int to cpu
		}

		if (mailbox_pend_addr->uart[0])
			return 1;
	}

	return 0;
}
