#ifndef __RVBT_INIT_H__
#define __RVBT_INIT_H__
#include "sbi/sbi_trap.h"

void rvbt_init(const void* fdt);
int rvbt_loop(struct sbi_trap_regs* regs);

#endif
