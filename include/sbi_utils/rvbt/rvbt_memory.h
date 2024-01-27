#ifndef __RVBT_MEMORY_H__
#define __RVBT_MEMORY_H__
#include "sbi/riscv_atomic.h"
#include "sbi/sbi_console.h"
#include "sbi/sbi_types.h"
#include "sbi_utils/fdt/fdt_helper.h"
#include <sbi/riscv_encoding.h>
#include <libfdt.h>

struct mem_reg_t {
	uint64_t base;
  uint64_t size;
};

struct riscv_satp_t {
  uint8_t mode: 4;
  uint16_t asid: 16;
  uint64_t ppn: 44;
}__attribute__((packed));

struct sv39_pte_t {
  bool valid: 1;
  bool readable: 1;
  bool writable: 1;
  bool executable: 1;
  bool user: 1;
  bool global: 1;
  bool access: 1;
  bool dirty: 1;
  uint8_t rsw: 2;
  uint64_t ppn: 44;
  uint64_t __unused: 10;
}__attribute__((packed));

extern struct riscv_satp_t val_to_satp(uint64_t satp_val);

extern uint64_t sv39_ppn_to_addr(uint64_t ppn);
extern uint64_t sv39_addr_to_ppn(uint64_t addr);
extern uint16_t sv39_addr_to_vpn(uint64_t addr, uint8_t level);
extern uint16_t sv39_addr_to_offset(uint64_t addr, uint8_t level);

void rvbt_detect_phys_mem(void *fdt);
struct mem_reg_t* rvbt_in_phys_mem(void *addr);
uint64_t rvbt_pageroot_translate(uint64_t virt_addr, uint64_t root_ppn);
uint64_t rvbt_mmu_translate(uint64_t virt_addr, uint64_t satp);
void rvbt_clear_pmp();

extern struct mem_reg_t mem_regs[64];
extern uint8_t mem_reg_cnt;
#endif
