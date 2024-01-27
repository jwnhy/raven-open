#include "sbi/riscv_asm.h"
#include "sbi/sbi_console.h"
#include "sbi/sbi_ipi.h"
#include "sbi/sbi_string.h"
#include "sbi_utils/rvbt/rvbt_memory.h"
#include "sbi_utils/rvbt/mfmt.h"
#include "sbi_utils/rvbt/rvbt_breakpoint.h"
#include "sbi_utils/rvbt/rvbt_serial.h"
#include "sbi_utils/rvbt/rvbt_stepping.h"
#include "sbi/sbi_ipi.h"
#include "sbi/sbi_trap.h"
static bool is_continuing[16];

void rvbt_init(void *fdt)
{
	rvbt_detect_phys_mem(fdt);
  rvbt_set_inst_point(0x80202000);
  rvbt_update_breakpoint();
	rvbt_serial_init();
}

int rvbt_loop(struct sbi_trap_regs *regs)
{
	char cmd[20];
  char param[20];
	int hartid = csr_read(CSR_MHARTID);
	char *input;
	uint64_t virt_addr = 0, phys_addr = 0;
	uint64_t satp_val = csr_read(CSR_SATP);
	if (is_continuing[hartid]) {
		is_continuing[hartid] = false;
		is_stepping[hartid]   = false;
		rvbt_update_breakpoint();
		return 0;
	}
	sbi_printf("At 0x%lx 0x%lx\n", regs->mepc,
		   rvbt_mmu_translate(regs->mepc, satp_val));
	while (true) {
		sbi_printf("[Raven]: Input command:");
		input = rvbt_gets();
		mfmt_scan(input, "%s %s", cmd, param);
		if (!sbi_strcmp(cmd, "s")) {
			if (rvbt_stepping(regs))
				sbi_printf(
					"[Raven]: Single stepping failed with phys_addr: %lx\n",
					regs->mepc);
			return 0;
    } else if (!sbi_strcmp(cmd, "csrr")) {
      if (!sbi_strcmp(param, "$stval"))
        sbi_printf("$stval: %lx\n", csr_read(CSR_STVAL));
      else if (!sbi_strcmp(param, "$sepc"))
        sbi_printf("$spec: %lx\n", csr_read(CSR_SEPC));
      else if (!sbi_strcmp(param, "$scause"))
        sbi_printf("$scause: %lu\n", csr_read(CSR_SCAUSE));
      else if (!sbi_strcmp(param, "$stvec"))
        sbi_printf("$stvec: %lx\n", csr_read(CSR_STVEC));
    } else if (!sbi_strcmp(cmd, "pr")) {
		  mfmt_scan(param, "%x", &virt_addr);
			phys_addr = rvbt_mmu_translate(virt_addr, satp_val);
			if (!rvbt_in_phys_mem((void *)phys_addr)) {
				sbi_printf(
					"[Raven]: Not in range virt: 0x%lx, phys: 0x%lx\n",
					virt_addr, phys_addr);
      }
			sbi_printf("[Raven]: *(0x%lx)=0x%x\n", virt_addr,
				   *(uint16_t *)phys_addr);
    } else if (!sbi_strcmp(cmd, "map")) {
		  mfmt_scan(param, "%x", &virt_addr);
			phys_addr = rvbt_mmu_translate(virt_addr, satp_val);
      sbi_printf("[Raven]: Map of virtual address 0x%lx is 0x%lx\n", virt_addr, phys_addr);
    } else if (!sbi_strcmp(cmd, "rr")) {
      if (!sbi_strcmp(param, "a0"))
        sbi_printf("[Raven]: $a0: %lx\n", regs->a0);
      if (!sbi_strcmp(param, "a1"))
        sbi_printf("[Raven]: $a1: %lx\n", regs->a1);
      if (!sbi_strcmp(param, "a2"))
        sbi_printf("[Raven]: $a2: %lx\n", regs->a2);
    }
    else if (!sbi_strcmp(cmd, "b")) {
		  mfmt_scan(param, "%x", &virt_addr);
			rvbt_set_inst_point(virt_addr);
			rvbt_update_breakpoint();
		} else if (!sbi_strcmp(cmd, "c")) {
			rvbt_stepping(regs);
			is_continuing[hartid] = true;
			rvbt_update_breakpoint();
			return 0;
		} else {
			sbi_printf(
				"[Raven]: Don't understand what you are saying.\n");
		}
	}
}
