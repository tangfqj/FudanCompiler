#include "translate.h"

extern int SEM_ARCH_SIZE;

/* patchList */

typedef struct patchList_ *patchList;
struct patchList_ {
  Temp_label *head;
  patchList tail;
};

static patchList PatchList(Temp_label *head, patchList tail) {
  patchList p = checked_malloc(sizeof(*p));
  p->head = head;
  p->tail = tail;
  return p;
}

void doPatch(patchList pl, Temp_label tl) {
  for (; pl; pl = pl->tail) *(pl->head) = tl;
}

patchList joinPatch(patchList first, patchList second) {
  if (!first) return second;
  if (!second) return first;
  patchList tmp = first;
  while (tmp->tail) tmp = tmp->tail;
  tmp->tail = second;
  return first;
}

/* Tr_exp */

typedef struct Cx_ *Cx;

struct Cx_ {
  patchList trues;
  patchList falses;
  T_stm stm;
};

struct Tr_exp_ {
  enum { Tr_ex, Tr_nx, Tr_cx } kind;
  union {
    T_exp ex;
    T_stm nx;
    Cx cx;
  } u;
};

static Tr_exp Tr_Ex(T_exp ex) {
  Tr_exp exp = checked_malloc(sizeof(*exp));
  exp->kind = Tr_ex;
  exp->u.ex = ex;
  return exp;
}

static Tr_exp Tr_Nx(T_stm nx) {
  Tr_exp exp = checked_malloc(sizeof(*exp));
  exp->kind = Tr_nx;
  exp->u.nx = nx;
  return exp;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
  Tr_exp exp = checked_malloc(sizeof(*exp));
  exp->kind = Tr_cx;
  exp->u.cx = checked_malloc(sizeof(*(exp->u.cx)));
  exp->u.cx->trues = trues;
  exp->u.cx->falses = falses;
  exp->u.cx->stm = stm;
  return exp;
}

static T_exp unEx(Tr_exp exp) {
  if (!exp) return NULL;
  switch (exp->kind) {
    case Tr_ex:
      return exp->u.ex;
    case Tr_cx: {
      Temp_temp r = Temp_newtemp(T_int);
      Temp_label t = Temp_newlabel();
      Temp_label f = Temp_newlabel();
      Temp_label e = Temp_newlabel();
      doPatch(exp->u.cx->trues, t);
      doPatch(exp->u.cx->falses, f);
      return T_Eseq(
          T_Seq(exp->u.cx->stm,
                T_Seq(T_Label(t),
                      T_Seq(T_Move(T_Temp(r), T_IntConst(1)),
                            T_Seq(T_Jump(e),
                                  T_Seq(T_Label(f),
                                        T_Seq(T_Move(T_Temp(r), T_IntConst(0)),
                                              T_Label(e))))))),
          T_Temp(r));
    }
    case Tr_nx:
      return T_Eseq(exp->u.nx, T_IntConst(0));
    default:
      assert(0);
  }
}

static T_stm unNx(Tr_exp exp) {
  if (!exp) return NULL;
  switch (exp->kind) {
    case Tr_ex:
      return T_Exp(exp->u.ex);
    case Tr_cx:
      return exp->u.cx->stm;
    case Tr_nx:
      return exp->u.nx;
    default:
      assert(0);
  }
}

static Cx unCx(Tr_exp exp) {
  if (!exp) return NULL;
  switch (exp->kind) {
    case Tr_ex: {
      T_stm stm = T_Cjump(T_ne, unEx(exp), T_IntConst(0), NULL, NULL);
      patchList trues = PatchList(&stm->u.CJUMP.t, NULL);
      patchList falses = PatchList(&stm->u.CJUMP.f, NULL);
      Tr_exp cx = Tr_Cx(trues, falses, stm);
      return cx->u.cx;
    }
    case Tr_cx:
      return exp->u.cx;
    default:
      assert(0);
  }
}

// methods
T_funcDeclList Tr_FuncDeclList(T_funcDecl fd, T_funcDeclList fdl) {
  return T_FuncDeclList(fd, fdl);
}

T_funcDeclList Tr_ChainFuncDeclList(T_funcDeclList first,
                                    T_funcDeclList second) {
  if (!first) {
    return second;
  }
  T_funcDeclList h = first;
  while (first->tail) {
    first = first->tail;
  }
  first->tail = second;
  return h;
}

T_funcDecl Tr_MainMethod(Tr_exp vdl, Tr_exp sl) {
  T_stm s;
  if (vdl == NULL && sl == NULL) {
    s = NULL;
  } else if (vdl == NULL) {
    s = unNx(sl);
  } else if (sl == NULL) {
    s = unNx(vdl);
  } else {
    s = T_Seq(unNx(vdl), unNx(sl));
  }
  return T_FuncDecl(String("main"), NULL, s, T_int);
}

T_funcDecl Tr_ClassMethod(string name, Temp_tempList paras, Tr_exp vdl,
                          Tr_exp sl, T_type ret_type) {
  T_stm s;
  if (vdl == NULL && sl == NULL) {
    s = NULL;
  } else if (vdl == NULL) {
    s = unNx(sl);
  } else if (sl == NULL) {
    s = unNx(vdl);
  } else {
    s = T_Seq(unNx(vdl), unNx(sl));
  }
  return T_FuncDecl(String(name), paras, s, ret_type);
}

// stms
Tr_exp Tr_StmList(Tr_exp head, Tr_exp tail) {
  if (head == NULL && tail == NULL) {
    return NULL;
  } else if (head == NULL) {
    return tail;
  } else if (tail == NULL) {
    return head;
  } else {
    return Tr_Nx(T_Seq(unNx(head), unNx(tail)));
  }
}

Tr_exp Tr_IfStm(Tr_exp test, Tr_exp then, Tr_exp elsee) {
  Temp_label t = Temp_newlabel();
  Temp_label f = Temp_newlabel();
  Cx cond = unCx(test);
  doPatch(cond->trues, t);
  doPatch(cond->falses, f);

  if (elsee == NULL) {
    T_stm s1 = unNx(then);
    return Tr_Nx(T_Seq(cond->stm, T_Seq(T_Label(t), T_Seq(s1, T_Label(f)))));
  } else {
    T_stm s1 = unNx(then);
    T_stm s2 = unNx(elsee);
    Temp_label e = Temp_newlabel();
    return Tr_Nx(T_Seq(
        cond->stm,
        T_Seq(T_Label(t),
              T_Seq(s1, T_Seq(T_Jump(e),
                              T_Seq(T_Label(f), T_Seq(s2, T_Label(e))))))));
  }
}

Tr_exp Tr_WhileStm(Tr_exp test, Tr_exp loop, Temp_label whiletest,
                   Temp_label whileend) {
  Cx cond = unCx(test);
  doPatch(cond->falses, whileend);
  if (loop == NULL) {
    doPatch(cond->trues, whiletest);
    return Tr_Nx(
        T_Seq(T_Label(whiletest), T_Seq(cond->stm, T_Label(whileend))));
  }
  Temp_label t = Temp_newlabel();
  doPatch(cond->trues, t);
  return Tr_Nx(T_Seq(
      T_Label(whiletest),
      T_Seq(cond->stm,
            T_Seq(T_Label(t), T_Seq(unNx(loop), T_Seq(T_Jump(whiletest),
                                                      T_Label(whileend)))))));
}

Tr_exp Tr_AssignStm(Tr_exp location, Tr_exp value) {
  return Tr_Nx(T_Move(unEx(location), unEx(value)));
}

Tr_exp Tr_ArrayInit(Tr_exp arr, Tr_expList init) {
  Tr_expList h = init;
  int len = 0;
  while (h) {
    len++;
    h = h->tail;
  }
  h = init;
  Temp_temp arradd = Temp_newtemp(T_int);
  T_stm lenset = T_Move(
      T_Mem(T_Binop(T_plus, T_Temp(arradd), T_IntConst(-SEM_ARCH_SIZE)), T_int),
      T_IntConst(len));
  T_stm s = T_Seq(
      T_Move(T_Temp(arradd),
             T_Binop(T_plus,
                     T_ExtCall(
                         String("malloc"),
                         T_ExpList(T_IntConst((len + 1) * SEM_ARCH_SIZE), NULL),
                         T_int),
                     T_IntConst(SEM_ARCH_SIZE))),
      lenset);
  for (int i = 0; i < len; i++, h = h->tail) {
    T_stm nxt = T_Move(
        T_Mem(T_Binop(T_plus, T_Temp(arradd),
                      T_Binop(T_mul, T_IntConst(i), T_IntConst(SEM_ARCH_SIZE))),
              T_int),
        unEx(h->head));
    s = T_Seq(s, nxt);
  }
  return Tr_Nx(T_Seq(s, T_Move(unEx(arr), T_Temp(arradd))));
}

Tr_exp Tr_CallStm(string meth, Tr_exp clazz, Tr_exp thiz, Tr_expList el,
                  T_type type) {
  T_expList paras = T_ExpList(unEx(thiz), Tr_List(el));
  return Tr_Ex(T_Call(meth, unEx(clazz), paras, type));
}

Tr_exp Tr_Continue(Temp_label whiletest) { return Tr_Nx(T_Jump(whiletest)); }

Tr_exp Tr_Break(Temp_label whileend) { return Tr_Nx(T_Jump(whileend)); }

Tr_exp Tr_Return(Tr_exp ret) { return Tr_Nx(T_Return(unEx(ret))); }

Tr_exp Tr_Putint(Tr_exp exp) {
  T_expList args = T_ExpList(unEx(exp), NULL);
  return Tr_Nx(T_Exp(T_ExtCall(String("putint"), args, T_int)));
}

Tr_exp Tr_Putfloat(Tr_exp exp) {
  T_expList args = T_ExpList(unEx(exp), NULL);
  return Tr_Nx(T_Exp(T_ExtCall(String("putfloat"), args, T_int)));
}

Tr_exp Tr_Putch(Tr_exp exp) {
  T_expList args = T_ExpList(unEx(exp), NULL);
  return Tr_Nx(T_Exp(T_ExtCall(String("putch"), args, T_int)));
}

Tr_exp Tr_Putarray(Tr_exp pos, Tr_exp arr) {
  return Tr_Nx(T_Exp(T_ExtCall(String("putarray"),
                               T_ExpList(unEx(pos), T_ExpList(unEx(arr), NULL)),
                               T_int)));
}

Tr_exp Tr_Putfarray(Tr_exp pos, Tr_exp arr) {
  return Tr_Nx(T_Exp(T_ExtCall(String("putfarray"),
                               T_ExpList(unEx(pos), T_ExpList(unEx(arr), NULL)),
                               T_int)));
}

Tr_exp Tr_Starttime() {
  return Tr_Nx(T_Exp(T_ExtCall(String("starttime"), NULL, T_int)));
}

Tr_exp Tr_Stoptime() {
  return Tr_Nx(T_Exp(T_ExtCall(String("stoptime"), NULL, T_int)));
}

// exps
Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
  Tr_expList tmp = checked_malloc(sizeof(*tmp));
  tmp->head = head;
  tmp->tail = tail;
  return tmp;
}

Tr_exp Tr_OpExp(A_binop op, Tr_exp left, Tr_exp right) {
  // Arithmetic operation
  switch (op) {
    case A_plus:
      return Tr_Ex(T_Binop(T_plus, unEx(left), unEx(right)));
    case A_minus:
      return Tr_Ex(T_Binop(T_minus, unEx(left), unEx(right)));
    case A_times:
      return Tr_Ex(T_Binop(T_mul, unEx(left), unEx(right)));
    case A_div:
      return Tr_Ex(T_Binop(T_div, unEx(left), unEx(right)));
  }
  // Comparison operation
  if (op != A_and && op != A_or) {
    T_relOp rop;
    switch (op) {
      case A_less:
        rop = T_lt;
        break;
      case A_le:
        rop = T_le;
        break;
      case A_greater:
        rop = T_gt;
        break;
      case A_ge:
        rop = T_ge;
        break;
      case A_eq:
        rop = T_eq;
        break;
      case A_ne:
        rop = T_ne;
        break;
    }
    T_stm cond = T_Cjump(rop, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.t, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.f, NULL);
    return Tr_Cx(trues, falses, cond);
  }
  // And, Or -> shortcut
  Cx l = unCx(left);
  Cx r = unCx(right);
  T_stm cond = NULL;
  patchList trues = NULL, falses = NULL;
  if (op == A_and) {
    Temp_label tmpl = Temp_newlabel();
    doPatch(l->trues, tmpl);
    cond = T_Seq(l->stm, T_Seq(T_Label(tmpl), r->stm));
    trues = r->trues;
    falses = joinPatch(l->falses, r->falses);
  } else {  // op == A_or
    Temp_label tmpl = Temp_newlabel();
    doPatch(l->falses, tmpl);
    cond = T_Seq(l->stm, T_Seq(T_Label(tmpl), r->stm));
    trues = joinPatch(l->trues, r->trues);
    falses = r->falses;
  }
  return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_ArrayExp(Tr_exp arr, Tr_exp pos, T_type type) {
  return Tr_Ex(
      T_Mem(T_Binop(T_plus, unEx(arr),
                    T_Binop(T_mul, unEx(pos), T_IntConst(SEM_ARCH_SIZE))),
            type));
}

Tr_exp Tr_CallExp(string meth, Tr_exp clazz, Tr_exp thiz, Tr_expList el,
                  T_type type) {
  T_expList paras = T_ExpList(unEx(thiz), Tr_List(el));
  return Tr_Ex(T_Call(meth, unEx(clazz), paras, type));
}

Tr_exp Tr_ClassVarExp(Tr_exp clazz, int offset, T_type type) {
  return Tr_Ex(T_Mem(
      T_Binop(T_plus, unEx(clazz), T_IntConst(offset * SEM_ARCH_SIZE)), type));
}

Tr_exp Tr_ClassMethExp(Tr_exp clazz, int offset) {
  return Tr_Ex(T_Mem(
      T_Binop(T_plus, unEx(clazz), T_IntConst(offset * SEM_ARCH_SIZE)), T_int));
}

Tr_exp Tr_ClassMethLabel(Temp_label label) { return Tr_Ex(T_Name(label)); }

Tr_exp Tr_BoolConst(bool b) {
  if (b) {
    return Tr_Ex(T_IntConst(1));
  }
  return Tr_Ex(T_IntConst(0));
}

Tr_exp Tr_NumConst(float num, T_type type) {
  if (type == T_int) {
    int temp = (int)num;
    return Tr_Ex(T_IntConst(temp));
  }
  return Tr_Ex(T_FloatConst(num));
}

Tr_exp Tr_IdExp(Temp_temp tmp) { return Tr_Ex(T_Temp(tmp)); }

Tr_exp Tr_ThisExp(Temp_temp tmp) { return Tr_Ex(T_Temp(tmp)); }

Tr_exp Tr_LengthExp(Tr_exp arr) {
  return Tr_Ex(
      T_Mem(T_Binop(T_plus, unEx(arr), T_IntConst(-SEM_ARCH_SIZE)), T_int));
}

Tr_exp Tr_NewArrExp(Tr_exp size) {
  Temp_temp tmpa = Temp_newtemp(T_int);
  Temp_temp tmpl = Temp_newtemp(T_int);
  return Tr_Ex(T_Eseq(
      T_Seq(T_Move(T_Temp(tmpl), unEx(size)),
            T_Seq(T_Move(T_Temp(tmpa),
                         T_Binop(
                             T_plus,
                             T_ExtCall(
                                 String("malloc"),
                                 T_ExpList(T_Binop(T_mul,
                                                   T_Binop(T_plus, T_Temp(tmpl),
                                                           T_IntConst(1)),
                                                   T_IntConst(SEM_ARCH_SIZE)),
                                           NULL),
                                 T_int),
                             T_IntConst(SEM_ARCH_SIZE))),
                  T_Move(T_Mem(T_Binop(T_plus, T_Temp(tmpa),
                                       T_IntConst(-SEM_ARCH_SIZE)),
                               T_int),
                         T_Temp(tmpl)))),
      T_Temp(tmpa)));
}

Tr_exp Tr_NewObjExp(Temp_temp obja, int size, Tr_expList init) {
  T_stm ret = T_Move(
      T_Temp(obja),
      T_ExtCall(String("malloc"),
                T_ExpList(T_IntConst(size * SEM_ARCH_SIZE), NULL), T_int));
  while (init) {
    T_stm s = unNx(init->head);
    ret = T_Seq(ret, s);
    init = init->tail;
  }
  return Tr_Ex(T_Eseq(ret, T_Temp(obja)));
}

Tr_exp Tr_NotExp(Tr_exp exp) {
  T_stm cond = T_Cjump(T_eq, unEx(exp), T_IntConst(0), NULL, NULL);
  patchList trues = PatchList(&cond->u.CJUMP.t, NULL);
  patchList falses = PatchList(&cond->u.CJUMP.f, NULL);
  return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_MinusExp(Tr_exp exp) {
  return Tr_Ex(T_Binop(T_minus, T_IntConst(0), unEx(exp)));
}

Tr_exp Tr_EscExp(Tr_exp stm, Tr_exp exp) {
  if (stm == NULL) {
    return exp;
  }
  return Tr_Ex(T_Eseq(unNx(stm), unEx(exp)));
}

Tr_exp Tr_Getfloat() {
  return Tr_Ex(T_ExtCall(String("getfloat"), NULL, T_float));
}

Tr_exp Tr_Getch() { return Tr_Ex(T_ExtCall(String("getch"), NULL, T_int)); }

Tr_exp Tr_Getarray(Tr_exp exp) {
  return Tr_Ex(
      T_ExtCall(String("getarray"), T_ExpList(unEx(exp), NULL), T_int));
}

Tr_exp Tr_Getfarray(Tr_exp exp) {
  return Tr_Ex(
      T_ExtCall(String("getfarray"), T_ExpList(unEx(exp), NULL), T_int));
}

Tr_exp Tr_Cast(Tr_exp exp, T_type type) {
  return Tr_Ex(T_Cast(unEx(exp), type));
}

T_expList Tr_List(Tr_expList el) {
  if (!el) return NULL;
  T_exp head = unEx(el->head);
  T_expList tail = NULL;
  if (el->tail) {
    tail = Tr_List(el->tail);
  }
  return T_ExpList(head, tail);
}

Tr_exp Tr_NewObjPos(Temp_temp obja, int pos) {
  return Tr_Ex(T_Binop(T_plus, T_Temp(obja), T_IntConst(pos * SEM_ARCH_SIZE)));
}

Tr_exp Tr_AssignNewObj(Tr_exp location, Tr_exp value, T_type type) {
  return Tr_Nx(T_Move(T_Mem(unEx(location), type), unEx(value)));
}