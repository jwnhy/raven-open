#include "sbi/riscv_asm.h"
#include "sbi/riscv_atomic.h"
#include "sbi/riscv_encoding.h"
#include "sbi/sbi_ipi.h"
#include "sbi_utils/rvbt/rvbt_stepping.h"
#include <sbi_utils/rvbt/rvbt_breakpoint.h>
#include <sbi_utils/rvbt/rvbt_memory.h>

struct rvbt_breakpoint_t rvbt_breakpoints[16];

int rvbt_set_data_point(uint64_t virt_addr, uint64_t log2size)
{
	int hartid		     = csr_read(CSR_MHARTID);
	struct rvbt_breakpoint_t *bp = rvbt_breakpoints;
	while (bp->enabled)
		bp++;
	bp->log2size		   = log2size;
	bp->type		   = DATA;
	bp->addr[hartid].virt_addr = virt_addr;
	bp->enabled		   = true;
	return 0;
}

int rvbt_set_inst_point(uint64_t virt_addr)
{
	int hartid		     = csr_read(CSR_MHARTID);
	struct rvbt_breakpoint_t *bp = rvbt_breakpoints;
	while (bp->enabled)
		bp++;
	bp->log2size		   = 2;
	bp->type		   = INST;
	bp->addr[hartid].virt_addr = virt_addr;
	bp->enabled		   = true;
	return 0;
}

int rvbt_update_breakpoint()
{
	int i, pmp_cnt = 0, hartid;
	uint64_t satp;
	struct rvbt_breakpoint_t *bp;
	struct rvbt_addr_pair_t *bp_addr;
	hartid = csr_read(CSR_MHARTID);
	for (int idx = 0; idx < 4; idx++) {
		pmp_set(idx, 0x0, 0x0, 0);
	}
	if (is_stepping[hartid])
		return 0;
	rvbt_clear_pmp();
	for (i = 0; i < 16; i++) {
		bp = &rvbt_breakpoints[i];
		if (!bp->enabled)
			continue;
		satp	= csr_read(CSR_SATP);
		bp_addr = &bp->addr[hartid];
		bp_addr->phys_addr =
			rvbt_mmu_translate(bp_addr->virt_addr, satp);
		bp_addr->mem_reg = rvbt_in_phys_mem((void *)bp_addr->phys_addr);
		if (bp_addr->mem_reg == NULL) {
			continue;
		}
		if (bp->type == DATA && bp->log2size != 2) {
			pmp_set(pmp_cnt++, PMP_A_NAPOT, bp_addr->phys_addr,
				bp->log2size);
		} else {
			pmp_set(pmp_cnt++, PMP_A_NA4 | PMP_R | PMP_W,
				bp_addr->phys_addr, bp->log2size);
		}
	}
	return 0;
}
