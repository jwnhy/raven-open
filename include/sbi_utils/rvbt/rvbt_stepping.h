#ifndef __RVBT_STEPPING__
#define __RVBT_STEPPING__
#include "sbi/sbi_types.h"
#include "stdbool.h"
#include "sbi/riscv_asm.h"
#include "sbi_utils/rvbt/rvbt_memory.h"
#include "sbi/sbi_math.h"
#include "sbi/sbi_trap.h"
#include "sbi/sbi_unpriv.h"

#define OPCODE_MASK 0x7f
#define OPCODE_JAL 0x6f
#define OPCODE_JALR 0x67
#define OPCODE_BRANCH 0x63

#define COPCODE_MASK 0x3

#define CJUMP_OP 0x1

#define CFUNC3_MASK (0x7 << 13)
#define CFUNC4_BIT_MASK (0x1 << 12)

// CJAL is RV32 Only
#define CFUNC3_JAL 0x1
#define CFUNC3_J   0x5
#define CFUNC3_JALR 0x4
#define CFUNC3_BEQZ 0x6
#define CFUNC3_BNEZ 0x7

#define SIGN_EXT20_MASK 0xffffffffffe00000
#define SIGN_EXT11_MASK 0xfffffffffffff000
#define SIGN_EXT8_MASK 0xfffffffffffffe00
#define SIGN_EXT12_MASK 0xffffffffffffe000
#define SIGN_MASK (0x1 << 31)

#define JALR_IMM_11_0_MASK 0xff000000
#define JAL_IMM_10_1_MASK 0x7fe00000
#define JAL_IMM_11_MASK (1 << 20)
#define JAL_IMM_19_12_MASK 0xff000

#define BRAN_IMM_10_5_MASK (0x3f << 25)
#define BRAN_IMM_4_1_MASK (0xf << 8)
#define BRAN_IMM_11_MASK (0x1 << 7)

#define FUNC3_BEQ 0x0
#define FUNC3_BNE 0x1
#define FUNC3_BLT 0x4
#define FUNC3_BGE 0x5
#define FUNC3_BLTU 0x6
#define FUNC3_BGEU 0x7


#define RS1_MASK (0x1f << 15)
#define RS2_MASK (0x1f << 20)
#define FUNC3_MASK (0x7 << 12) 
#define RD_MASK (0x1f << 7)

struct jalr_insn_t {
  uint8_t opcode: 7;
  uint8_t rd: 5;
  uint8_t func3: 3;
  uint8_t rs1 :5;
  uint64_t imm11_0: 12;
}__attribute__((packed));

struct jal_insn_t {
  uint8_t opcode: 7;
  uint8_t rd: 5;
  uint64_t imm19_12: 8;
  uint64_t imm11: 1;
  uint64_t imm10_1: 10;
  uint64_t imm20:1;
}__attribute__((packed));

struct cb_insn_t {
  uint8_t opcode:2;
  uint64_t imm5:1;
  uint64_t imm2_1:2;
  uint64_t imm7_6:2;
  uint8_t crs1:3;
  uint64_t imm4_3:2;
  uint64_t imm8:1;
  uint8_t func3:3;
}__attribute__((packed));

struct cj_insn_t {
  uint8_t opcode:2;
  uint64_t imm5: 1;
  uint64_t imm3_1 :3;
  uint64_t imm7: 1;
  uint64_t imm6: 1;
  uint64_t imm10: 1;
  uint64_t imm9_8: 2;
  uint64_t imm4: 1;
  uint64_t imm11: 1;
  uint8_t func3: 3;
}__attribute__((packed));

struct cr_insn_t {
  uint8_t opcode:2;
  uint8_t rs2:5;
  uint8_t rs1:5;
  uint8_t func4: 4;
}__attribute__((packed));


extern bool is_stepping[16];
int rvbt_stepping(struct sbi_trap_regs* regs);
uint64_t rvbt_jump_decode(uint32_t insn,uint64_t insn_addr, struct sbi_trap_regs *regs, bool modifying);
uint64_t rvbt_jump_predict(uint32_t insn,uint64_t insn_addr, struct sbi_trap_regs *regs, bool modifying);
#endif
