#ifndef ARMGEN_H
#define ARMGEN_H

#include "assem.h"
#include "util.h"

AS_instrList armprolog(AS_instr il);
AS_instrList armbody(AS_instrList il);
AS_instrList armepilog(AS_instrList il);

/* Helper methods */
typedef enum {
  NONE,
  BR,
  RET,
  ADD,
  SUB,
  MUL,
  DIV,
  FADD,
  FSUB,
  FMUL,
  FDIV,
  F2I,
  I2F,
  I2P,
  P2I,
  LOAD,
  STORE,
  CALL,
  EXTCALL,
  ICMP,
  FCMP,
  LABEL,
  CJUMP,
  END,
} AS_type;

AS_type gettype(AS_instr ins);
static string getlabel(AS_instr inst);
static Temp_temp nthTemp(Temp_tempList list, int i);
void munchInstr(AS_instr inst);
void munchRet(AS_instr inst);
void munchAdd(AS_instr inst);
void munchSub(AS_instr inst);
void munchMul(AS_instr inst);
void munchDiv(AS_instr inst);
void munchP2I(AS_instr inst);
void munchFbinop(AS_instr inst, string bop);
void munchLoad(AS_instr inst);
void munchStore(AS_instr inst);
void munchCall(AS_instr inst);
void munchExtCall(AS_instr inst);
void munchIcmp(AS_instr inst);
void munchFcmp(AS_instr inst);
void moveInt(int i, Temp_tempList dst);
void moveFloat(float f, Temp_tempList dst);
typedef union {
  int i;
  float f;
  unsigned u;
} NumUnion;
void callerSaveCall();
void callerSaveRet();
#endif