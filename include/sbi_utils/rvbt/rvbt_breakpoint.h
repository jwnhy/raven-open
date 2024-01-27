#ifndef __RVBT_BREAKPOINT_H__
#define __RVBT_BREAKPOINT_H__
#define SFENCE_VM 0x12000073
#define SATP_ACCESS 0x18000073
#define SFENCE_VM_MASK 0xfe007fff
#define SATP_ACCESS_MASK 0xfff0007f

#include <sbi_utils/rvbt/rvbt_memory.h>
#include <sbi/riscv_asm.h>

enum rvbt_bp_type_t {
	INST,
	DATA,
};

struct rvbt_addr_pair_t{
  uint64_t virt_addr;
  uint64_t phys_addr;
	struct mem_reg_t *mem_reg;
};

struct rvbt_breakpoint_t {
	uint64_t log2size;
  struct rvbt_addr_pair_t addr[16];
	enum rvbt_bp_type_t type;
  bool enabled;
};

int rvbt_update_breakpoint();
int rvbt_set_inst_point(uint64_t virt_addr);
int rvbt_set_data_point(uint64_t virt_addr, uint64_t log2size);
#endif
