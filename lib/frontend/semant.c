#include "semant.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "temp.h"
#include "tigerirp.h"
#include "util.h"

int SEM_ARCH_SIZE;    // to be set by arch_size in transA_Prog
static S_table cenv;  // class environment
static string MAIN_CLASS = "MAIN_CLASS";
static string curr_class_id;
static string curr_method_id;

#define OFF_SIZE 200
static S_symbol offvar[OFF_SIZE];
static S_symbol offmeth[OFF_SIZE];
static S_table varoff;
static S_table methoff;
static long globalsz = 0;

static S_table venv;  // variable environment

static jstack teststk;  // while loop test stack
static jstack endstk;   // while loop end stack

expty ExpTy(Tr_exp exp, Ty_ty value, Ty_ty location) {
  expty t = checked_malloc(sizeof(*t));
  t->exp = exp;
  t->value = value;
  t->location = location;
  return t;
}

jstack stk_empty(void) {
  jstack stk = checked_malloc(sizeof(*stk));
  stk->top = -1;
  return stk;
}

void push(jstack jstk, Temp_label tmplb) {
  jstk->top++;
  jstk->lb[jstk->top] = tmplb;
}

void pop(jstack jstk) { jstk->top--; }

Temp_label top(jstack jstk) {
  Temp_label tmp = jstk->lb[jstk->top];
  return tmp;
}

bool empty(jstack jstk) { return jstk->top == -1; }

void transError(FILE *out, A_pos pos, string msg) {
  fprintf(out, "(line:%d col:%d) %s\n", pos->line, pos->pos, msg);
  fflush(out);
  exit(1);
}

/* API */

T_funcDeclList transA_Prog(FILE *out, A_prog p, int arch_size) {
  SEM_ARCH_SIZE = arch_size;
  return transSemant(out, p->cdl, p->m);
}

/* preprocess */
void transPreprocess(FILE *out, A_classDeclList cdl) {
  // record class name -> check inheritance
  transA_ClassDeclList_extend(out, cdl);
  // construct vtbl, mtbl; varoff, methoff, globaloff
  transA_ClassDeclList_basic(out, cdl);
}
// trans basic class decl info
void transA_ClassDeclList_basic(FILE *out, A_classDeclList cdl) {
  if (!cdl) {
    return;
  }
  transA_ClassDecl_basic(out, cdl->head);
  if (cdl->tail) {
    transA_ClassDeclList_basic(out, cdl->tail);
  }
}

void transA_ClassDecl_basic(FILE *out, A_classDecl cd) {
  if (!cd) {
    return;
  }
  E_enventry cety = S_look(cenv, S_Symbol(cd->id));
  if (cety->u.cls.status == E_transFill) {
    return;
  }
  S_table vtbl = S_empty();
  S_table mtbl = S_empty();
  // If the class has a father, check father first
  if (cd->parentID) {
    E_enventry facety = S_look(cenv, S_Symbol(cd->parentID));
    transA_ClassDecl_basic(out, facety->u.cls.cd);

    // copy variable table and method table from father
    facety = S_look(cenv, S_Symbol(cd->parentID));
    S_copy(facety->u.cls.vtbl, vtbl);
    S_copy(facety->u.cls.mtbl, mtbl);
  }
  // Then check itself
  curr_class_id = cd->id;
  // Record class variables
  vtbl = transA_ClassVarDeclList_basic(out, vtbl, cd->vdl);
  // Record method signatures
  mtbl = transA_MethodDeclList_basic(out, mtbl, S_Symbol(cd->id), cd->mdl);
  // Update cenv
  cety->u.cls.status = E_transFill;
  cety->u.cls.vtbl = vtbl;
  cety->u.cls.mtbl = mtbl;
  S_enter(cenv, S_Symbol(cd->id), cety);
}

S_table transA_ClassVarDeclList_basic(FILE *out, S_table vtbl,
                                      A_varDeclList vdl) {
  if (!vdl) {
    return vtbl;
  }
  vtbl = transA_ClassVarDecl_basic(out, vtbl, vdl->head);
  if (vdl->tail) {
    return transA_ClassVarDeclList_basic(out, vtbl, vdl->tail);
  }
  return vtbl;
}

S_table transA_ClassVarDecl_basic(FILE *out, S_table vtbl, A_varDecl vd) {
  if (!vd) {
    return vtbl;
  }
  if (S_look(vtbl, S_Symbol(vd->v)) != NULL) {
    transError(out, vd->pos, "Variable redeclaration!");
  }
  switch (vd->t->t) {
    case A_intType:
      S_enter(vtbl, S_Symbol(vd->v), E_VarEntry(vd, Ty_Int(), NULL));
      break;
    case A_floatType:
      S_enter(vtbl, S_Symbol(vd->v), E_VarEntry(vd, Ty_Float(), NULL));
      break;
    case A_intArrType:
      S_enter(vtbl, S_Symbol(vd->v), E_VarEntry(vd, Ty_Array(Ty_Int()), NULL));
      break;
    case A_floatArrType:
      S_enter(vtbl, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Array(Ty_Float()), NULL));
      break;
    case A_idType:
      if (S_look(cenv, S_Symbol(vd->t->id)) == NULL) {
        transError(out, vd->pos, "Undefined class name!");
      }
      S_enter(vtbl, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Name(S_Symbol(vd->t->id)), NULL));
      break;
    default:
      transError(out, vd->pos, "Unknown variable type");
  }
  if (S_look(varoff, S_Symbol(vd->v)) == NULL) {
    S_enter(varoff, S_Symbol(vd->v), (void *)(globalsz + 1));
    offvar[globalsz] = S_Symbol(vd->v);
    globalsz++;
  }
  return vtbl;
}

S_table transA_MethodDeclList_basic(FILE *out, S_table mtbl, S_symbol classid,
                                    A_methodDeclList mdl) {
  if (!mdl) {
    return mtbl;
  }
  mtbl = transA_MethodDecl_basic(out, mtbl, classid, mdl->head);
  if (mdl->tail) {
    return transA_MethodDeclList_basic(out, mtbl, classid, mdl->tail);
  }
  return mtbl;
}

S_table transA_MethodDecl_basic(FILE *out, S_table mtbl, S_symbol classid,
                                A_methodDecl md) {
  if (!md) {
    return mtbl;
  }
  Ty_ty ret = Aty2Ty(md->t);
  Ty_fieldList tfl = transA_FormalList_basic(out, md->fl);

  E_enventry temp_mety = S_look(mtbl, S_Symbol(md->id));
  if (temp_mety != NULL) {
    if (strcmp(S_name(temp_mety->u.meth.from), curr_class_id) == 0) {
      transError(out, md->pos, "Method redeclaration within the same class!");
    }
    Ty_ty temp_ret = temp_mety->u.meth.ret;
    if (cmpTyStrict(out, temp_ret, ret) == FALSE) {
      transError(out, md->pos,
                 "Method redeclaration with incompatible return type!");
    }
    Ty_fieldList temp_tfl = temp_mety->u.meth.fl;
    if (cmpFieldList(out, temp_tfl, tfl) == FALSE) {
      transError(out, md->pos,
                 "Method redeclaration with incompatible formal list!");
    }
  }
  E_enventry mety = E_MethodEntry(md, classid, ret, tfl);
  S_enter(mtbl, S_Symbol(md->id), mety);
  if (S_look(methoff, S_Symbol(md->id)) == NULL) {
    S_enter(methoff, S_Symbol(md->id), (void *)(globalsz + 1));
    offmeth[globalsz] = S_Symbol(md->id);
    globalsz++;
  }
  return mtbl;
}

Ty_fieldList transA_FormalList_basic(FILE *out, A_formalList fl) {
  if (!fl) {
    return NULL;
  }
  Ty_fieldList tfl = checked_malloc(sizeof(*tfl));
  switch (fl->head->t->t) {
    case A_intType:
      tfl->head = Ty_Field(S_Symbol(fl->head->id), Ty_Int());
      break;
    case A_floatType:
      tfl->head = Ty_Field(S_Symbol(fl->head->id), Ty_Float());
      break;
    case A_intArrType:
      tfl->head = Ty_Field(S_Symbol(fl->head->id), Ty_Array(Ty_Int()));
      break;
    case A_floatArrType:
      tfl->head = Ty_Field(S_Symbol(fl->head->id), Ty_Array(Ty_Float()));
      break;
    case A_idType:
      if (S_look(cenv, S_Symbol(fl->head->t->id)) == NULL) {
        transError(out, fl->head->pos, "Undefined class name!");
      }
      tfl->head =
          Ty_Field(S_Symbol(fl->head->id), Ty_Name(S_Symbol(fl->head->t->id)));
  }

  tfl->tail = NULL;
  if (fl->tail) {
    tfl->tail = transA_FormalList_basic(out, fl->tail);
  }
  return tfl;
}

// trans extends class decl info
void transA_ClassDeclList_extend(FILE *out, A_classDeclList cdl) {
  A_classDeclList h = cdl;
  while (h) {
    A_classDecl cd = h->head;
    if (S_look(cenv, S_Symbol(cd->id)) != NULL) {
      transError(out, cd->pos, "Class redeclaration!");
    }
    string fa;
    if (cd->parentID) {
      fa = cd->parentID;
    } else {
      fa = MAIN_CLASS;
    }
    S_enter(cenv, S_Symbol(cd->id),
            E_ClassEntry(cd, S_Symbol(fa), E_transInit, NULL, NULL));
    h = h->tail;
  }
  h = cdl;
  while (h) {
    A_classDecl cd = cdl->head;
    E_enventry cety = S_look(cenv, S_Symbol(cd->id));
    if (cd->parentID != NULL && S_look(cenv, cety->u.cls.fa) == NULL) {
      transError(out, cd->pos, "Inheriting undefined class!");
    }
    h = h->tail;
  }
  // Check circular inheritance
  h = cdl;
  while (h) {
    A_classDecl cd = h->head;
    E_enventry cety = S_look(cenv, S_Symbol(cd->id));
    string fa = S_name(cety->u.cls.fa);
    if (cety->u.cls.status == E_transFind) {
      break;
    }
    while (strcmp(fa, MAIN_CLASS) != 0) {
      cety->u.cls.status = E_transFind;
      if (strcmp(fa, cd->id) == 0) {
        transError(out, cd->pos, "Circular inheritance of class!");
      }
      S_enter(cenv, S_Symbol(cd->id), cety);
      E_enventry facety = S_look(cenv, S_Symbol(fa));
      fa = S_name(facety->u.cls.fa);
    }
    h = h->tail;
  }
}

/* semantic analysis */
T_funcDeclList transSemant(FILE *out, A_classDeclList cdl, A_mainMethod m) {
  T_funcDecl fd = NULL;
  T_funcDeclList fdl = NULL;
  Initialize();
  if (cdl) {
    transPreprocess(out, cdl);
    fdl = transA_ClassDeclList(out, cdl);
  }

  if (m) {
    fd = transA_MainMethod(out, m);
  }
  return Tr_FuncDeclList(fd, fdl);
}

void Initialize() {
  cenv = S_empty();
  venv = S_empty();
  varoff = S_empty();
  methoff = S_empty();
  teststk = stk_empty();
  endstk = stk_empty();
  for (int i = 0; i < OFF_SIZE; i++) {
    offvar[i] = NULL;
    offmeth[i] = NULL;
  }
}

// classes
T_funcDeclList transA_ClassDeclList(FILE *out, A_classDeclList cdl) {
  if (!cdl) {
    return NULL;
  }
  T_funcDeclList first = transA_ClassDecl(out, cdl->head);
  T_funcDeclList second = NULL;
  if (cdl->tail) {
    second = transA_ClassDeclList(out, cdl->tail);
  }
  return Tr_ChainFuncDeclList(first, second);
}

T_funcDeclList transA_ClassDecl(FILE *out, A_classDecl cd) {
  E_enventry cety = S_look(cenv, S_Symbol(cd->id));
  S_table mtbl = cety->u.cls.mtbl;
  curr_class_id = cd->id;
  return transA_MethodDeclList(out, mtbl, cd->mdl);
}

// void transA_ClassVarDeclList(FILE *out, S_table vtbl, A_varDeclList vdl);
// void transA_ClassVarDecl(FILE *out, S_table vtbl, A_varDecl vd);
T_funcDeclList transA_MethodDeclList(FILE *out, S_table mtbl,
                                     A_methodDeclList mdl) {
  if (!mdl) {
    return NULL;
  }
  curr_method_id = mdl->head->id;
  T_funcDecl fd = transA_MethodDecl(out, mtbl, mdl->head);
  T_funcDeclList fdl = NULL;
  if (mdl->tail) {
    fdl = transA_MethodDeclList(out, mtbl, mdl->tail);
  }
  return Tr_FuncDeclList(fd, fdl);
}

T_funcDecl transA_MethodDecl(FILE *out, S_table mtbl, A_methodDecl md) {
  S_beginScope(venv);
  E_enventry mety = S_look(mtbl, S_Symbol(md->id));
  Temp_tempList paras = Temp_TempList(this(), NULL);
  Tr_exp vdl = NULL, sl = NULL;
  if (md->fl) {
    paras->tail = transA_FormalList(out, mety->u.meth.fl, md->fl);
  }
  if (md->vdl) {
    vdl = transA_VarDeclList(out, md->vdl);
  }
  if (md->sl) {
    sl = transA_StmList(out, md->sl);
  }
  S_endScope(venv);
  S_symbol mid = S_link(mety->u.meth.from, S_Symbol(md->id));
  T_type ret_type;
  if (mety->u.meth.ret->kind == Ty_int) {
    ret_type = T_int;
  } else {
    ret_type = T_float;
  }
  return Tr_ClassMethod(S_name(mid), paras, vdl, sl, ret_type);
}

Temp_tempList transA_FormalList(FILE *out, Ty_fieldList fieldList,
                                A_formalList fl) {
  Temp_temp h = transA_Formal(out, fieldList->head, fl->head);
  Temp_tempList t = NULL;
  if (fieldList->tail) {
    t = transA_FormalList(out, fieldList->tail, fl->tail);
  }
  return Temp_TempList(h, t);
}

Temp_temp transA_Formal(FILE *out, Ty_field field, A_formal f) {
  if (S_look(venv, field->name) != NULL) {
    transError(out, f->pos, "Formal redeclaration!");
  }
  Temp_temp ftmp = Temp_newtemp(T_int);
  S_enter(venv, field->name, E_VarEntry(NULL, field->ty, ftmp));
  return ftmp;
}

Tr_exp transA_VarDeclList(FILE *out, A_varDeclList vdl) {
  if (!vdl) {
    return NULL;
  }
  Tr_exp head = transA_VarDecl(out, vdl->head);
  Tr_exp tail = NULL;
  if (vdl->tail) {
    tail = transA_VarDeclList(out, vdl->tail);
  }
  return Tr_StmList(head, tail);
}

Tr_exp transA_VarDecl(FILE *out, A_varDecl vd) {
  if (!vd) {
    return NULL;
  }
  if (S_look(venv, S_Symbol(vd->v)) != NULL) {
    transError(out, vd->pos, "Variable redeclaration!");
  }
  switch (vd->t->t) {
    case A_intType:
      S_enter(venv, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Int(), Temp_newtemp(T_int)));
      break;
    case A_floatType:
      S_enter(venv, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Float(), Temp_newtemp(T_float)));
      break;
    case A_intArrType:
      S_enter(venv, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Array(Ty_Int()), Temp_newtemp(T_int)));
      break;
    case A_floatArrType:
      S_enter(venv, S_Symbol(vd->v),
              E_VarEntry(vd, Ty_Array(Ty_Float()), Temp_newtemp(T_int)));
      break;
    case A_idType:
      if (S_look(cenv, S_Symbol(vd->t->id)) == NULL) {
        transError(out, vd->pos, "Undefined class name!");
      }
      S_enter(
          venv, S_Symbol(vd->v),
          E_VarEntry(vd, Ty_Name(S_Symbol(vd->t->id)), Temp_newtemp(T_int)));
      break;
    default:
      transError(out, vd->pos, "Unknown variable type");
  }

  if (vd->elist) {
    E_enventry vety = S_look(venv, S_Symbol(vd->v));
    Ty_ty type = vety->u.var.ty;
    Tr_exp val, loc;
    Tr_expList arr;
    switch (type->kind) {
      case Ty_int:
        val = transA_Exp_NumConst(vd->elist->head, Ty_Int());
        loc = Tr_IdExp(vety->u.var.tmp);
        return Tr_AssignStm(loc, val);
      case Ty_float:
        val = transA_Exp_NumConst(vd->elist->head, Ty_Float());
        loc = Tr_IdExp(vety->u.var.tmp);
        return Tr_AssignStm(loc, val);
      case Ty_array:
        arr = transA_ExpList_NumConst(vd->elist, type->u.array);
        loc = Tr_IdExp(vety->u.var.tmp);
        return Tr_ArrayInit(loc, arr);
    }
  }
  return NULL;
}

// main method
T_funcDecl transA_MainMethod(FILE *out, A_mainMethod main) {
  S_beginScope(venv);
  curr_class_id = MAIN_CLASS;
  Tr_exp vdl = NULL;
  Tr_exp sl = NULL;
  if (main->vdl) {
    vdl = transA_VarDeclList(out, main->vdl);
  }
  if (main->sl) {
    sl = transA_StmList(out, main->sl);
  }
  S_endScope(venv);
  return Tr_MainMethod(vdl, sl);
}

// stms
Tr_exp transA_StmList(FILE *out, A_stmList sl) {
  if (!sl) {
    return NULL;
  }
  Tr_exp head = transA_Stm(out, sl->head);
  Tr_exp tail = NULL;
  if (sl->tail) {
    tail = transA_StmList(out, sl->tail);
  }
  return Tr_StmList(head, tail);
}

Tr_exp transA_Stm(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  switch (s->kind) {
    case A_nestedStm:
      return transA_NestedStm(out, s);
    case A_ifStm:
      return transA_IfStm(out, s);
    case A_whileStm:
      return transA_WhileStm(out, s);
    case A_assignStm:
      return transA_AssignStm(out, s);
    case A_arrayInit:
      return transA_ArrayInit(out, s);
    case A_callStm:
      return transA_CallStm(out, s);
    case A_continue:
      return transA_Continue(out, s);
    case A_break:
      return transA_Break(out, s);
    case A_return:
      return transA_Return(out, s);
    case A_putnum:
      return transA_Putnum(out, s);
    case A_putch:
      return transA_Putch(out, s);
    case A_putarray:
      return transA_Putarray(out, s);
    case A_starttime:
      return transA_Starttime(out, s);
    case A_stoptime:
      return transA_Stoptime(out, s);
    default:
      transError(out, s->pos, "Unknown statement type!");
  }
  return NULL;
}

Tr_exp transA_NestedStm(FILE *out, A_stm s) {
  return transA_StmList(out, s->u.ns);
}

Tr_exp transA_IfStm(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.if_stat.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, s->pos, "Incompatible if statement's condition!");
  }
  Tr_exp txp1 = transA_Stm(out, s->u.if_stat.s1);
  Tr_exp txp2 = transA_Stm(out, s->u.if_stat.s2);
  return Tr_IfStm(t->exp, txp1, txp2);
}

Tr_exp transA_WhileStm(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.while_stat.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, s->pos, "Incompatible while statement's condition!");
  }
  Temp_label whiletest = Temp_newlabel();
  push(teststk, whiletest);
  Temp_label whileend = Temp_newlabel();
  push(endstk, whileend);
  Tr_exp txp = transA_Stm(out, s->u.while_stat.s);
  pop(endstk);
  pop(teststk);
  return Tr_WhileStm(t->exp, txp, whiletest, whileend);
}

Tr_exp transA_AssignStm(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t1 = transA_Exp(out, s->u.assign.arr, NULL);
  if (t1 == NULL) {
    return NULL;
  }
  if (t1->location == NULL) {
    transError(out, s->pos, "Must assign to a left value!");
  }
  expty t2 = transA_Exp(out, s->u.assign.value, NULL);
  if (t2 == NULL) {
    return NULL;
  }
  Tr_exp val = convertTy(out, s->pos, t1->location, t2);
  return Tr_AssignStm(t1->exp, val);
}

Tr_exp transA_ArrayInit(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.array_init.arr, NULL);
  if (t->value->kind != Ty_array) {
    transError(out, s->pos, "Incompatible argument type! (array expected)");
  }
  Tr_expList txp = transA_ExpList_Num(out, s->u.array_init.init_values,
                                      t->location->u.array);
  return Tr_ArrayInit(t->exp, txp);
}

Tr_exp transA_CallStm(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty obj = transA_Exp(out, s->u.call_stat.obj, NULL);
  if (obj->value->kind != Ty_name) {
    transError(out, s->pos, "Not a class type!");
  }
  E_enventry cety = S_look(cenv, obj->value->u.name);
  S_table mtbl = cety->u.cls.mtbl;
  if (S_look(mtbl, S_Symbol(s->u.call_stat.fun)) == NULL) {
    transError(out, s->pos, "Undefined class method!");
  }
  E_enventry mety = S_look(mtbl, S_Symbol(s->u.call_stat.fun));
  Tr_expList paras =
      transA_ExpList_Call(out, mety->u.meth.fl, s->u.call_stat.el, s->pos);
  long methoffset = (long)S_look(methoff, S_Symbol(s->u.call_stat.fun));
  Tr_exp methadd = Tr_ClassMethExp(obj->exp, methoffset - 1);
  return Tr_CallStm(s->u.call_stat.fun, methadd, obj->exp, paras,
                    T_int);
}

Tr_exp transA_Continue(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  if (empty(teststk)) {
    transError(out, s->pos, "Continue must be in while loop!");
  }
  Temp_label tmpl = top(teststk);
  return Tr_Continue(tmpl);
}

Tr_exp transA_Break(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  if (empty(endstk)) {
    transError(out, s->pos, "Break must be in while loop!");
  }
  Temp_label tmpl = top(endstk);
  return Tr_Break(tmpl);
}

Tr_exp transA_Return(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  Tr_exp txp;
  if (strcmp(curr_class_id, MAIN_CLASS) == 0) {  // main method
    txp = convertTy(out, s->pos, Ty_Int(), t);
  } else {
    E_enventry cety = S_look(cenv, S_Symbol(curr_class_id));
    E_enventry mety = S_look(cety->u.cls.mtbl, S_Symbol(curr_method_id));
    Ty_ty ret = mety->u.meth.ret;
    txp = convertTy(out, s->pos, ret, t);
  }
  return Tr_Return(txp);
}

Tr_exp transA_Putnum(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, s->pos, "Incompatible argument type! (int expected)");
  } else if (t->value->kind == Ty_int) {
    return Tr_Putint(t->exp);
  }
  // Ty_float
  return Tr_Putfloat(t->exp);
}

Tr_exp transA_Putch(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t = transA_Exp(out, s->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, s->pos, "Incompatible argument type! (int expected)");
  }
  Tr_exp txp;
  if (t->value->kind == Ty_int) {
    txp = t->exp;
  } else {
    txp = Tr_Cast(t->exp, T_int);
  }
  return Tr_Putch(txp);
}

Tr_exp transA_Putarray(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  expty t1 = transA_Exp(out, s->u.putarray.e1, NULL);
  expty t2 = transA_Exp(out, s->u.putarray.e2, NULL);
  if (t1 == NULL || t2 == NULL) {
    return NULL;
  }
  if (t1->value->kind != Ty_int && t1->value->kind != Ty_float) {
    transError(out, s->pos, "Incompatible argument type!");
  }
  if (t2->value->kind != Ty_array) {
    transError(out, s->pos, "Incompatible argument type!");
  }
  Tr_exp pos;
  if (t1->value->kind == Ty_int) {
    pos = t1->exp;
  } else {  // Ty_float
    pos = Tr_Cast(t1->exp, T_int);
  }
  if (t2->value->u.array->kind == Ty_int) {
    return Tr_Putarray(pos, t2->exp);
  }
  return Tr_Putfarray(pos, t2->exp);
}

Tr_exp transA_Starttime(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  return Tr_Starttime();
}

Tr_exp transA_Stoptime(FILE *out, A_stm s) {
  if (!s) {
    return NULL;
  }
  return Tr_Stoptime();
}

// exps
Tr_expList transA_ExpList_NumConst(A_expList el, Ty_ty type) {
  Tr_expList head = NULL, tail = NULL;
  Tr_exp txp;
  while (el) {
    txp = transA_Exp_NumConst(el->head, type);
    Tr_expList tmp = Tr_ExpList(txp, NULL);
    if (!head) {
      head = tmp;
      tail = head;
    } else {
      tail->tail = tmp;
      tail = tail->tail;
    }
    el = el->tail;
  }
  return head;
}

Tr_exp transA_Exp_NumConst(A_exp e, Ty_ty type) {
  if (type->kind == Ty_int) {
    return Tr_NumConst(e->u.num, T_int);
  }
  return Tr_NumConst(e->u.num, T_float);
}

Tr_expList transA_ExpList_Num(FILE *out, A_expList el, Ty_ty type) {
  Tr_expList head = NULL, tail = NULL;
  Tr_exp txp;
  while (el) {
    txp = transA_Exp_Num(out, el->head, type);
    Tr_expList tmp = Tr_ExpList(txp, NULL);
    if (!head) {
      head = tmp;
      tail = head;
    } else {
      tail->tail = tmp;
      tail = tail->tail;
    }
    el = el->tail;
  }
  return head;
}

Tr_exp transA_Exp_Num(FILE *out, A_exp e, Ty_ty type) {
  expty t = transA_Exp(out, e, NULL);
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type! (int/float expected)");
  }
  if (t->value->kind == type->kind) {
    return t->exp;
  } else if (type->kind == Ty_int) {
    return Tr_Cast(t->exp, T_int);
  } else {
    return Tr_Cast(t->exp, T_float);
  }
}

Tr_expList transA_ExpList_Call(FILE *out, Ty_fieldList fieldList, A_expList el,
                               A_pos pos) {
  if (!fieldList && !el) {
    return NULL;
  }
  if (!fieldList || !el) {
    transError(out, pos, "Incompatible parameter number!");
  }
  expty t = transA_Exp(out, el->head, NULL);
  if (cmpTy(out, fieldList->head->ty, t->value) == FALSE) {
    transError(out, pos, "Incompatible parameter type!");
  }
  Tr_exp txp;
  if (fieldList->head->ty->kind == Ty_int && t->value->kind == Ty_float) {
    txp = Tr_Cast(t->exp, T_int);
  } else if (fieldList->head->ty->kind == Ty_float &&
             t->value->kind == Ty_int) {
    txp = Tr_Cast(t->exp, T_float);
  } else {
    txp = t->exp;
  }
  Tr_exp head = txp;
  Tr_expList tail = NULL;
  if (fieldList->tail) {
    tail = transA_ExpList_Call(out, fieldList->tail, el->tail, pos);
  }
  return Tr_ExpList(head, tail);
}

expty transA_Exp(FILE *out, A_exp e, Ty_ty type) {
  if (!e) {
    return NULL;
  }
  switch (e->kind) {
    case A_opExp:
      return transA_OpExp(out, e);
    case A_arrayExp:
      return transA_ArrayExp(out, e);
    case A_callExp:
      return transA_CallExp(out, e);
    case A_classVarExp:
      return transA_ClassVarExp(out, e);
    case A_boolConst:
      return transA_BoolConst(out, e);
    case A_numConst:
      return transA_NumConst(out, e);
    case A_lengthExp:
      return transA_LengthExp(out, e);
    case A_idExp:
      return transA_IdExp(out, e);
    case A_thisExp:
      return transA_ThisExp(out, e);
    case A_newIntArrExp:
      return transA_NewIntArrExp(out, e);
    case A_newFloatArrExp:
      return transA_NewFloatArrExp(out, e);
    case A_newObjExp:
      return transA_NewObjExp(out, e);
    case A_notExp:
      return transA_NotExp(out, e);
    case A_minusExp:
      return transA_MinusExp(out, e);
    case A_escExp:
      return transA_EscExp(out, e);
    case A_getnum:
      return transA_Getnum();
    case A_getch:
      return transA_Getch();
    case A_getarray:
      return transA_Getarray(out, e);
    default:
      transError(out, e->pos, "Unknown expression type!");
  }
}

expty transA_OpExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty left = transA_Exp(out, e->u.op.left, NULL);
  expty right = transA_Exp(out, e->u.op.right, NULL);
  if (left == NULL || right == NULL) {
    return NULL;
  }
  if (left->value->kind != Ty_int && left->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible operand type! (int/float expected)");
  }
  if (right->value->kind != Ty_int && right->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible operand type! (int/float expected)");
  }
  Tr_exp txp = Tr_OpExp(e->u.op.oper, left->exp, right->exp);
  if (e->u.op.oper == A_and || e->u.op.oper == A_or) {
    return ExpTy(txp, Ty_Int(), NULL);
  }
  if (left->value->kind == Ty_int && right->value->kind == Ty_int) {
    return ExpTy(txp, Ty_Int(), NULL);
  }
  return ExpTy(txp, Ty_Float(), NULL);
}

expty transA_ArrayExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t1 = transA_Exp(out, e->u.array_pos.arr, NULL);
  expty t2 = transA_Exp(out, e->u.array_pos.arr_pos, NULL);
  if (t1 == NULL || t2 == NULL) {
    return NULL;
  }
  if (t1->value->kind != Ty_array) {
    transError(out, e->pos, "Incompatible argument type!");
  }
  if (t2->value->kind != Ty_int && t2->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type!");
  }
  Tr_exp pos;
  if (t2->value->kind == Ty_int) {
    pos = t2->exp;
  } else {  // Ty_float
    pos = Tr_Cast(t2->exp, T_int);
  }
  Tr_exp txp;
  if (t1->value->u.array->kind == Ty_int) {
    txp = Tr_ArrayExp(t1->exp, pos, T_int);
  } else {
    txp = Tr_ArrayExp(t1->exp, pos, T_float);
  }
  return ExpTy(txp, t1->value->u.array, t1->value->u.array);
}

expty transA_CallExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty obj = transA_Exp(out, e->u.call.obj, NULL);
  if (obj->value->kind != Ty_name) {
    transError(out, e->pos, "Not a class type!");
  }
  E_enventry cety = S_look(cenv, obj->value->u.name);
  S_table mtbl = cety->u.cls.mtbl;
  if (S_look(mtbl, S_Symbol(e->u.call.fun)) == NULL) {
    transError(out, e->pos, "Undefined class method!");
  }
  E_enventry mety = S_look(mtbl, S_Symbol(e->u.call.fun));
  Tr_expList paras =
      transA_ExpList_Call(out, mety->u.meth.fl, e->u.call.el, e->pos);
  long methoffset = (long)S_look(methoff, S_Symbol(e->u.call.fun));
  Tr_exp methadd = Tr_ClassMethExp(obj->exp, methoffset - 1);
  Tr_exp txp;
  if (mety->u.meth.ret->kind == Ty_float) {
    txp = Tr_CallExp(e->u.call.fun, methadd, obj->exp, paras, T_float);
  } else {
    txp = Tr_CallExp(e->u.call.fun, methadd, obj->exp, paras, T_int);
  }
  return ExpTy(txp, mety->u.meth.ret, NULL);
}

expty transA_ClassVarExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty obj = transA_Exp(out, e->u.classvar.obj, NULL);
  if (obj == NULL) {
    return NULL;
  }
  if (obj->value->kind != Ty_name) {
    transError(out, e->pos, "Not a class type!");
  }
  E_enventry cety = S_look(cenv, obj->value->u.name);
  S_table vtbl = cety->u.cls.vtbl;
  if (S_look(vtbl, S_Symbol(e->u.classvar.var)) == NULL) {
    transError(out, e->pos, "Undefined class variable!");
  }
  E_enventry vety = S_look(vtbl, S_Symbol(e->u.classvar.var));
  long varoffset = (long)S_look(varoff, S_Symbol(e->u.classvar.var));
  Tr_exp txp = Tr_ClassVarExp(obj->exp, varoffset - 1);
  return ExpTy(txp, vety->u.var.ty, vety->u.var.ty);
}

expty transA_BoolConst(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  Tr_exp txp = Tr_BoolConst(e->u.b);
  return ExpTy(txp, Ty_Int(), NULL);
}

expty transA_NumConst(FILE *out, A_exp e) {
  // Here type refers to the type you expected
  if (!e) {
    return NULL;
  }
  int temp = (int)e->u.num;
  Tr_exp txp;
  if (temp == e->u.num) {  // int
    txp = Tr_NumConst(e->u.num, T_int);
    return ExpTy(txp, Ty_Int(), NULL);
  }
  // float
  txp = Tr_NumConst(e->u.num, T_float);
  return ExpTy(txp, Ty_Float(), NULL);
}

expty transA_LengthExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_array) {
    transError(out, e->pos, "Incompatible argument type! (array expected)");
  }
  Tr_exp txp = Tr_LengthExp(t->exp);
  return ExpTy(txp, Ty_Int(), NULL);
}

expty transA_IdExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  E_enventry vety = S_look(venv, S_Symbol(e->u.v));
  if (!vety) {
    transError(out, e->pos, "Undefined variable");
  }
  Tr_exp txp = Tr_IdExp(vety->u.var.tmp);
  return ExpTy(txp, vety->u.var.ty, vety->u.var.ty);
}

expty transA_ThisExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  if (strcmp(curr_class_id, MAIN_CLASS) == 0) {
    transError(out, e->pos, "This expression in main method!");
  }
  Tr_exp txp = Tr_ThisExp(this());
  return ExpTy(txp, Ty_Name(S_Symbol(curr_class_id)), NULL);
}

expty transA_NewIntArrExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type! (int expected)");
  }
  Tr_exp sz;
  if (t->value->kind == Ty_int) {
    sz = t->exp;
  } else {  // Ty_float
    sz = Tr_Cast(t->exp, T_int);
  }
  Tr_exp txp = Tr_NewArrExp(sz);
  return ExpTy(txp, Ty_Array(Ty_Int()), NULL);
}

expty transA_NewFloatArrExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type! (int expected)");
  }
  Tr_exp sz;
  if (t->value->kind == Ty_int) {
    sz = t->exp;
  } else {  // Ty_float
    sz = Tr_Cast(t->exp, T_int);
  }
  Tr_exp txp = Tr_NewArrExp(sz);
  return ExpTy(txp, Ty_Array(Ty_Float()), NULL);
}

expty transA_NewObjExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  if (S_look(cenv, S_Symbol(e->u.v)) == NULL) {
    transError(out, e->pos, "Undefined class name!");
  }
  Temp_temp obja = Temp_newtemp(T_int);
  Tr_expList init = transA_NewOjbInit(S_Symbol(e->u.v), obja);
  Tr_exp txp = Tr_NewObjExp(obja, globalsz, init);
  return ExpTy(txp, Ty_Name(S_Symbol(e->u.v)), NULL);
}

expty transA_NotExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type! (int/float expected)");
  }
  Tr_exp txp = Tr_NotExp(t->exp);
  return ExpTy(txp, Ty_Int(), NULL);
}

expty transA_MinusExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_int && t->value->kind != Ty_float) {
    transError(out, e->pos, "Incompatible argument type! (int/float expected)");
  }
  Tr_exp txp = Tr_MinusExp(t->exp);
  if (t->value->kind == Ty_int) {
    return ExpTy(txp, Ty_Int(), NULL);
  }
  return ExpTy(txp, Ty_Float(), NULL);
}

expty transA_EscExp(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  Tr_exp txp1 = transA_StmList(out, e->u.escExp.ns);
  expty t = transA_Exp(out, e->u.escExp.exp, NULL);
  Tr_exp txp2 = Tr_EscExp(txp1, t->exp);
  return ExpTy(txp2, t->value, NULL);
}

expty transA_Getnum() {
  Tr_exp txp = Tr_Getfloat();
  return ExpTy(txp, Ty_Float(), NULL);
}

expty transA_Getch() {
  Tr_exp txp = Tr_Getch();
  return ExpTy(txp, Ty_Int(), NULL);
}

expty transA_Getarray(FILE *out, A_exp e) {
  if (!e) {
    return NULL;
  }
  expty t = transA_Exp(out, e->u.e, NULL);
  if (t == NULL) {
    return NULL;
  }
  if (t->value->kind != Ty_array) {
    transError(out, e->pos, "Incompatible argument type! (array expected)");
  }
  Tr_exp txp;
  if (t->value->u.array->kind == Ty_int) {
    txp = Tr_Getarray(t->exp);
  } else {
    txp = Tr_Getfarray(t->exp);
  }
  return ExpTy(txp, Ty_Int(), NULL);
}

/* Helper methods */
bool isChild(Ty_ty fa, Ty_ty ch) {
  // Assume Ty_kind of fa and ch is Ty_name
  while (strcmp(S_name(fa->u.name), S_name(ch->u.name)) != 0) {
    E_enventry cety = S_look(cenv, ch->u.name);
    if (cety == NULL) {
      return FALSE;
    }
    if (strcmp(S_name(cety->u.cls.fa), "MAIN_CLASS") == 0) {
      return FALSE;
    }
    ch = Ty_Name(cety->u.cls.fa);
  }
  return TRUE;
}

Ty_ty Aty2Ty(A_type aty) {
  switch (aty->t) {
    case A_intType:
      return Ty_Int();
    case A_floatType:
      return Ty_Float();
    case A_intArrType:
      return Ty_Array(Ty_Int());
    case A_floatArrType:
      return Ty_Array(Ty_Float());
    case A_idType:
      return Ty_Name(S_Symbol(aty->id));
  }
}

bool cmpFieldList(FILE *out, Ty_fieldList tfl1, Ty_fieldList tfl2) {
  if (!tfl1 && !tfl2) {
    return TRUE;
  } else if (!tfl1 || !tfl2) {
    return FALSE;
  }
  if (cmpTyStrict(out, tfl1->head->ty, tfl2->head->ty) == FALSE) {
    return FALSE;
  }
  return cmpFieldList(out, tfl1->tail, tfl2->tail);
}

bool cmpTy(FILE *out, Ty_ty t1, Ty_ty t2) {
  // Assume t1 and t2 not NULL
  switch (t1->kind) {
    case Ty_int:
    case Ty_float:
      if (t2->kind == Ty_int || t2->kind == Ty_float) {
        return TRUE;
      } else {
        return FALSE;
      }
    case Ty_array:
      if (t2->kind != Ty_array || t1->u.array->kind != t2->u.array->kind) {
        return FALSE;
      } else {
        return TRUE;
      }
    case Ty_name:
      if (t2->kind != Ty_name) {
        return FALSE;
      }
      if (isChild(t1, t2) == FALSE) {
        return FALSE;
      } else {
        return TRUE;
      }
  }
}

bool cmpTyStrict(FILE *out, Ty_ty t1, Ty_ty t2) {
  if (t1->kind != t2->kind) {
    return FALSE;
  }
  if (t1->kind == Ty_array) {
    if (t1->u.array != t2->u.array) {
      return FALSE;
    }
  }
  if (t1->kind == Ty_name) {
    if (t1->u.name != t2->u.name) {
      return FALSE;
    }
  }
  return TRUE;
}

Tr_exp convertTy(FILE *out, A_pos pos, Ty_ty loc, expty val) {
  switch (loc->kind) {
    case Ty_int:
      if (val->value->kind == Ty_int) {
        return val->exp;
      } else if (val->value->kind == Ty_float) {
        return Tr_Cast(val->exp, T_int);
      } else {
        transError(out, pos, "Incompatible assignment type!");
      }
      break;
    case Ty_float:
      if (val->value->kind == Ty_float) {
        return val->exp;
      } else if (val->value->kind == Ty_int) {
        return Tr_Cast(val->exp, T_float);
      } else {
        transError(out, pos, "Incompatible type!");
      }
      break;
    case Ty_array:
      if (val->value->u.array->kind != loc->u.array->kind) {
        transError(out, pos, "Incompatible type!");
      }
      return val->exp;
    case Ty_name:
      if (val->value->kind != Ty_name) {
        transError(out, pos, "Incompatible type!");
      }
      if (isChild(loc, val->value) == FALSE) {
        transError(out, pos, "Incompatible type!");
      }
      return val->exp;
    default:
      transError(out, pos, "Unknown type!");
  }
}

Tr_expList transA_NewOjbInit(S_symbol classid, Temp_temp obja) {
  E_enventry cety = S_look(cenv, classid);
  S_table vtbl = cety->u.cls.vtbl;
  S_table mtbl = cety->u.cls.mtbl;
  Tr_expList txpl = NULL;
  Tr_exp txp = NULL;
  Tr_exp val, loc;
  for (int i = 0; i < globalsz; i++) {
    S_symbol sym = offvar[i];
    if (sym != NULL && S_look(vtbl, sym) != NULL) {
      E_enventry vety = S_look(vtbl, sym);
      A_varDecl vd = vety->u.var.vd;
      if (vd->elist) {
        Ty_ty type = vety->u.var.ty;
        Tr_expList arr;
        switch (type->kind) {
          case Ty_int:
            val = transA_Exp_NumConst(vd->elist->head, Ty_Int());
            loc = Tr_NewObjPos(obja, i);
            txp = Tr_AssignNewObj(loc, val);
            break;
          case Ty_float:
            val = transA_Exp_NumConst(vd->elist->head, Ty_Float());
            loc = Tr_NewObjPos(obja, i);
            txp = Tr_AssignNewObj(loc, val);
            break;
          case Ty_array:
            arr = transA_ExpList_NumConst(vd->elist, type->u.array);
            loc = Tr_NewObjPos(obja, i);
            txp = Tr_ArrayInit(loc, arr);
            break;
        }
        txpl = Tr_ExpList(txp, txpl);
      }
    }
  }
  for (int i = 0; i < globalsz; i++) {
    S_symbol sym = offmeth[i];
    if (sym != NULL && S_look(mtbl, sym) != NULL) {
      E_enventry mety = S_look(mtbl, sym);
      S_symbol mid = S_link(mety->u.meth.from, S_Symbol(mety->u.meth.md->id));
      Temp_label mlb = Temp_namedlabel(S_name(mid));
      val = Tr_ClassMethLabel(mlb);
      loc = Tr_NewObjPos(obja, i);
      txp = Tr_AssignNewObj(loc, val);
      txpl = Tr_ExpList(txp, txpl);
    }
  }

  return txpl;
}