/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi_utils/rvbt/rvbt_breakpoint.h>
#include <sbi/riscv_barrier.h>

typedef int (*illegal_insn_func)(ulong insn, struct sbi_trap_regs *regs);

static int truly_illegal_insn(ulong insn, struct sbi_trap_regs *regs)
{
	struct sbi_trap_info trap;

	trap.epc   = regs->mepc;
	trap.cause = CAUSE_ILLEGAL_INSTRUCTION;
	trap.tval  = insn;
	trap.tval2 = 0;
	trap.tinst = 0;

	return sbi_trap_redirect(regs, &trap);
}

static int system_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int do_write, rs1_num = (insn >> 15) & 0x1f;
	ulong rs1_val = GET_RS1(insn, regs);
	int csr_num   = (u32)insn >> 20;
	ulong csr_val, new_csr_val;

	/* TODO: Ensure that we got CSR read/write instruction */

	if (sbi_emulate_csr_read(csr_num, regs, &csr_val))
		return truly_illegal_insn(insn, regs);

	do_write = rs1_num;
	switch (GET_RM(insn)) {
	case 1:
		new_csr_val = rs1_val;
		do_write    = 1;
		break;
	case 2:
		new_csr_val = csr_val | rs1_val;
		break;
	case 3:
		new_csr_val = csr_val & ~rs1_val;
		break;
	case 5:
		new_csr_val = rs1_num;
		do_write    = 1;
		break;
	case 6:
		new_csr_val = csr_val | rs1_num;
		break;
	case 7:
		new_csr_val = csr_val & ~rs1_num;
		break;
	default:
		return truly_illegal_insn(insn, regs);
	};

	if (do_write && sbi_emulate_csr_write(csr_num, regs, new_csr_val))
		return truly_illegal_insn(insn, regs);

	SET_RD(insn, regs, csr_val);

	regs->mepc += 4;

	return 0;
}

static illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
	truly_illegal_insn, /* 1 */
	truly_illegal_insn, /* 2 */
	truly_illegal_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
	truly_illegal_insn, /* 9 */
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	truly_illegal_insn, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

int rvbt_emulate_satp_access(uint64_t insn, struct sbi_trap_regs *regs)
{
	const uint64_t FUNC3_MASK = 0x7000;
	const uint64_t RS1_MASK	  = 0xf8000;
	const uint64_t RD_MASK	  = 0xf80;
	uint8_t rs1 = (insn & RS1_MASK) >> 15, rd = (insn & RD_MASK) >> 7;
	uint64_t *regs_arr = (uint64_t *)regs;
	uint64_t temp	   = csr_read(CSR_SATP);
	switch ((insn & FUNC3_MASK) >> 12) {
	case 1:
		csr_write(CSR_SATP, regs_arr[rs1]);
		break;
	case 2:
		csr_write(CSR_SATP, regs_arr[rs1] | temp);
		break;
	case 3:
		csr_write(CSR_SATP, regs_arr[rs1] & temp);
		break;
	case 4:
		csr_write(CSR_SATP, rs1);
		break;
	case 5:
		csr_write(CSR_SATP, rs1 | temp);
		break;
	case 6:
		csr_write(CSR_SATP, rs1 & temp);
		break;
	}
	regs_arr[rd] = temp;
	return 0;
}

static int tvm_count = 0;
int sbi_illegal_insn_handler(ulong insn, struct sbi_trap_regs *regs)
{
	struct sbi_trap_info uptrap;

	/*
	 * We only deal with 32-bit (or longer) illegal instructions. If we
	 * see instruction is zero OR instruction is 16-bit then we fetch and
	 * check the instruction encoding using unprivilege access.
	 *
	 * The program counter (PC) in RISC-V world is always 2-byte aligned
	 * so handling only 32-bit (or longer) illegal instructions also help
	 * the case where MTVAL CSR contains instruction address for illegal
	 * instruction trap.
	 */

	insn = sbi_get_insn(regs->mepc, &uptrap);
  
	//RVBT
	if ((insn & SFENCE_VM_MASK) == SFENCE_VM) {
		tvm_count++;
		__asm__ __volatile("sfence.vma");
		regs->mepc += 4;
		rvbt_update_breakpoint();
    //if (tvm_count % 1000 == 0)
      //sbi_printf("tvm_count: %d\n", tvm_count);
		return 0;
	} else if ((insn & SATP_ACCESS_MASK) == SATP_ACCESS) {
		tvm_count++;
		rvbt_emulate_satp_access(insn, regs);
		regs->mepc += 4;
		rvbt_update_breakpoint();
		//if (tvm_count % 1000 == 0)
      //sbi_printf("tvm_count: %d\n", tvm_count);
		return 0;
	}

	sbi_pmu_ctr_incr_fw(SBI_PMU_FW_ILLEGAL_INSN);
	if (unlikely((insn & 3) != 3)) {
		insn = sbi_get_insn(regs->mepc, &uptrap);
		if (uptrap.cause) {
			uptrap.epc = regs->mepc;
			return sbi_trap_redirect(regs, &uptrap);
		}
		if ((insn & 3) != 3)
			return truly_illegal_insn(insn, regs);
	}

	return illegal_insn_table[(insn & 0x7c) >> 2](insn, regs);
}
