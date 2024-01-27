#include "sbi_utils/rvbt/rvbt_stepping.h"
#include "sbi/riscv_asm.h"
#include "sbi/riscv_encoding.h"
#include "sbi_utils/rvbt/rvbt_serial.h"

bool is_stepping[16];

int rvbt_stepping(struct sbi_trap_regs *regs)
{
	int hartid = csr_read(CSR_MHARTID);
	struct sbi_trap_info uptrap;
	uint64_t virt_addr  = regs->mepc, phys_addr, next_phys, next_virt;
	uint64_t satp_val   = csr_read(CSR_SATP);
	uint32_t insn	    = sbi_get_insn(regs->mepc, &uptrap);
	uint8_t offset	    = 0;
	bool aligned	    = (regs->mepc & 0x03) == 0;
	is_stepping[hartid] = true;
	do {
		if ((next_virt =
			     rvbt_jump_decode(insn, virt_addr, regs, false))) {
			next_virt =
				rvbt_jump_decode(insn, virt_addr, regs, true);
			regs->mepc = next_virt;
			break;
		}
		if (INSN_IS_16BIT(insn)) {
			offset = 4;
			uint32_t next_insn =
				sbi_get_insn(regs->mepc + 2, &uptrap);
			next_virt = rvbt_jump_predict(next_insn, regs->mepc + 2,
						      regs, false);
		} else if (INSN_IS_32BIT(insn)) {
			offset = aligned ? 4 : 6;
			uint32_t next_insn =
				sbi_get_insn(regs->mepc + 4, &uptrap);
			next_virt = rvbt_jump_predict(next_insn, regs->mepc + 4,
						      regs, false);
		}
	} while (false);
	phys_addr = rvbt_mmu_translate(virt_addr + offset, satp_val);
	if (!rvbt_in_phys_mem((void *)phys_addr))
		return -1;

	next_phys = rvbt_mmu_translate(next_virt, satp_val);
	if (rvbt_in_phys_mem((void *)next_phys))
		pmp_set(1, PMP_A_NA4 | PMP_R | PMP_W, next_phys, 2);
	else
		pmp_set(1, PMP_A_NA4 | PMP_R | PMP_W, phys_addr, 2);
	pmp_set(0, PMP_A_NA4 | PMP_R | PMP_W, phys_addr, 2);
	return 0;
}

int64_t jal_emu(uint32_t insn_val, uint64_t pc, struct sbi_trap_regs *regs,
		bool modifying)
{

	struct jal_insn_t insn = *((struct jal_insn_t *)&insn_val);
	uint32_t sign	       = (insn_val & SIGN_MASK) >> 31;
	uint64_t res	       = (sign << 20) | (insn.imm19_12 << 12) |
		       (insn.imm11 << 11) | (insn.imm10_1 << 1);
	int64_t offset = res | (SIGN_EXT20_MASK * sign);

	uint8_t rd_idx = (insn_val & RD_MASK) >> 7;
	if (modifying)
		((uint64_t *)regs)[rd_idx] = pc + 4;

	return pc + offset;
}

uint64_t jalr_emu(uint32_t insn_val, uint64_t pc, struct sbi_trap_regs *regs,
		  bool modifying)
{
	struct jalr_insn_t insn = *((struct jalr_insn_t *)&insn_val);
	uint32_t sign		= (insn_val & SIGN_MASK) >> 31;
	int64_t imm		= insn.imm11_0 | (SIGN_EXT11_MASK * sign);
	int64_t base		= ((int64_t *)regs)[insn.rs1];
	if (modifying)
		((uint64_t *)regs)[insn.rd] = pc + 4;

	return imm + base;
}

uint64_t bran_emu(uint32_t insn, uint64_t pc, struct sbi_trap_regs *regs)
{

	uint32_t sign	 = (insn & SIGN_MASK) >> 31;
	uint32_t imm10_5 = (insn & BRAN_IMM_10_5_MASK) >> 25;
	uint32_t imm4_1	 = (insn & BRAN_IMM_4_1_MASK) >> 8;
	uint32_t imm11	 = (insn & BRAN_IMM_11_MASK) >> 7;
	int64_t imm	 = (sign << 12) | (imm11 << 11) | (imm10_5 << 5) |
		      (imm4_1 << 1) | (SIGN_EXT12_MASK * sign);

	uint8_t rs1_idx = (insn & RS1_MASK) >> 15;
	uint8_t rs2_idx = (insn & RS2_MASK) >> 20;
	int64_t rs1	= ((int64_t *)regs)[rs1_idx];
	int64_t rs2	= ((int64_t *)regs)[rs2_idx];

	uint8_t func3 = (insn & FUNC3_MASK) >> 12;
	switch (func3) {
	case FUNC3_BEQ:
		return rs1 == rs2 ? pc + imm : pc + 4;
	case FUNC3_BNE:
		return rs1 != rs2 ? pc + imm : pc + 4;
	case FUNC3_BLT:
		return rs1 < rs2 ? pc + imm : pc + 4;
	case FUNC3_BLTU:
		return ((uint64_t)rs1) < ((uint64_t)rs2) ? pc + imm : pc + 4;
	case FUNC3_BGE:
		return rs1 >= rs2 ? pc + imm : pc + 4;
	case FUNC3_BGEU:
		return ((uint64_t)rs1) >= ((uint64_t)rs2) ? pc + imm : pc + 4;
	default:
		return -1;
	}
}

uint64_t bran_dest(uint32_t insn, uint64_t pc, struct sbi_trap_regs *regs)
{

	uint32_t sign	 = (insn & SIGN_MASK) >> 31;
	uint32_t imm10_5 = (insn & BRAN_IMM_10_5_MASK) >> 25;
	uint32_t imm4_1	 = (insn & BRAN_IMM_4_1_MASK) >> 8;
	uint32_t imm11	 = (insn & BRAN_IMM_11_MASK) >> 7;
	int64_t imm	 = (sign << 12) | (imm11 << 11) | (imm10_5 << 5) |
		      (imm4_1 << 1) | (SIGN_EXT12_MASK * sign);
	uint8_t func3 = (insn & FUNC3_MASK) >> 12;
	switch (func3) {
	case FUNC3_BEQ:
	case FUNC3_BNE:
	case FUNC3_BLT:
	case FUNC3_BLTU:
	case FUNC3_BGE:
	case FUNC3_BGEU:
		return pc + imm;
	default:
		return -1;
	}
}
uint64_t cj_emu(uint16_t insn_val, uint64_t pc, struct sbi_trap_regs *regs,
		bool modifying)
{
	struct cj_insn_t insn = *(struct cj_insn_t *)&insn_val;
	int64_t imm	      = (insn.imm11 << 11) | (insn.imm10 << 10) |
		      (insn.imm9_8 << 8) | (insn.imm7 << 7) | (insn.imm6 << 6) |
		      (insn.imm5 << 5) | (insn.imm4 << 4) | (insn.imm3_1 << 1);
	imm = imm | (SIGN_EXT11_MASK * insn.imm11);
	if (insn.func3 == CFUNC3_J)
		return pc + imm;
	return 0;
}

uint64_t cr_emu(uint16_t insn_val, uint64_t pc, struct sbi_trap_regs *regs,
		bool modifying)
{
	struct cr_insn_t insn = *(struct cr_insn_t *)&insn_val;
	if (insn.func4 != 0x8 && insn.func4 != 0x9)
		return 0;
	if (insn.rs1 == 0 || insn.rs2 != 0 ||
	    (insn.func4 != 0x8 && insn.func4 != 0x9))
		return 0;
	if (insn.func4 == 0x9 && modifying)
		((uint64_t *)regs)[1] = pc + 2;
	return ((uint64_t *)regs)[insn.rs1];
}

uint64_t cb_emu(uint16_t insn_val, uint64_t pc, struct sbi_trap_regs *regs)
{
	struct cb_insn_t insn = *(struct cb_insn_t *)&insn_val;
	int64_t imm = (insn.imm8 << 8) | (insn.imm7_6 << 6) | (insn.imm5 << 5) |
		      (insn.imm4_3 << 3) | (insn.imm2_1 << 1) |
		      (insn.imm8 * SIGN_EXT8_MASK);
	uint64_t rs1 = ((uint64_t *)regs)[insn.crs1 + 8];
	if (insn.func3 == CFUNC3_BEQZ)
		return rs1 == 0 ? pc + imm : pc + 2;
	else if (insn.func3 == CFUNC3_BNEZ)
		return rs1 != 0 ? pc + imm : pc + 2;
	return 0;
}

uint64_t cb_dest(uint16_t insn_val, uint64_t pc, struct sbi_trap_regs *regs)
{
	struct cb_insn_t insn = *(struct cb_insn_t *)&insn_val;
	int64_t imm = (insn.imm8 << 8) | (insn.imm7_6 << 6) | (insn.imm5 << 5) |
		      (insn.imm4_3 << 3) | (insn.imm2_1 << 1) |
		      (insn.imm8 * SIGN_EXT8_MASK);
	if (insn.func3 == CFUNC3_BEQZ)
		return pc + imm;
	else if (insn.func3 == CFUNC3_BNEZ)
		return pc + imm;
	return 0;
}
uint64_t rvbt_jump_decode(uint32_t insn, uint64_t insn_addr,
			  struct sbi_trap_regs *regs, bool modifying)
{
	uint8_t opcode = insn & COPCODE_MASK;
	uint8_t func3  = (insn & CFUNC3_MASK) >> 13;
	if (opcode == 0x1) {
		if (func3 == CFUNC3_BEQZ || func3 == CFUNC3_BNEZ)
			return cb_emu(insn, insn_addr, regs);
		else if (func3 == CFUNC3_J)
			return cj_emu(insn, insn_addr, regs, modifying);
	} else if (opcode == 0x2) {
		return cr_emu(insn, insn_addr, regs, modifying);
	} else if (opcode == 0x3) {
		opcode = insn & OPCODE_MASK;
		switch (opcode) {
		case OPCODE_JAL:
			return jal_emu(insn, insn_addr, regs, modifying);
		case OPCODE_JALR:
			return jalr_emu(insn, insn_addr, regs, modifying);
		case OPCODE_BRANCH:
			return bran_emu(insn, insn_addr, regs);
		}
	}
	return 0;
}

uint64_t rvbt_jump_predict(uint32_t insn, uint64_t insn_addr,
			   struct sbi_trap_regs *regs, bool modifying)
{
	uint8_t opcode = insn & COPCODE_MASK;
	uint8_t func3  = (insn & CFUNC3_MASK) >> 13;
	if (opcode == 0x1) {
		if (func3 == CFUNC3_BEQZ || func3 == CFUNC3_BNEZ)
			return cb_dest(insn, insn_addr, regs);
		else if (func3 == CFUNC3_J)
			return cj_emu(insn, insn_addr, regs, modifying);
	} else if (opcode == 0x2) {
		return cr_emu(insn, insn_addr, regs, modifying);
	} else if (opcode == 0x3) {
		opcode = insn & OPCODE_MASK;
		switch (opcode) {
		case OPCODE_JAL:
			return jal_emu(insn, insn_addr, regs, modifying);
		case OPCODE_JALR:
			return jalr_emu(insn, insn_addr, regs, modifying);
		case OPCODE_BRANCH:
			return bran_dest(insn, insn_addr, regs);
		}
	}
	return 0;
}
