#include "sbi/riscv_asm.h"
#include "sbi/riscv_atomic.h"
#include "sbi/riscv_encoding.h"
#include <sbi_utils/rvbt/rvbt_memory.h>

#define MASK_OFFSET 0xfff
#define MASK_L0 0x1ff000
#define MASK_L1 0x3fe00000
#define MASK_L2 0x7fc0000000

struct mem_reg_t mem_regs[64];
uint8_t mem_reg_cnt = 0;


inline struct riscv_satp_t val_to_satp(uint64_t satp_val) {
  struct riscv_satp_t satp = {
    .mode = (satp_val & SATP64_MODE) >> 60,
    .asid = (satp_val & SATP64_ASID) >> 44,
    .ppn = satp_val & SATP64_PPN
  };
  return satp;
}

inline uint64_t sv39_ppn_to_addr(uint64_t ppn)
{
	return ppn << 12;
}

inline uint64_t sv39_addr_to_ppn(uint64_t addr)
{
	return addr >> 12;
}

inline uint16_t sv39_addr_to_vpn(uint64_t addr, uint8_t level)
{
  switch(level) {
    case 0:
      addr = addr & MASK_L0;
      break;
    case 1:
      addr = addr & MASK_L1;
      break;
    case 2:
      addr = addr & MASK_L2;
      break;
  }
  return (addr >> 12) >> (level * 9);
}

inline uint16_t sv39_addr_to_offset(uint64_t addr, uint8_t level)
{
  return addr & MASK_OFFSET;
}

static const void *get_prop(const void *fdt, const char *node_path,
			    const char *property, int minlen)
{
	const void *prop;
	int offset, len;

	offset = fdt_path_offset(fdt, node_path);
	if (offset < 0)
		return NULL;

	prop = fdt_getprop(fdt, offset, property, &len);
	if (!prop || len < minlen)
		return NULL;

	return prop;
}

static uint64_t get_val(const fdt32_t *cells, uint32_t ncells)
{
	uint64_t r;

	r = fdt32_ld(cells);
	if (ncells > 1)
		r = (r << 32) | fdt32_ld(cells + 1);

	return r;
}

static uint32_t get_cells(const void *fdt, const char *name)
{
	const fdt32_t *prop = get_prop(fdt, "/", name, sizeof(fdt32_t));

	if (!prop) {
		/* default */
		return 1;
	}

	return fdt32_ld(prop);
}

void rvbt_detect_phys_mem(void *fdt)
{
	uint32_t addr_cells, size_cells;
	uint64_t base, size;
	const fdt32_t *reg, *endp;
	const char *type;
	int len, err, offset;

	addr_cells = get_cells(fdt, "#address-cells");
	size_cells = get_cells(fdt, "#size-cells");
	if (addr_cells > 2 || size_cells > 2)
		return;

	err = fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + 32);
	if (err < 0)
		return;

	for (offset = fdt_next_node(fdt, -1, NULL); offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		type = fdt_getprop(fdt, offset, "device_type", NULL);
		if (!type || strcmp(type, "memory"))
			continue;
		reg = fdt_getprop(fdt, offset, "reg", &len);
		if (!reg || !len)
			continue;

		for (endp = reg + (len / sizeof(fdt32_t));
		     endp - reg >= addr_cells + size_cells;
		     reg += addr_cells + size_cells) {
			size = get_val(reg + addr_cells, size_cells);
			base = fdt32_ld(reg + addr_cells - 1);
			sbi_printf(
				"[Raven] Detected physical memory, Base: %lx, Size: %lx\n",
				base, size);
			struct mem_reg_t mem_reg = { .base = base,
						     .size = size };
			mem_regs[mem_reg_cnt++]	 = mem_reg;
		}
	}
}

struct mem_reg_t *rvbt_in_phys_mem(void *addr)
{
	int idx;
	uint64_t base, size, addr_val = (uint64_t)addr;
	for (idx = 0; idx < mem_reg_cnt; idx++) {
		base = mem_regs[idx].base;
		size = mem_regs[idx].size;
		if (base <= addr_val && addr_val < base + size) {
			return &mem_regs[idx];
		}
	}
	return NULL;
}

uint64_t rvbt_pageroot_translate(uint64_t virt_addr, uint64_t root_ppn)
{
	int level;
	uint64_t vpn, offset, phys_addr, offset_mask;
	struct sv39_pte_t pte;
	struct sv39_pte_t *page_table =
		(struct sv39_pte_t *)sv39_ppn_to_addr(root_ppn);
	for (level = 2; level >= 0; level--) {
		vpn = sv39_addr_to_vpn(virt_addr, level);
		pte = page_table[vpn];
		if (pte.valid == 0)
			return -1;
		if (pte.readable || pte.writable || pte.access) {
			offset_mask = (1 << (12 + level * 9)) - 1;
			offset	    = virt_addr & offset_mask;
			phys_addr   = (pte.ppn << 12) & ~offset_mask;
			phys_addr   = phys_addr | offset;
      return phys_addr;
		}
		page_table = (struct sv39_pte_t *)sv39_ppn_to_addr(pte.ppn);
		if (!rvbt_in_phys_mem((void *)page_table)) {
			sbi_printf(
				"[RVBT]: MMU Translate fail: page_table: %lx, virt_addr: %lx",
				(uint64_t)page_table, virt_addr);
			return -1;
		}
	}
	return -1;
}


uint64_t rvbt_mmu_translate(uint64_t virt_addr, uint64_t satp_val)
{
	struct riscv_satp_t satp = val_to_satp(satp_val);
	if (satp.mode == SATP_MODE_OFF)
		return virt_addr;
	if (satp.mode != SATP_MODE_SV39)
		return -1;
	return rvbt_pageroot_translate(virt_addr, satp.ppn);
}

void rvbt_clear_pmp() {
	int pmpcfg_csr, pmpcfg_shift, pmpaddr_csr;
  unsigned long cfgmask, pmpcfg;
  for (int n = 0; n < 4; n++) {
    pmpcfg_csr   = (CSR_PMPCFG0 + (n >> 2)) & ~1;
	  pmpcfg_shift = (n & 7) << 3;
	  pmpaddr_csr = CSR_PMPADDR0 + n;
	  cfgmask = ~(0xffUL << pmpcfg_shift);
    pmpcfg = (csr_read_num(pmpcfg_csr) & cfgmask);
    csr_write_num(pmpaddr_csr, 0);
	  csr_write_num(pmpcfg_csr, pmpcfg);
  }
}
