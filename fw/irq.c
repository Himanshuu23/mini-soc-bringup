#include "irq.h"
#include "sys.h"
#include "uart.h"

void irq_handler(uint32_t pending)
{
    uart_puts("unexpected irq 0x");
    uart_put_hex(pending);
    uart_putc('\n');
    sys_exit(3);
}
