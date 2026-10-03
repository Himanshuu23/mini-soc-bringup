#ifndef SOC_H
#define SOC_H

#define RAM_BASE   0x00000000
#define RAM_SIZE   0x00010000
#define UART_BASE  0x10000000
#define TIMER_BASE 0x10001000
#define DMA_BASE   0x10002000
#define SYS_BASE   0x10003000

#define UART_BAUD_DIV 16

#define IRQ_TIMER (1 << 0)
#define IRQ_DMA   (1 << 1)

#endif
