#ifndef LLVMGEN_H
#define LLVMGEN_H

#include "assem.h"
#include "tigerirp.h"
#include "util.h"

AS_instrList llvmbody(T_stmList stmList);
AS_instrList llvmbodybeg(Temp_label lbeg);
AS_instrList llvmprolog(string methodname, Temp_tempList args, T_type rettype);
AS_instrList llvmepilog(Temp_label lend);

/* Helper methods */
void munchStm(T_stm s);
Temp_temp munchExp(T_exp e);
void munchCjump(T_stm s);
void munchMove(T_stm s);
Temp_temp munchBinop(Temp_temp ret, T_exp e);
void munchCall(Temp_temp ret, T_exp e);
void munchExtCall(Temp_temp ret, T_exp e);
Temp_tempList munchArgs(T_expList args, string res, bool first, int i);
#endif
