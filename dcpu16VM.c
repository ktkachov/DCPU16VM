#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define RAM_SIZE 0x10000
#define NUM_REGS 8


typedef struct architecture_t {
  uint16_t ram[RAM_SIZE];
  uint16_t regs[NUM_REGS];
  uint16_t num_inst;
  uint16_t pc;
  uint16_t sp;
  uint16_t o;
  uint64_t cycles;
} arch_t;

short isLiteral(uint16_t);
uint16_t toLiteral(uint16_t);

inline short isLiteral(uint16_t v) {
  return v >= 0x20 && v <= 0x3f; 
}

inline uint16_t toLiteral(uint16_t v) {
  return v - 0x20;
}

void dumpRegs(arch_t* a) {
  uint16_t* regs = a->regs;
  printf("A = %#x\n", regs[0]);
  printf("B = %#x\n", regs[1]);
  printf("C = %#x\n", regs[2]);
  printf("X = %#x\n", regs[3]);
  printf("Y = %#x\n", regs[4]);
  printf("Z = %#x\n", regs[5]);
  printf("I = %#x\n", regs[6]);
  printf("J = %#x\n", regs[7]);
  printf("SP = %#x\n", a->sp);
  printf("PC = %#x\n", a->pc);
  printf("O = %#x\n", a->o);
}

short sizeOfInstructionWords(uint16_t i) {
  uint16_t op_code = i & 0xf;
  uint16_t a = (i >> 4) & 0x3f;
  uint16_t b = (i >> 10) & 0x3f;
  if (op_code == 0) {
    if (b == 0x1e || b == 0x1f) {
      return 2;
    }
    return 1;
  }
  short s = 1;
  s = (a == 0x1e || a == 0x1f) ? s+1 : s;
  s = (b == 0x1e || b == 0x1f) ? s+1 : s;
  return s;
}

uint16_t* getValueAddress(arch_t* a, uint16_t v) {
  if (v <= 0x07) {
    return &a->regs[v];
  }
  if (v <= 0x0f) {
    return &a->ram[a->regs[v - NUM_REGS]];
  }
  if (v <= 0x17) {
    a->cycles += 1;
    return &a->ram[a->ram[a->pc++] + a->regs[v-NUM_REGS]];
  }
  switch (v) {
    case 0x18: /*POP*/
      return &a->ram[a->sp++];
    case 0x19: /*PEEK*/
      return &a->ram[a->sp];
    case 0x1a: /*PUSH*/
      return &a->ram[--a->sp];
    case 0x1b: /*SP*/
      return &a->sp;
    case 0x1c: /*PC*/
      return &a->pc;
    case 0x1d: /*Overflow*/
      return &a->o;
    case 0x1e: /*[next word]*/
      a->cycles += 1;
      return &a->ram[a->ram[a->pc++]];
    case 0x1f: /*next word literal (probably not entirely correct handling*/
      a->cycles += 1;
      return &a->ram[a->pc++];
    default:
      printf("Error in value: %#x, literals should have been handled earlier\n", v);
  }
  return NULL;
}

uint16_t getValue(arch_t* a, uint16_t v) {
  if (isLiteral(v)) {
    return toLiteral(v);
  }
  return *getValueAddress(a, v);
}

int main(int argc, char* argv[]) {
  arch_t* arch = malloc(sizeof(*arch));
  arch->num_inst = 0;
  /*Read in program stored as hex integers in ascii format*/
  while (scanf("%x ", &arch->ram[arch->num_inst++]) != EOF) {
    if (arch->num_inst == RAM_SIZE-1) {
      printf("instructions cannot fit in RAM!\n");
      return 1;
    }
//    printf("%x\n", arch->ram[arch->num_inst-1]);
  }

  arch->cycles = 0;
  arch->pc = 0x0;
  arch->sp = RAM_SIZE - 1;
  arch->o = 0x0;

  /*decode and execute instructions*/
  uint8_t op_code = 0;
  uint8_t op_a    = 0;
  uint8_t op_b    = 0;
  
  while(arch->pc < RAM_SIZE-1) {
    uint16_t inst = arch->ram[arch->pc++];
    op_code = inst & 0xf;
    op_a    = (inst >> 4) & 0x3f;
    op_b    = (inst >> 10) & 0x3f;

    printf("exec ram[%#x]: %#x, a: %#x, b: %#x @ cycle %lu\n", arch->pc, op_code, op_a, op_b, arch->cycles);

    dumpRegs(arch);

    if (op_code == 0x0) {
      op_code = op_a;
      op_a = op_b;
      switch (op_code) {
        case 0x00: /*reserved*/
          break;

        case 0x01: /*JSR*/
          ;  /*??????????????????????????????*/
          uint16_t a = getValue(arch, op_a);
          arch->ram[--arch->sp] = arch->pc;
          arch->pc = a;
          arch->cycles += 2;
          break;

        default: /*0x02 - 0x3f reserved*/
          break;            
      }
    } else {
       uint16_t* a = getValueAddress(arch, op_a);
       uint16_t b = getValue(arch, op_b);
       switch (op_code) {
        case 0x1: /*SET*/
          if (!isLiteral(op_a)) {
            *a = b;
          }
          arch->cycles += 1;
          break;
        case 0x2:
          if (!isLiteral(op_a)) {
            uint16_t s16 = *a + b;
            uint32_t s32 = (uint32_t)*a + b;
            arch->o = s16 == s32 ? 0x0 : 0x0001;
            *a = s16;
          }
          arch->cycles += 2;
          break;
        case 0x3:
          if (!isLiteral(op_a)) {
            uint16_t d = *a-b;
            arch->o = *a < b ? 0xffff : 0x0;
            *a = d;
          }
          arch->cycles += 2;
          break;
        case 0x4:
          if (!isLiteral(op_a)) {
            uint16_t m = (*a) * b;
            arch->o = (m >> 16) & 0xffff;
            *a = m;
          }
          arch->cycles += 2;
          break;
        case 0x5:
          if (!isLiteral(op_a)) {
            if (b == 0) {
              *a = 0;
              arch->o = 0;
            } else {
              uint16_t d = *a / b;
              arch->o = (((*a) << 16) / b) & 0xffff;
            }
          }
          arch->cycles += 3;
          break;
        case 0x6:
          if (!isLiteral(op_a)) {
            *a = b == 0 ? 0 : *a % b;
          }
          arch->cycles += 3;
          break;
        case 0x7:
          if (!isLiteral(op_a)) {
            *a = (*a) << b;
            arch->o = ((*a) >> 16) & 0xffff;
          }
          arch->cycles += 2;
          break;
        case 0x8:
          if (!isLiteral(op_a)) {
            uint16_t r = (*a) << b;
            arch->o = ( ((*a) << 16) >> b ) & 0xffff;
          }
          arch->cycles += 2;
          break;
        case 0x9:
          if (!isLiteral(op_a)) {
            *a = (*a) & b;
          }
          arch->cycles += 1;
          break;
        case 0xa:
          if (!isLiteral(op_a)) {
            *a = (*a) | b;
          }
          arch->cycles += 1;
          break;
        case 0xb:
          if (!isLiteral(op_a)) {
            *a = isLiteral(op_a) ? *a : (*a) ^ b;
          }
          arch->cycles += 1;
          break;
        case 0xc:
          if (!(*a == b)) {
            arch->pc += sizeOfInstructionWords(arch->ram[arch->pc]);
            arch->cycles += 1;
          }
          arch->cycles += 2;
          break;
        case 0xd:
          if (!(*a != b)) {
            arch->pc += sizeOfInstructionWords(arch->ram[arch->pc]);
            arch->cycles += 1;
          }
          arch->cycles += 2;
          break;
        case 0xe:
          if (!(*a > b)) {
            arch->pc += sizeOfInstructionWords(arch->ram[arch->pc]);
            arch->cycles += 1;
          }
          arch->cycles += 2;
          break;
        case 0xf:
          if (!((*a & b) != 0)) {
            arch->pc += sizeOfInstructionWords(arch->ram[arch->pc]);
            arch->cycles += 1;
          }
          arch->cycles += 2;
          break;
      }
    }
  }
  printf("Dumping registers\n");
  dumpRegs(arch);
  return 0;
}
