#include "armgen.h"

#define ARMGEN_DEBUG
#undef ARMGEN_DEBUG

#define ARCH_SIZE 4
#define TYPELEN 10
static string cmpop;
static NumUnion nu;
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

AS_instrList armprolog(AS_instr il) {
  if (!il) return NULL;
  assert(il->kind == I_OPER);
  iList = last = NULL;
  char* assem = il->u.OPER.assem;

  char* def = strtok(assem, " ");
  char* typ = strtok(NULL, " ");
  char* meth = strtok(NULL, "(");

  string ir = (string)checked_malloc(IR_MAXLEN);
  emit(AS_Oper(".text", NULL, NULL, NULL));
  emit(AS_Oper(".align 1", NULL, NULL, NULL));
  sprintf(ir, ".global %s", meth + 1);
  emit(AS_Oper(ir, NULL, NULL, NULL));
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "%s:", meth + 1);
  emit(AS_Oper(ir, NULL, NULL, NULL));
  // push {fp}
  emit(AS_Oper("push {fp}", NULL, NULL, NULL));
  // mov fp, sp
  emit(AS_Oper("mov fp, sp", NULL, NULL, NULL));
  // push {r4, r5, r6, r7, r8, r9, r10, lr}
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "push {%%r4, %%r5, %%r6, %%r7, %%r8, %%r9, %%r10, lr}");
  emit(AS_Oper(ir, NULL, NULL, NULL));
  // get parameters
  int i = 0;
  Temp_tempList paras = il->u.OPER.src;
  while (paras) {
    Temp_temp curr = paras->head;
    Temp_temp r = Temp_reg(i, T_int);
    ir = (string)checked_malloc(IR_MAXLEN);
    if (curr->type == T_int) {
      sprintf(ir, "mov %%`d0, %%r%d", i);
    } else {
      sprintf(ir, "vmov.f32 %%`d0, %%r%d", i);
    }
    emit(AS_Oper(ir, TL(curr, NULL), TL(r, NULL), NULL));
    i++;
    paras = paras->tail;
  }
  return iList;
}

AS_instrList armbody(AS_instrList il) {
  if (!il) return NULL;
  iList = last = NULL;
  cmpop = (string)checked_malloc(IR_MAXLEN);
  while (il) {
    armMunchInstr(il->head);
    il = il->tail;
  }
  return iList;
}

/* Helper methods */
void armMunchInstr(AS_instr inst) {
  AS_type inst_type = gettype(inst);
  string ir = (string)checked_malloc(IR_MAXLEN);
  Temp_temp t = NULL;
  string assem;
  if (inst->kind == I_OPER) {
    assem = inst->u.OPER.assem;
  } else if (inst->kind == I_MOVE) {
    assem = inst->u.MOVE.assem;
  } else {
    assem = inst->u.LABEL.assem;
  }
  //fprintf(stderr, "Munch: %s\n", assem);
  switch (inst_type) {
    case ARM_BR:
      sprintf(ir, "b `j0");
      emit(AS_Oper(ir, NULL, NULL, inst->u.OPER.jumps));
      break;
    case ARM_RET:
      armMunchRet(inst);
      break;
    case ARM_ADD:
      armMunchAdd(inst);
      break;
    case ARM_SUB:
      armMunchSub(inst);
      break;
    case ARM_MUL:
      armMunchMul(inst);
      break;
    case ARM_DIV:
      armMunchDiv(inst);
      break;
    case ARM_FADD:
      armMunchFbinop(inst, "add");
      break;
    case ARM_FSUB:
      armMunchFbinop(inst, "sub");
      break;
    case ARM_FMUL:
      armMunchFbinop(inst, "mul");
      break;
    case ARM_FDIV:
      armMunchFbinop(inst, "div");
      break;
    case ARM_F2I:
      t = Temp_newtemp(T_float);
      sprintf(ir, "vcvt.s32.f32 %%`d0, %%`s0");
      emit(AS_Oper(ir, TL(t, inst->u.OPER.src), inst->u.OPER.src, NULL));
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "vmov.f32 %%`d0, %%`s0");
      emit(AS_Move(ir, inst->u.OPER.dst, TL(t, NULL)));
      break;
    case ARM_I2F:
      t = Temp_newtemp(T_float);
      sprintf(ir, "vmov.f32 %%`d0, %%`s0");
      emit(AS_Move(ir, TL(t, NULL), inst->u.OPER.src));
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "vcvt.f32.s32 %%`d0, %%`s0");
      emit(AS_Oper(ir, TLS(inst->u.OPER.dst, TL(t, NULL)), TL(t, NULL), NULL));
      break;
    case ARM_I2P:
      sprintf(ir, "mov %%`d0, %%`s0");
      emit(AS_Move(ir, inst->u.OPER.dst, inst->u.OPER.src));
      break;
    case ARM_P2I:
      armMunchP2I(inst);
      break;
    case ARM_LOAD:
      armMunchLoad(inst);
      break;
    case ARM_STORE:
      armMunchStore(inst);
      break;
    case ARM_CALL:
      armMunchCall(inst);
      break;
    case ARM_EXTCALL:
      armMunchExtCall(inst);
      break;
    case ARM_ICMP:
      armMunchIcmp(inst);
      break;
    case ARM_FCMP:
      armMunchFcmp(inst);
      break;
    case ARM_LABEL:
      sprintf(ir, "%s:", getlabel(inst));
      emit(AS_Label(ir, inst->u.LABEL.label));
      break;
    case ARM_CJUMP:
      assert(cmpop != NULL);
      sprintf(ir, "b%s `j0", cmpop);
      emit(AS_Oper(ir, NULL, NULL, inst->u.OPER.jumps));
      cmpop = (string)checked_malloc(IR_MAXLEN);
      break;
    case ARM_END:
      break;
    default:
      fprintf(stderr, "Unknown type! %s\n", assem);
      break;
  }
}

void armMunchRet(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* op = strtok(assem, " ");
  char* typ = strtok(NULL, " ");
  char* ret = strtok(NULL, " ");

  string ir = (string)checked_malloc(IR_MAXLEN);
  Temp_temp r0 = Temp_reg(0, T_int);
  if (ret[0] == '%' && ret[1] == '`') {
    if (inst->u.OPER.src->head->type == T_int) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "mov %%r0, %%`s0");
      emit(AS_Move(ir, TL(r0, NULL), inst->u.OPER.src));
    } else {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "vmov.f32 %%r0, %%`s0");
      emit(AS_Oper(ir, TL(r0, NULL), inst->u.OPER.src, NULL));
    }
  } else if (!strcmp(typ, "i64")) {
    int ret_i = atoi(ret);
    moveInt(ret_i, TL(r0, NULL));
  } else {
    float ret_f = atof(ret);
    moveFloat(ret_f, TL(r0, NULL));
  }
  // pop r4-r10, lr
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "pop {%%r4, %%r5, %%r6, %%r7, %%r8, %%r9, %%r10, lr}");
  emit(AS_Oper(ir, NULL, NULL, NULL));
  emit(AS_Oper("pop {fp}", NULL, NULL, NULL));
  emit(AS_Oper("bx lr", NULL, TL(r0, NULL), NULL));
}
void armMunchAdd(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);

  int left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  } else if (!left) {
    left_num = atoi(leftOperand);
    left = Temp_newtemp(T_int);
    moveInt(left_num, TL(left, NULL));
  } else if (!right) {
    right_num = atoi(rightOperand);
    right = Temp_newtemp(T_int);
    moveInt(right_num, TL(right, NULL));
  }
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "add %%`d0, %%`s0, %%`s1");
  emit(AS_Oper(ir, inst->u.OPER.dst, TL(left, TL(right, NULL)), NULL));
}

void armMunchSub(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);
  if (left && right) {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "sub %%`d0, %%`s0, %%`s1");
    emit(AS_Oper(ir, inst->u.OPER.dst, TL(left, TL(right, NULL)), NULL));
    return;
  }

  int left_num, right_num;
  if (!left && !right) {
    left_num = atoi(leftOperand);
    right_num = atoi(rightOperand);
    if (left_num == 0) {
      Temp_temp rtmp = Temp_newtemp(T_int);
      moveInt(right_num, TL(rtmp, NULL));
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "rsb %%`d0, %%`s0, #0");
      emit(AS_Oper(ir, inst->u.OPER.dst, TL(rtmp, NULL), NULL));
      return;
    }
    Temp_temp ltmp = Temp_newtemp(T_int);
    moveInt(left_num, TL(ltmp, NULL));
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "sub %%`d0, %%`s0, #%d", right_num);
    emit(AS_Oper(ir, inst->u.OPER.dst, TL(ltmp, NULL), NULL));
  } else if (!left) {
    left_num = atoi(leftOperand);
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "rsb %%`d0, %%`s0, #%d", left_num);
    emit(AS_Oper(ir, inst->u.OPER.dst, TL(right, NULL), NULL));
  } else if (!right) {
    right_num = atoi(rightOperand);
    if (right_num == 0) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "mov %%`d0, %%`s0");
      emit(AS_Oper(ir, inst->u.OPER.dst, TL(left, NULL), NULL));
      return;
    }
    Temp_temp rtmp = Temp_newtemp(T_int);
    moveInt(right_num, TL(rtmp, NULL));
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "sub %%`d0, %%`s0, %%`s1");
    emit(AS_Move(ir, inst->u.OPER.dst, TL(left, TL(rtmp, NULL))));
  }
}

void armMunchMul(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);
  int left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  } else if (!left) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
  } else if (!right) {
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  }

  sprintf(ir, "mul %%`d0, %%`s0, %%`s1");
  emit(AS_Oper(ir, inst->u.OPER.dst, TL(left, TL(right, NULL)), NULL));
}

void armMunchDiv(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;
  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  int left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  } else if (!left) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
  } else if (!right) {
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  }

  callerSaveCall();
  Temp_temp r0 = Temp_reg(0, T_int);
  Temp_temp r1 = Temp_reg(1, T_int);
  emit(AS_Move("mov %r0, %`s0", TL(r0, NULL), TL(left, NULL)));
  emit(AS_Move("mov %r1, %`s0", TL(r1, NULL), TL(right, NULL)));
  emit(AS_Oper("blx __aeabi_idiv", NULL, NULL, NULL));
  callerSaveRet();
  emit(AS_Move("mov %`d0, %r0", inst->u.OPER.dst, TL(r0, NULL)));
}
void armMunchP2I(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");
  string src = strtok(NULL, " ");

  string ir = (string)checked_malloc(IR_MAXLEN);
  if (src[0] == '%' && src[1] == '`') {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "mov %%`d0, %%`s0");
    emit(AS_Move(ir, inst->u.OPER.dst, inst->u.OPER.src));
  } else if (src[0] == '@') {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "ldr %%`d0, =%s", src + 1);
    emit(AS_Oper(ir, inst->u.OPER.dst, NULL, NULL));
  } else
    assert(0);
}
void armMunchLoad(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, ", ");

  string ir = (string)checked_malloc(IR_MAXLEN);
  if (!strcmp(typ, "i64")) {
    sprintf(ir, "ldr %%`d0, [%%`s0]");
    emit(AS_Oper(ir, Temp_TempListSplice(inst->u.OPER.dst, inst->u.OPER.src),
                 inst->u.OPER.src, NULL));
  } else {
    sprintf(ir, "vldr.f32 %%`d0, [%%`s0]");
    emit(AS_Oper(ir, Temp_TempListSplice(inst->u.OPER.dst, inst->u.OPER.src),
                 inst->u.OPER.src, NULL));
  }
}
void armMunchStore(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* op = strtok(assem, " ");
  char* typ = strtok(NULL, " ");
  char* src = strtok(NULL, ", ");

  string ir = (string)checked_malloc(IR_MAXLEN);

  if (!strcmp(typ, "i64")) {
    if (src[0] == '%' && src[1] == '`') {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "str %%`s0, [%%`s1]");
      emit(AS_Oper(ir, NULL, TLS(inst->u.OPER.src, inst->u.OPER.dst), NULL));
    } else {
      int src_num = atoi(src);
      Temp_temp src_tmp = Temp_newtemp(T_int);
      moveInt(src_num, TL(src_tmp, NULL));
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "str %%`s0, [%%`s1]");
      emit(AS_Oper(ir, NULL, TL(src_tmp, inst->u.OPER.src), NULL));
    }
  } else {
    if (src[0] == '%' && src[1] == '`') {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "vstr.f32 %%`s0, [%%`s1]");
      emit(AS_Oper(ir, NULL, TLS(inst->u.OPER.src, inst->u.OPER.dst), NULL));
    } else {
      float src_num = atof(src);
      Temp_temp src_tmp = Temp_newtemp(T_float);
      moveFloat(src_num, TL(src_tmp, NULL));
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "vstr.f32 %%`s0, [%%`s1]");
      emit(AS_Oper(ir, NULL, TL(src_tmp, inst->u.OPER.src), NULL));
    }
  }
}

void armMunchCall(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* cll = strtok(equal + 1, " ");
  T_type ret = T_int;
  char* typ = strtok(NULL, " ");
  if (!strcmp(typ, "i64") || !strcmp(typ, "i64*")) {
    ret = T_int;
  } else if (!strcmp(typ, "double")) {
    ret = T_float;
  }
  char* meth = strtok(NULL, "(");

  string ir = (string)checked_malloc(IR_MAXLEN);
  int i = 0;

  callerSaveCall();

  Temp_tempList tmpl = inst->u.OPER.src;
  while (tmpl) {
    typ = strtok(NULL, " ");
    if (!typ) break;
    char* src = strtok(NULL, ",)");
    if (!strcmp(typ, "i64")) {
      if (src[0] == '%' && src[1] == '`') {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "mov %%r%d, %%`s0", i);
        Temp_temp para = nthTemp(tmpl, src[3] - '0');
        emit(AS_Move(ir, TL(Temp_reg(i, T_int), NULL), TL(para, NULL)));
      } else {
        int src_num = atoi(src);
        moveInt(src_num, TL(Temp_reg(i, T_int), NULL));
      }
    } else {
      if (src[0] == '%' && src[1] == '`') {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "vmov.f32 %%r%d, %%`s0", i);
        Temp_temp para = nthTemp(tmpl, src[3] - '0');
        emit(AS_Move(ir, TL(Temp_reg(i, T_int), NULL), TL(para, NULL)));
      } else {
        float src_num = atof(src);
        moveFloat(src_num, TL(Temp_reg(i, T_float), NULL));
      }
    }
    i++;
  }
  if (!strcmp(meth, "@malloc") && inst->u.OPER.src == NULL) {
    typ = strtok(NULL, " ");
    char* src = strtok(NULL, ")");
    int src_num = atoi(src);
    moveInt(src_num, TL(Temp_reg(0, T_int), NULL));
  }
  if (meth[0] == '@') {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "blx %s", meth + 1);
    emit(AS_Oper(ir, NULL, NULL, NULL));
  } else {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "blx %%`s0");
    Temp_temp meth_tmp = inst->u.OPER.src->head;
    emit(AS_Oper(ir, TL(Temp_reg(0, T_int), NULL), TL(meth_tmp, NULL), NULL));
  }

  callerSaveRet();

  Temp_temp r0 = Temp_reg(0, T_int);
  if (!strcmp(meth, "@getfloat")) {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "vmov.f32 %%`d0, %%`s0");
    emit(AS_Move(ir, inst->u.OPER.dst, TL(Temp_reg(0, T_float), NULL)));
  } else if (ret == T_int) {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "mov %%`d0, %%r0");
    emit(AS_Move(ir, inst->u.OPER.dst, TL(r0, NULL)));
  } else {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "vmov.f32 %%`d0, %%r0");
    emit(AS_Move(ir, inst->u.OPER.dst, TL(r0, NULL)));
  }
}

void armMunchExtCall(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* cll = strtok(assem, " ");
  char* typ = strtok(NULL, " ");
  char* meth = strtok(NULL, "(");

  string ir = (string)checked_malloc(IR_MAXLEN);
  int i = 0;
  Temp_tempList tmpl = inst->u.OPER.src;
  callerSaveCall();
  while (tmpl) {
    typ = strtok(NULL, " ");
    if (!typ) break;
    char* src = strtok(NULL, ",)");
    if (!strcmp(typ, "i64")) {
      if (src[0] == '%' && src[1] == '`') {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "mov %%r%d, %%`s0", i);
        Temp_temp para = nthTemp(tmpl, src[3] - '0');
        emit(AS_Move(ir, TL(Temp_reg(i, T_int), NULL), TL(para, NULL)));
      } else {
        int src_num = atoi(src);
        moveInt(src_num, TL(Temp_reg(i, T_int), NULL));
      }
    } else {
      if (src[0] == '%' && src[1] == '`') {
        ir = (string)checked_malloc(IR_MAXLEN);
        sprintf(ir, "vmov.f32 %%r%d, %%`s0", i);
        Temp_temp para = nthTemp(tmpl, src[3] - '0');
        emit(AS_Move(ir, TL(Temp_reg(i, T_int), NULL), TL(para, NULL)));
      } else {
        float src_num = atof(src);
        moveFloat(src_num, TL(Temp_reg(i, T_float), NULL));
      }
    }
    i++;
  }
  assert(meth[0] == '@');
  if (!strcmp(meth, "@putch") && inst->u.OPER.src == NULL) {
    typ = strtok(NULL, " ");
    char* src = strtok(NULL, ")");
    int src_num = atoi(src);
    moveInt(src_num, TL(Temp_reg(0, T_int), NULL));
  }
  if (!strcmp(meth, "@putint") && inst->u.OPER.src == NULL) {
    typ = strtok(NULL, " ");
    char* src = strtok(NULL, ")");
    int src_num = atoi(src);
    moveInt(src_num, TL(Temp_reg(0, T_int), NULL));
  }
  if (!strcmp(meth, "@putfloat") && inst->u.OPER.src == NULL) {
    typ = strtok(NULL, " ");
    char* src = strtok(NULL, ")");
    float src_num = atof(src);
    moveFloat(src_num, TL(Temp_reg(0, T_float), NULL));
  }
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "bl %s", meth + 1);
  emit(AS_Oper(ir, TL(Temp_reg(0, T_int), NULL), NULL, NULL));
  callerSaveRet();
}
void armMunchIcmp(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* op1 = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");
  char* op2 = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = inst->u.OPER.src->head;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = inst->u.OPER.src->tail->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);
  int left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  } else if (!left) {
    left = Temp_newtemp(T_int);
    left_num = atoi(leftOperand);
    moveInt(left_num, TL(left, NULL));
  } else if (!right) {
    right = Temp_newtemp(T_int);
    right_num = atoi(rightOperand);
    moveInt(right_num, TL(right, NULL));
  }

  sprintf(ir, "cmp %%`s0, %%`s1");
  emit(AS_Oper(ir, NULL, TL(left, TL(right, NULL)), NULL));
  // record op code
  if (!strcmp(typ, "eq")) {
    cmpop = "eq";
  } else if (!strcmp(typ, "ne")) {
    cmpop = "ne";
  } else if (!strcmp(typ, "slt")) {
    cmpop = "lt";
  } else if (!strcmp(typ, "sle")) {
    cmpop = "le";
  } else if (!strcmp(typ, "sgt")) {
    cmpop = "gt";
  } else if (!strcmp(typ, "sge")) {
    cmpop = "ge";
  }
}

void armMunchFcmp(AS_instr inst) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* op1 = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");
  char* op2 = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);
  float left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_float);
    left_num = atof(leftOperand);
    moveFloat(left_num, TL(left, NULL));
    right = Temp_newtemp(T_float);
    right_num = atof(rightOperand);
    moveFloat(right_num, TL(right, NULL));
  } else if (!left) {
    left = Temp_newtemp(T_float);
    left_num = atof(leftOperand);
    moveFloat(left_num, TL(left, NULL));
  } else if (!right) {
    right = Temp_newtemp(T_float);
    right_num = atof(rightOperand);
    moveFloat(right_num, TL(right, NULL));
  }

  sprintf(ir, "vcmp.f32 %%`s0, %%`s1");
  emit(AS_Oper(ir, NULL, TL(left, TL(right, NULL)), NULL));
  emit(AS_Oper("vmrs APSR_nzcv, FPSCR", NULL, NULL, NULL));

  // record op code
  if (!strcmp(typ, "oeq")) {
    cmpop = "eq";
  } else if (!strcmp(typ, "one")) {
    cmpop = "ne";
  } else if (!strcmp(typ, "olt")) {
    cmpop = "lt";
  } else if (!strcmp(typ, "ole")) {
    cmpop = "le";
  } else if (!strcmp(typ, "ogt")) {
    cmpop = "gt";
  } else if (!strcmp(typ, "oge")) {
    cmpop = "ge";
  }
}

void armMunchFbinop(AS_instr inst, string bop) {
  assert(inst->kind == I_OPER);
  char* assem = inst->u.OPER.assem;

  char* equal = strchr(assem, '=');
  char* opcode = strtok(equal + 1, " ");
  char* typ = strtok(NULL, " ");

  string leftOperand = strtok(NULL, ", ");
  string rightOperand = strtok(NULL, ", ");
  Temp_temp left = NULL, right = NULL;
  Temp_tempList src = inst->u.OPER.src;
  if (leftOperand[0] == '%' && leftOperand[1] == '`') {
    left = src->head;
    src = src->tail;
  }
  if (rightOperand[0] == '%' && rightOperand[1] == '`') {
    right = src->head;
  }

  string ir = (string)checked_malloc(IR_MAXLEN);
  float left_num, right_num;
  if (!left && !right) {
    left = Temp_newtemp(T_float);
    left_num = atof(leftOperand);
    moveFloat(left_num, TL(left, NULL));
    right = Temp_newtemp(T_float);
    right_num = atof(rightOperand);
    moveFloat(right_num, TL(right, NULL));
  } else if (!left) {
    left = Temp_newtemp(T_float);
    left_num = atof(leftOperand);
    moveFloat(left_num, TL(left, NULL));
  } else if (!right) {
    right = Temp_newtemp(T_float);
    right_num = atof(rightOperand);
    moveFloat(right_num, TL(right, NULL));
  }

  sprintf(ir, "v%s.f32 %%`d0, %%`s0, %%`s1", bop);
  emit(AS_Oper(ir, inst->u.OPER.dst, TL(left, TL(right, NULL)), NULL));
}

AS_type gettype(AS_instr ins) {
  AS_type ret = ARM_NONE;
  string assem = ins->u.OPER.assem;
  if (ins->kind == I_MOVE) {
    assem = ins->u.MOVE.assem;
  } else if (ins->kind == I_LABEL) {
    ret = ARM_LABEL;
    return ret;
  }
  if (!strncmp(assem, "br label", 6)) {
    ret = ARM_BR;
    return ret;
  } else if (!strncmp(assem, "ret", 3)) {
    ret = ARM_RET;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fadd", TYPELEN)) {
    ret = ARM_FADD;
    return ret;
  } else if (!strncmp(assem, "%`d0 = add", TYPELEN)) {
    ret = ARM_ADD;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fsub", TYPELEN)) {
    ret = ARM_FSUB;
    return ret;
  } else if (!strncmp(assem, "%`d0 = sub", TYPELEN)) {
    ret = ARM_SUB;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fmul", TYPELEN)) {
    ret = ARM_FMUL;
    return ret;
  } else if (!strncmp(assem, "%`d0 = mul", TYPELEN)) {
    ret = ARM_MUL;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fdiv", TYPELEN)) {
    ret = ARM_FDIV;
    return ret;
  } else if (!strncmp(assem, "%`d0 = sdiv", TYPELEN)) {
    ret = ARM_DIV;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fptosi", TYPELEN)) {
    ret = ARM_F2I;
    return ret;
  } else if (!strncmp(assem, "%`d0 = sitofp", TYPELEN)) {
    ret = ARM_I2F;
    return ret;
  } else if (!strncmp(assem, "%`d0 = inttoptr", TYPELEN)) {
    ret = ARM_I2P;
    return ret;
  } else if (!strncmp(assem, "%`d0 = load", TYPELEN)) {
    ret = ARM_LOAD;
    return ret;
  } else if (!strncmp(assem, "store", 5)) {
    ret = ARM_STORE;
    return ret;
  } else if (!strncmp(assem, "%`d0 = ptrtoint", TYPELEN)) {
    ret = ARM_P2I;
    return ret;
  } else if (!strncmp(assem, "%`d0 = call", TYPELEN)) {
    ret = ARM_CALL;
    return ret;
  } else if (!strncmp(assem, "call", 4)) {
    ret = ARM_EXTCALL;
    return ret;
  } else if (!strncmp(assem, "%`d0 = icmp", TYPELEN)) {
    ret = ARM_ICMP;
    return ret;
  } else if (!strncmp(assem, "%`d0 = fcmp", TYPELEN)) {
    ret = ARM_FCMP;
    return ret;
  } else if (!strncmp(assem, "br i1 %`s0", TYPELEN)) {
    ret = ARM_CJUMP;
    return ret;
  } else if (!strncmp(assem, "}", 1)) {
    ret = ARM_END;
    return ret;
  }
  return ret;
}

static string getlabel(AS_instr inst) {
  assert(inst->kind == I_LABEL);
  return Temp_labelstring(inst->u.LABEL.label);
}

static Temp_temp nthTemp(Temp_tempList list, int i) {
  assert(list);
  if (i == 0)
    return list->head;
  else
    return nthTemp(list->tail, i - 1);
}

void moveInt(int i, Temp_tempList dst) {
  string ir = (string)checked_malloc(IR_MAXLEN);
  if (i >= 0) {
    if (dst->head != NULL && dst->head->num < 99) {
      ir = (string)checked_malloc(IR_MAXLEN);
      sprintf(ir, "mov %%r%d, #%d", dst->head->num, i);
      emit(AS_Oper(ir, dst, NULL, NULL));
      return;
    }
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "mov %%`d0, #%d", i);
    emit(AS_Oper(ir, dst, NULL, NULL));
    return;
  }

  nu.i = i;

  char s[20];
  sprintf(s, "%.8x", nu.u);

  if (dst->head != NULL && dst->head->num < 99) {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "movw %%r%d, #0x%.4s", dst->head->num, &s[4]);
    emit(AS_Oper(ir, dst, NULL, NULL));
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "movt %%r%d, #0x%.4s", dst->head->num, s);
    emit(AS_Oper(ir, dst, NULL, NULL));
    return;
  }
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "movw %%`d0, #0x%.4s", &s[4]);
  emit(AS_Oper(ir, dst, NULL, NULL));
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "movt %%`d0, #0x%.4s", s);
  emit(AS_Oper(ir, dst, NULL, NULL));
}
void moveFloat(float f, Temp_tempList dst) {
  nu.f = f;
  string ir = (string)checked_malloc(IR_MAXLEN);

  Temp_temp t = Temp_newtemp(T_int);
  char s[20];
  sprintf(s, "%.8x", nu.u);

  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "movw %%`d0, #0x%.4s", &s[4]);
  emit(AS_Oper(ir, TL(t, NULL), NULL, NULL));
  ir = (string)checked_malloc(IR_MAXLEN);
  sprintf(ir, "movt %%`d0, #0x%.4s", s);
  emit(AS_Oper(ir, TL(t, NULL), NULL, NULL));

  if (dst->head != NULL && dst->head->num < 99) {
    ir = (string)checked_malloc(IR_MAXLEN);
    sprintf(ir, "vmov.f32 %%s%d, %%`s0", dst->head->num);
    emit(AS_Move(ir, dst, TL(t, NULL)));
    return;
  }

  emit(AS_Move("vmov.f32 %`d0, %`s0", dst, TL(t, NULL)));
}

void callerSaveCall() {
  // save r1-r3
  emit(AS_Oper("push {%r1, %r2, %r3}", NULL, NULL, NULL));
}

void callerSaveRet() {
  // restore r1-r3
  emit(AS_Oper("pop {%r1, %r2, %r3}", NULL, NULL, NULL));
}