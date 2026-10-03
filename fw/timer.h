#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "soc.h"

typedef struct {
    volatile uint32_t count;
    volatile uint32_t compare;
    volatile uint32_t ctrl;
    volatile uint32_t status;
} timer_regs_t;

#define TIMER ((timer_regs_t *)TIMER_BASE)

#define TIMER_CTRL_ENABLE (1u << 0)
#define TIMER_CTRL_IRQ_EN (1u << 1)
#define TIMER_CTRL_RELOAD (1u << 2)
#define TIMER_STATUS_MATCH (1u << 0)

extern volatile uint32_t timer_irq_count;
extern volatile uint32_t timer_irq_limit;

void timer_stop(void);
void timer_start(uint32_t compare, uint32_t ctrl_flags);
void timer_isr(void);

#endif
