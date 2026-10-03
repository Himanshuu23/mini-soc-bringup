#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "soc.h"

typedef struct {
    volatile uint32_t data;
    volatile uint32_t status;
    volatile uint32_t baud;
} uart_regs_t;

#define UART ((uart_regs_t *)UART_BASE)

#define UART_STATUS_TX_BUSY  (1u << 0)
#define UART_STATUS_RX_VALID (1u << 1)

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
void uart_put_hex(uint32_t value);
void uart_put_dec(uint32_t value);

#endif
