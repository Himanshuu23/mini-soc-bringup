#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include "soc.h"

typedef struct {
    volatile uint32_t src;
    volatile uint32_t dst;
    volatile uint32_t len;
    volatile uint32_t ctrl;
    volatile uint32_t status;
} dma_regs_t;

#define DMA ((dma_regs_t *)DMA_BASE)

#define DMA_CTRL_START (1u << 0)
#define DMA_CTRL_IRQ_EN (1u << 1)
#define DMA_STATUS_BUSY (1u << 0)
#define DMA_STATUS_DONE (1u << 1)

extern volatile uint32_t dma_irq_count;

void dma_start(const void *src, void *dst, uint32_t words, int irq);
int dma_wait(uint32_t max_polls);
void dma_isr(void);

#endif
