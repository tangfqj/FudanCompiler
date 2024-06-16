#include "llvmgen.h"

#include "temp.h"
#include "util.h"

#define LLVMGEN_DEBUG
#undef LLVMGEN_DEBUG

static AS_instrList iList = NULL, last = NULL;
static void emit(AS_instr inst) {
  if (last) {
    last = last->tail = AS_InstrList(inst, NULL);
  } else {
    last = iList = AS_InstrList(inst, NULL);
  }
}

static Temp_tempList TL(Temp_temp t, Temp_tempList tl) { return Temp_TempList(t, tl); }

static Temp_tempList TLS(Temp_tempList a, Temp_tempList b) {
  return Temp_TempListSplice(a, b);
}

static Temp_labelList LL(Temp_label l, Temp_labelList ll) {
  return Temp_LabelList(l, ll);
}

AS_instrList llvmbody(T_stmList stmList) {
  if (!stmList) return NULL;
  iList = last = NULL;
  T_stmList sl;
  for (sl = stmList; sl; sl = sl->tail) {
    llvmMunchStm(sl->head);
  }

  return iList;
}

// This function is to make the beginning of the function that jumps to the
// beginning label (lbeg) of a block (in case the lbeg is NOT at the beginning
// of the block).
AS_instrList llvmbodybeg(Temp_label lbeg) {
  if (!lbeg) return NULL;
  iList = last = NULL;
  Temp_label lstart = Temp_newlabel();
  string ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "%s:", Temp_labelstring(lstart));
  emit(AS_Label(ir, lstart));
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "br label %%`j0");
  emit(AS_Oper(ir, NULL, NULL, AS_Targets(LL(lbeg, NULL))));
  return iList;
}

// This function is to make the prolog of the function that takes the method
// name and the arguments
AS_instrList llvmprolog(string methodname, Temp_tempList args, T_type rettype) {
#ifdef LLVMGEN_DEBUG
  fprintf(stderr, "llvmprolog: methodname=%s, rettype=%d\n", methodname, rettype);
#endif
  if (!methodname) return NULL;
  iList = last = NULL;
  string ir = (string)checked_malloc(IR_MAXLEN);
  if (rettype == T_int)
    sprintf(ir, "define i64 @%s(", methodname);
  else if (rettype == T_float)
    sprintf(ir, "define double @%s(", methodname);
#ifdef LLVMGEN_DEBUG
  fprintf(stderr, "llvmprolog: ir=%s\n", ir);
#endif
  if (args) {
    int i = 0;
    for (Temp_tempList arg = args; arg; arg = arg->tail, i += 1) {
      if (i != 0) sprintf(ir + strlen(ir), ", ");
      if (arg->head->type == T_int) {
        sprintf(ir + strlen(ir), "i64 %%`s%d", i);
#ifdef LLVMGEN_DEBUG
        fprintf(stderr, "%d, llvmprolog: ir=%s\n", i, ir);
#endif
      } else if (arg->head->type == T_float) {
#ifdef LLVMGEN_DEBUG
        fprintf(stderr, "%d, llvmprolog: ir=%s\n", i, ir);
#endif
        sprintf(ir + strlen(ir), "double %%`s%d", i);
      }
#ifdef LLVMGEN_DEBUG
      fprintf(stderr, "llvmprolog args: arg=%s\n", ir);
#endif
    }
  }
  sprintf(ir + strlen(ir), ") {");
#ifdef LLVMGEN_DEBUG
  fprintf(stderr, "llvmprolog final: ir=%s\n", ir);
#endif
  emit(AS_Oper(ir, NULL, args, NULL));
  return iList;
}

// This function is to make the epilog of the function that jumps to the end
// label (lend) of a block
AS_instrList llvmepilog(Temp_label lend) {
  if (!lend) return NULL;
  iList = last = NULL;
  // string ir = (string) checked_malloc(IR_MAXLEN);
  // sprintf(ir, "%s:", Temp_labelstring(lend));
  // emit(AS_Label(ir, lend));
  // emit(AS_Oper("ret i64 -1", NULL, NULL, NULL));
  emit(AS_Oper("}", NULL, NULL, NULL));
  return iList;
}

/* Helper methods */
void llvmMunchStm(T_stm s) {\
  string ir = (string)checked_malloc(IR_MAXLEN);
  switch (s->kind) {
    case T_SEQ:
      llvmMunchStm(s->u.SEQ.left);
      llvmMunchStm(s->u.SEQ.right);
      break;
    case T_LABEL:
      sprintf(ir, "%s:", Temp_labelstring(s->u.LABEL));
      emit(AS_Label(ir, s->u.LABEL));
      break;
    case T_JUMP:
      emit(AS_Oper("br label %`j0", NULL, NULL, AS_Targets(LL(s->u.JUMP.jump, NULL))));
      break;
    case T_CJUMP:
      llvmMunchCjump(s);
      break;
    case T_MOVE:
      llvmMunchMove(s);
      break;
    case T_EXP:
      llvmMunchExp(s->u.EXP);
      break;
    case T_RETURN:
      if (s->u.EXP->kind == T_CONST) {
        if (s->u.EXP->type == T_int) {
          sprintf(ir, "ret i64 %d", s->
                                    u.EXP->u.CONST.i);
          emit(AS_Oper(ir, NULL, NULL, NULL));
        } else {  // s->u.EXP->type == T_float
          sprintf(ir, "ret double %f", s->u.EXP->u.CONST.f);
          emit(AS_Oper(ir, NULL, NULL, NULL));
        }
      } else {
        Temp_temp ret = llvmMunchExp(s->u.EXP);
        if (ret->type == T_int) {
          sprintf(ir, "ret i64 %%`s0");
          emit(AS_Oper(ir, NULL, TL(ret, NULL), NULL));
        } else {
          sprintf(ir, "ret double %%`s0");
          emit(AS_Oper(ir, NULL, TL(ret, NULL), NULL));
        }
      }
      break;
    default:
      fprintf(stderr, "Error in munchStm!");
  }
}

Temp_temp llvmMunchExp(T_exp e) {
  string ir = (string)checked_malloc(IR_MAXLEN);
  string ld = (string)checked_malloc(IR_MAXLEN);
  Temp_temp ret, l1, cst;
  switch (e->kind) {
    case T_BINOP:
      return llvmMunchBinop(NULL, e);
    case T_MEM:
      l1 = Temp_newtemp(T_int);
      if (e->u.MEM->kind == T_CONST) {
        sprintf(ir, "%%`d0 = inttoptr i64 %d to i64*", e->u.MEM->u.CONST.i);
        emit(AS_Oper(ir, TL(l1, NULL), NULL, NULL));
      } else {
        Temp_temp loc = llvmMunchExp(e->u.MEM);
        sprintf(ir, "%%`d0 = inttoptr i64 %%`s0 to i64*");
        emit(AS_Oper(ir, TL(l1, NULL), TL(loc, NULL), NULL));
      }
      if (e->type == T_int) {
        ret = Temp_newtemp(T_int);
        sprintf(ld, "%%`d0 = load i64, i64* %%`s0");
      } else {
        ret = Temp_newtemp(T_float);
        sprintf(ld, "%%`d0 = load double, i64* %%`s0");
      }
      emit(AS_Oper(ld, TL(ret, NULL), TL(l1, NULL), NULL));
      return ret;
    case T_TEMP:
      return e->u.TEMP;
    case T_ESEQ:
      llvmMunchStm(e->u.ESEQ.stm);
      return llvmMunchExp(e->u.ESEQ.exp);
    case T_NAME:
      ret = Temp_newtemp(T_int);
      sprintf(ir, "%%`d0 = ptrtoint i64* @%s to i64", Temp_labelstring(e->u.NAME));
      emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
      return ret;
    case T_CONST:
      if (e->type == T_int) {
        ret = Temp_newtemp(T_int);
        sprintf(ir, "%%`d0 = add i64 %d, 0", e->u.CONST.i);
      } else {  // T_float
        ret = Temp_newtemp(T_float);
        sprintf(ir, "%%`d0 = fadd double %f, 0.0", e->u.CONST.f);
      }
      emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
      return ret;
    case T_CALL:
      ret = Temp_newtemp(T_int);
      llvmMunchCall(ret, e);
      return ret;
    case T_ExtCALL:
      ret = Temp_newtemp(T_int);
      llvmMunchExtCall(ret, e);
      return ret;
    case T_CAST:
      cst = llvmMunchExp(e->u.CAST);
      if (e->type == T_int) {
        ret = Temp_newtemp(T_int);
        sprintf(ir, "%%`d0 = fptosi double %%`s0 to i64");
        emit(AS_Oper(ir, TL(ret, NULL), TL(cst, NULL), NULL));
      } else {
        ret = Temp_newtemp(T_float);
        sprintf(ir, "%%`d0 = sitofp i64 %%`s0 to double");
        emit(AS_Oper(ir, TL(ret, NULL), TL(cst, NULL), NULL));
      }
      return ret;
    default:
      fprintf(stderr, "Error in munchExp!");
      return NULL;
  }
}

void llvmMunchCjump(T_stm s) {
  string ir = (string)checked_malloc(IR_MAXLEN);
  string brch = (string)checked_malloc(IR_MAXLEN);
  Temp_temp cond = Temp_newtemp(T_int);
  Temp_temp s0 = NULL, s1 = NULL;
  if (s->u.CJUMP.left->kind != T_CONST) {
    s0 = llvmMunchExp(s->u.CJUMP.left);
  }
  if (s->u.CJUMP.right->kind != T_CONST) {
    s1 = llvmMunchExp(s->u.CJUMP.right);
  }
  // Get the type of operands
  T_type left, right;
  if (!s0 && !s1) {
    left = s->u.CJUMP.left->type;
    right = s->u.CJUMP.right->type;
  } else if (!s0) {
    left = s->u.CJUMP.left->type;
    right = s1->type;
  } else if (!s1) {
    left = s0->type;
    right = s->u.CJUMP.right->type;
  } else {
    left = s0->type;
    right = s1->type;
  }
  if (left != right) {
    fprintf(stderr, "Type incompatible, left %d, right %d\n", left, right);
  }
  // Decide the operator
  string op;
  if (left == T_int && right == T_int) {
    switch (s->u.CJUMP.op) {
      case T_eq:
        op = "eq";
        break;
      case T_ne:
        op = "ne";
        break;
      case T_lt:
        op = "slt";
        break;
      case T_le:
        op = "sle";
        break;
      case T_gt:
        op = "sgt";
        break;
      case T_ge:
        op = "sge";
        break;
      default:
        fprintf(stderr, "Error in Cjump!");
        break;
    }
    if (!s0 && !s1) {
      sprintf(ir, "%%`d0 = icmp %s i64 %d, %d", op, s->u.CJUMP.left->u.CONST.i,
              s->u.CJUMP.right->u.CONST.i);
      emit(AS_Oper(ir, TL(cond, NULL), NULL, NULL));
    } else if (!s0) {
      sprintf(ir, "%%`d0 = icmp %s i64 %d, %%`s0", op, s->u.CJUMP.left->u.CONST.i);
      emit(AS_Oper(ir, TL(cond, NULL), TL(s1, NULL), NULL));
    } else if (!s1) {
      sprintf(ir, "%%`d0 = icmp %s i64 %%`s0, %d", op, s->u.CJUMP.right->u.CONST.i);
      emit(AS_Oper(ir, TL(cond, NULL), TL(s0, NULL), NULL));
    } else {
      sprintf(ir, "%%`d0 = icmp %s i64 %%`s0, %%`s1", op);
      emit(AS_Oper(ir, TL(cond, NULL),
                   TL(s0, TL(s1, NULL)), NULL));
    }
  } else {  // T_float
    switch (s->u.CJUMP.op) {
      case T_eq:
        op = "oeq";
        break;
      case T_ne:
        op = "one";
        break;
      case T_lt:
        op = "olt";
        break;
      case T_le:
        op = "ole";
        break;
      case T_gt:
        op = "ogt";
        break;
      case T_ge:
        op = "oge";
        break;
      default:
        fprintf(stderr, "Error in Cjump!");
        break;
    }
    if (!s0 && !s1) {
      sprintf(ir, "%%`d0 = fcmp %s double %f, %f", op, s->u.CJUMP.left->u.CONST.f,
              s->u.CJUMP.right->u.CONST.f);
      emit(AS_Oper(ir, TL(cond, NULL), NULL, NULL));
    } else if (!s0) {
      sprintf(ir, "%%`d0 = fcmp %s double %f, %%`s0", op, s->u.CJUMP.left->u.CONST.f);
      emit(AS_Oper(ir, TL(cond, NULL), TL(s1, NULL), NULL));
    } else if (!s1) {
      sprintf(ir, "%%`d0 = fcmp %s double %%`s0, %f", op, s->u.CJUMP.right->u.CONST.f);
      emit(AS_Oper(ir, TL(cond, NULL), TL(s0, NULL), NULL));
    } else {
      sprintf(ir, "%%`d0 = fcmp %s double %%`s0, %%`s1", op);
      emit(AS_Oper(ir, TL(cond, NULL),
                   TL(s0, TL(s1, NULL)), NULL));
    }
  }
  sprintf(brch, "br i1 %%`s0, label %%`j0, label %%`j1");
  emit(AS_Oper(brch, NULL, TL(cond, NULL),
               AS_Targets(LL(s->u.CJUMP.t, LL(s->u.CJUMP.f, NULL)))));
}

void llvmMunchMove(T_stm s) {
  string ir;
  if (s->u.MOVE.dst->kind == T_MEM) {
    Temp_temp src = NULL, dst = NULL;
    if (s->u.MOVE.src->kind != T_CONST) {
      src = llvmMunchExp(s->u.MOVE.src);
    }
    if (s->u.MOVE.dst->u.MEM->kind != T_CONST) {
      dst = llvmMunchExp(s->u.MOVE.dst->u.MEM);
    }
    Temp_temp l1 = Temp_newtemp(T_int);
    if (s->u.MOVE.dst->u.MEM->kind == T_CONST) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = inttoptr i64 %d to i64*", s->u.MOVE.dst->u.MEM->u.CONST.i);
      emit(AS_Oper(ir, TL(l1, NULL), NULL, NULL));
    } else {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = inttoptr i64 %%`s0 to i64*");
      emit(AS_Oper(ir, TL(l1, NULL), TL(dst, NULL), NULL));
    }
    if (s->u.MOVE.src->kind == T_CONST) {
      if (s->u.MOVE.src->type == T_int) {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "store i64 %d, i64* %%`s0", s->u.MOVE.src->u.CONST.i);
        emit(AS_Oper(ir, NULL, TL(l1, NULL), NULL));
      } else {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "store double %f, i64* %%`s0", s->u.MOVE.src->u.CONST.f);
        emit(AS_Oper(ir, NULL, TL(l1, NULL), NULL));
      }
    } else {
      ir = (string)checked_malloc(IR_MAXLEN);
      if (src->type == T_int) {
        sprintf(ir, "store i64 %%`s0, i64* %%`s1");
        emit(AS_Oper(ir, NULL, TL(src, TL(l1, NULL)), NULL));
      } else {
        sprintf(ir, "store double %%`s0, i64* %%`s1");
        emit(AS_Oper(ir, NULL, TL(src, TL(l1, NULL)), NULL));
      }
    }
  }
  else if (s->u.MOVE.src->kind == T_MEM) {
    Temp_temp src = NULL;
    Temp_temp dst = llvmMunchExp(s->u.MOVE.dst);
    Temp_temp l1 = Temp_newtemp(T_int);
    if (s->u.MOVE.src->u.MEM->kind == T_CONST) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = inttoptr i64 %d to i64*", s->u.MOVE.src->u.MEM->u.CONST.i);
      emit(AS_Oper(ir, TL(l1, NULL), NULL, NULL));
    } else {
      src = llvmMunchExp(s->u.MOVE.src->u.MEM);
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = inttoptr i64 %%`s0 to i64*");
      emit(AS_Oper(ir, TL(l1, NULL), TL(src, NULL), NULL));
    }
    ir = (string)checked_malloc(IR_MAXLEN);
    if (dst->type == T_int)
      sprintf(ir, "%%`d0 = load i64, i64* %%`s0");
    else
      sprintf(ir, "%%`d0 = load double, i64* %%`s0");
    emit(AS_Oper(ir, TL(dst, NULL), TL(l1, NULL), NULL));
  }
  else if (s->u.MOVE.src->kind == T_TEMP && s->u.MOVE.dst->kind == T_TEMP) {
    Temp_temp src = llvmMunchExp(s->u.MOVE.src);
    Temp_temp dst = llvmMunchExp(s->u.MOVE.dst);
    if (src->type == T_int && dst->type == T_int) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = add i64 %%`s0, 0");
      emit(AS_Move(ir, TL(dst, NULL), TL(src, NULL)));
    } else if (src->type == T_float && dst->type == T_float) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "%%`d0 = fadd double %%`s0, 0.0");
      emit(AS_Move(ir, TL(dst, NULL), TL(src, NULL)));
    } else {
      fprintf(stderr, "Error in Move!\n");
    }
  }
  else {
    Temp_temp src, dst;
    switch (s->u.MOVE.src->kind) {
      case T_BINOP:
        dst = llvmMunchExp(s->u.MOVE.dst);
        llvmMunchBinop(dst, s->u.MOVE.src);
        break;
      case T_CONST:
        dst = llvmMunchExp(s->u.MOVE.dst);
        if (s->u.MOVE.src->type == T_int) {
          ir = (string)checked_malloc(IR_MAXLEN);
          sprintf(ir, "%%`d0 = add i64 %d, 0", s->u.MOVE.src->u.CONST.i);
        } else {  // T_float
          ir = (string)checked_malloc(IR_MAXLEN);
          sprintf(ir, "%%`d0 = fadd double %f, 0.0", s->u.MOVE.src->u.CONST.f);
        }
        emit(AS_Oper(ir, TL(dst, NULL), NULL, NULL));
        break;
      case T_ExtCALL:
        dst = llvmMunchExp(s->u.MOVE.dst);
        llvmMunchExtCall(dst, s->u.MOVE.src);
        break;
      case T_CALL:
        dst = llvmMunchExp(s->u.MOVE.dst);
        llvmMunchCall(dst, s->u.MOVE.src);
        break;
      default:
        src = llvmMunchExp(s->u.MOVE.src);
        dst = llvmMunchExp(s->u.MOVE.dst);
        if (src->type == T_int && dst->type == T_int) {
          ir = (string)checked_malloc(IR_MAXLEN);
          sprintf(ir, "%%`d0 = add i64 %%`s0, 0");
          emit(AS_Move(ir, TL(dst, NULL), TL(src, NULL)));
        } else if (src->type == T_float && dst->type == T_float) {
          ir = (string)checked_malloc(IR_MAXLEN);
          sprintf(ir, "%%`d0 = fadd double %%`s0, 0.0");
          emit(AS_Move(ir, TL(dst, NULL), TL(src, NULL)));
        } else {
          fprintf(stderr, "Error in Move!\n");
        }
        break;
    }
  }
}

Temp_temp llvmMunchBinop(Temp_temp ret, T_exp e) {
  string ir = (string)checked_malloc(IR_MAXLEN);
  Temp_temp s0 = NULL, s1 = NULL;
  if (e->u.BINOP.left->kind != T_CONST) {
    s0 = llvmMunchExp(e->u.BINOP.left);
  }
  if (e->u.BINOP.right->kind != T_CONST) {
    s1 = llvmMunchExp(e->u.BINOP.right);
  }
  // Get the type of operands
  T_type left, right;
  if (!s0 && !s1) {
    left = e->u.BINOP.left->type;
    right = e->u.BINOP.right->type;
  } else if (!s0) {
    left = e->u.BINOP.left->type;
    right = s1->type;
  } else if (!s1) {
    left = s0->type;
    right = e->u.BINOP.right->type;
  } else {
    left = s0->type;
    right = s1->type;
  }
  // Decide the operator and emit the instruction
  string op;
  if (left == T_int && right == T_int) {
    if (ret == NULL) {
      ret = Temp_newtemp(T_int);
    }
    switch (e->u.BINOP.op) {
      case T_plus:
        op = "add";
        break;
      case T_minus:
        op = "sub";
        break;
      case T_mul:
        op = "mul";
        break;
      case T_div:
        op = "sdiv";
        break;
      default:
        fprintf(stderr, "Error in Binop!");
        break;
    }
    if (!s0 && !s1) {
      sprintf(ir, "%%`d0 = %s i64 %d, %d", op, e->u.BINOP.left->u.CONST.i,
              e->u.BINOP.right->u.CONST.i);
      emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
    } else if (!s0) {
      sprintf(ir, "%%`d0 = %s i64 %d, %%`s0", op, e->u.BINOP.left->u.CONST.i);
      emit(AS_Oper(ir, TL(ret, NULL), TL(s1, NULL), NULL));
    } else if (!s1) {
      sprintf(ir, "%%`d0 = %s i64 %%`s0, %d", op, e->u.BINOP.right->u.CONST.i);
      emit(AS_Oper(ir, TL(ret, NULL), TL(s0, NULL), NULL));
    } else {
      sprintf(ir, "%%`d0 = %s i64 %%`s0, %%`s1", op);
      emit(AS_Oper(ir, TL(ret, NULL),
                   TL(s0, TL(s1, NULL)), NULL));
    }
  } else {  // T_float
    if (ret == NULL) {
      ret = Temp_newtemp(T_float);
    }
    switch (e->u.BINOP.op) {
      case T_plus:
        op = "fadd";
        break;
      case T_minus:
        op = "fsub";
        break;
      case T_mul:
        op = "fmul";
        break;
      case T_div:
        op = "fdiv";
        break;
      default:
        fprintf(stderr, "Error in Binop!");
        break;
    }
    if (!s0 && !s1) {
      sprintf(ir, "%%`d0 = %s double %f, %f", op, e->u.BINOP.left->u.CONST.f,
              e->u.BINOP.right->u.CONST.f);
      emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
    } else if (!s0) {
      sprintf(ir, "%%`d0 = %s double %f, %%`s0", op, e->u.BINOP.left->u.CONST.f);
      emit(AS_Oper(ir, TL(ret, NULL), TL(s1, NULL), NULL));
    } else if (!s1) {
      sprintf(ir, "%%`d0 = %s double %%`s0, %f", op, e->u.BINOP.right->u.CONST.f);
      emit(AS_Oper(ir, TL(ret, NULL), TL(s0, NULL), NULL));
    } else {
      sprintf(ir, "%%`d0 = %s double %%`s0, %%`s1", op);
      emit(AS_Oper(ir, TL(ret, NULL),
                   TL(s0, TL(s1, NULL)), NULL));
    }
  }
  return ret;
}
void llvmMunchCall(Temp_temp ret, T_exp e) {
  Temp_temp obj = llvmMunchExp(e->u.CALL.obj);
  Temp_temp tmp1;
  tmp1 = Temp_newtemp(T_int);
  string rettype;
  if (e->type == T_int) {
    rettype = "i64";
  } else {
    rettype = "double";
  }
  string ir1 = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir1, "%%`d0 = inttoptr i64 %%`s0 to i64*");
  emit(AS_Oper(ir1, TL(tmp1, NULL), TL(obj, NULL), NULL));
  string argstr = (string)checked_malloc(IR_MAXLEN);
  Temp_tempList args = llvmMunchArgs(e->u.CALL.args, argstr, TRUE, 1);
  string ir2 = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir2, "%%`d0 = call %s %%`s0(%s)", rettype, argstr);
  emit(AS_Oper(ir2, TL(ret, NULL), TL(tmp1, args), NULL));
}

void llvmMunchExtCall(Temp_temp ret, T_exp e) {
  string func = e->u.ExtCALL.extfun;
  string ir = (string)checked_malloc(IR_MAXLEN);
  string tsf = (string)checked_malloc(IR_MAXLEN);
  if (strcmp(func, "putint") == 0) {
    if (e->u.ExtCALL.args->head->kind == T_CONST) {
      sprintf(ir, "call void @putint(i64 %d)", e->u.ExtCALL.args->head->u.CONST.i);
      emit(AS_Oper(ir, NULL, NULL, NULL));
    } else {
      Temp_temp arg = llvmMunchExp(e->u.ExtCALL.args->head);
      sprintf(ir, "call void @putint(i64 %%`s0)");
      emit(AS_Oper(ir, NULL, TL(arg, NULL), NULL));
    }
  } else if (strcmp(func, "putfloat") == 0) {
    if (e->u.ExtCALL.args->head->kind == T_CONST) {
      sprintf(ir, "call void @putfloat(double %f)", e->u.ExtCALL.args->head->u.CONST.f);
      emit(AS_Oper(ir, NULL, NULL, NULL));
    } else {
      Temp_temp arg = llvmMunchExp(e->u.ExtCALL.args->head);
      sprintf(ir, "call void @putfloat(double %%`s0)");
      emit(AS_Oper(ir, NULL, TL(arg, NULL), NULL));
    }
  } else if (strcmp(func, "putch") == 0) {
    if (e->u.ExtCALL.args->head->kind == T_CONST) {
      sprintf(ir, "call void @putch(i64 %d)", e->u.ExtCALL.args->head->u.CONST.i);
      emit(AS_Oper(ir, NULL, NULL, NULL));
    } else {
      Temp_temp arg = llvmMunchExp(e->u.ExtCALL.args->head);
      sprintf(ir, "call void @putch(i64 %%`s0)");
      emit(AS_Oper(ir, NULL, TL(arg, NULL), NULL));
    }
  } else if (strcmp(func, "putarray") == 0) {
    Temp_temp arrptr = Temp_newtemp(T_int);
    Temp_temp arr = llvmMunchExp(e->u.ExtCALL.args->tail->head);
    sprintf(tsf, "%%`d0 = inttoptr i64 %%`s0 to i64*");
    emit(AS_Oper(tsf, TL(arrptr, NULL), TL(arr, NULL), NULL));
    Temp_temp pos = llvmMunchExp(e->u.ExtCALL.args->head);
    sprintf(ir, "call void @putarray(i64 %%`s0, i64* %%`s1)");
    emit(AS_Oper(ir, NULL, TL(pos, TL(arrptr, NULL)), NULL));
  } else if (strcmp(func, "putfarray") == 0) {
    Temp_temp arrptr = Temp_newtemp(T_int);
    Temp_temp arr = llvmMunchExp(e->u.ExtCALL.args->tail->head);
    sprintf(tsf, "%%`d0 = inttoptr i64 %%`s0 to i64*");
    emit(AS_Oper(tsf, TL(arrptr, NULL), TL(arr, NULL), NULL));
    Temp_temp pos = llvmMunchExp(e->u.ExtCALL.args->head);
    sprintf(ir, "call void @putfarray(i64 %%`s0, i64* %%`s1)");
    emit(AS_Oper(ir, NULL, TL(pos, TL(arrptr, NULL)), NULL));
  } else if (strcmp(func, "getint") == 0) {  // Never used
    sprintf(ir, "%%`d0 = call i64 @getint()");
    emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
  } else if (strcmp(func, "getfloat") == 0) {
    sprintf(ir, "%%`d0 = call double @getfloat()");
    emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
  } else if (strcmp(func, "getch") == 0) {
    sprintf(ir, "%%`d0 = call i64 @getch()");
    emit(AS_Oper(ir, TL(ret, NULL), NULL, NULL));
  } else if (strcmp(func, "getarray") == 0) {
    Temp_temp arrayptr = Temp_newtemp(T_int);
    Temp_temp arr = llvmMunchExp(e->u.ExtCALL.args->head);
    sprintf(tsf, "%%`d0 = inttoptr i64 %%`s0 to i64*");
    emit(AS_Oper(tsf, TL(arrayptr, NULL), TL(arr, NULL), NULL));
    sprintf(ir, "%%`d0 = call i64 @getarray(i64* %%`s0)");
    emit(AS_Oper(ir, TL(ret, NULL), TL(arrayptr, NULL), NULL));
  } else if (strcmp(func, "getfarray") == 0) {
    Temp_temp arrayptr = Temp_newtemp(T_int);
    Temp_temp arr = llvmMunchExp(e->u.ExtCALL.args->head);
    sprintf(tsf, "%%`d0 = inttoptr i64 %%`s0 to i64*");
    emit(AS_Oper(tsf, TL(arrayptr, NULL), TL(arr, NULL), NULL));
    sprintf(ir, "%%`d0 = call i64 @getfarray(i64* %%`s0)");
    emit(AS_Oper(ir, TL(ret, NULL), TL(arrayptr, NULL), NULL));
  } else if (strcmp(func, "malloc") == 0) {
    Temp_temp tmp = Temp_newtemp(T_int);
    if (e->u.ExtCALL.args->head->kind == T_CONST) {
      sprintf(ir, "%%`d0 = call i64* @malloc(i64 %d)",
              e->u.ExtCALL.args->head->u.CONST.i);
      emit(AS_Oper(ir, TL(tmp, NULL), NULL, NULL));
    } else {
      Temp_temp size = llvmMunchExp(e->u.ExtCALL.args->head);
      sprintf(ir, "%%`d0 = call i64* @malloc(i64 %%`s0)");
      emit(AS_Oper(ir, TL(tmp, NULL), TL(size, NULL), NULL));
    }
    sprintf(tsf, "%%`d0 = ptrtoint i64* %%`s0 to i64");
    emit(AS_Oper(tsf, TL(ret, NULL), TL(tmp, NULL), NULL));
  } else if (strcmp(func, "starttime") == 0) {
    sprintf(ir, "call void @starttime()");
    emit(AS_Oper(ir, NULL, NULL, NULL));
  } else if (strcmp(func, "stoptime") == 0) {
    sprintf(ir, "call void @stoptime()");
    emit(AS_Oper(ir, NULL, NULL, NULL));
  } else {
    fprintf(stderr, "Error in munchExtCall!");
  }
}

Temp_tempList llvmMunchArgs(T_expList args, string res, bool first, int i) {
  if (!args) {
    return NULL;
  }
  Temp_temp arg = llvmMunchExp(args->head);
  if (first) {
    if (arg->type == T_int) {
      sprintf(res, "i64 %%`s%d", i);
    } else {
      sprintf(res, "double %%`s%d", i);
    }
    Temp_tempList tmp = llvmMunchArgs(args->tail, res, FALSE, i + 1);
    return Temp_TempList(arg, tmp);
  }
  if (arg->type == T_int) {
    sprintf(res + strlen(res), ", i64 %%`s%d", i);
  } else {
    sprintf(res + strlen(res), ", double %%`s%d", i);
  }
  return Temp_TempList(arg, llvmMunchArgs(args->tail, res, FALSE, i + 1));
}
