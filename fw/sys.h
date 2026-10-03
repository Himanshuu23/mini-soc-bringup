#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include "soc.h"

typedef struct {
    volatile uint32_t exit_code;
} sys_regs_t;

#define SYS ((sys_regs_t *)SYS_BASE)

void sys_exit(int code) __attribute__((noreturn));

#endif
