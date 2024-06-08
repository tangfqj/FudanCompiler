#ifndef ARMGEN_H
#define ARMGEN_H

#include "assem.h"
#include "util.h"
#include "temp.h"

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
void armMunchInstr(AS_instr inst);
void armMunchRet(AS_instr inst);
void armMunchAdd(AS_instr inst);
void armMunchSub(AS_instr inst);
void armMunchMul(AS_instr inst);
void armMunchDiv(AS_instr inst);
void armMunchP2I(AS_instr inst);
void armMunchFbinop(AS_instr inst, string bop);
void armMunchLoad(AS_instr inst);
void armMunchStore(AS_instr inst);
void armMunchCall(AS_instr inst);
void armMunchExtCall(AS_instr inst);
void armMunchIcmp(AS_instr inst);
void armMunchFcmp(AS_instr inst);
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