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
void llvmMunchStm(T_stm s);
Temp_temp llvmMunchExp(T_exp e);
void llvmMunchCjump(T_stm s);
void llvmMunchMove(T_stm s);
Temp_temp llvmMunchBinop(Temp_temp ret, T_exp e);
void llvmMunchCall(Temp_temp ret, T_exp e);
void llvmMunchExtCall(Temp_temp ret, T_exp e);
Temp_tempList llvmMunchArgs(T_expList args, string res, bool first, int i);
#endif
