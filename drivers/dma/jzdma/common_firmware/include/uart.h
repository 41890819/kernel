#ifndef __UART_H__
#define __UART_H__

#define UART0_BASE            0x10030000
#define UART_REG32(addr)      (*(volatile unsigned int *)(addr))
#define UART_REG8(addr)      (*(volatile unsigned char *)(addr))

#define UART0_RBR             (UART0_BASE + 0x00)
#define UART0_THR             (UART0_BASE + 0x00)
#define UART0_DLLR            (UART0_BASE + 0x00)
#define UART0_DLHR            (UART0_BASE + 0x04)
#define UART0_IER             (UART0_BASE + 0x04)
#define UART0_IIR             (UART0_BASE + 0x08)
#define UART0_FCR             (UART0_BASE + 0x08)
#define UART0_LCR             (UART0_BASE + 0x0C)
#define UART0_MCR             (UART0_BASE + 0x10)
#define UART0_LSR             (UART0_BASE + 0x14)
#define UART0_MSR             (UART0_BASE + 0x18)
#define UART0_SPR             (UART0_BASE + 0x1C)
#define UART0_ISR             (UART0_BASE + 0x20)
#define UART0_UMR             (UART0_BASE + 0x24)
#define UART0_ACR             (UART0_BASE + 0x28)
#define UART0_RCR             (UART0_BASE + 0x40)
#define UART0_TCR             (UART0_BASE + 0x44)

#define IIR_NO_INT            (0x1 << 0)
#define IIR_INID              (0x7 << 1)
#define IIR_RECEIVE_TIMEOUT   (0x6 << 1)

#define LSR_TDRQ              (0x1 << 5)
#define LSR_DRY               (0x1 << 0)

void uart_init(void);
int handle_uart_irq(void);

#endif
