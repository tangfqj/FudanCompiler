#include <stdio.h>
#include <string.h>

#include "canon.h"
#include "fdmjast.h"
#include "parser.h"
#include "print_ast.h"
#include "print_irp.h"
#include "print_src.h"
#include "print_stm.h"
#include "semant.h"
#include "util.h"
#include "assem.h"
#include "assemblock.h"
#include "bg.h"
#include "temp.h"
#include "llvmgen.h"

A_prog root;

extern int yyparse();

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    fprintf(stdout, "Usage: %s filename\n", argv[0]);
    return 1;
  }

  // output filename
  string file = checked_malloc(IR_MAXLEN);
  sprintf(file, "%s", argv[1]);
  string file_src = checked_malloc(IR_MAXLEN);
  sprintf(file_src, "%s.1.src", file);
  string file_ast = checked_malloc(IR_MAXLEN);
  sprintf(file_ast, "%s.2.ast", file);
  string file_irp = checked_malloc(IR_MAXLEN);
  sprintf(file_irp, "%s.3.irp", file);
  string file_stm = checked_malloc(IR_MAXLEN);
  sprintf(file_stm, "%s.4.stm", file);
  string file_liv = checked_malloc(IR_MAXLEN);
  sprintf(file_liv, "%s.5.ins", file);
  string file_ins = checked_malloc(IR_MAXLEN);
  sprintf(file_ins, "%s.5.ins", file);

  // lex & parse
  yyparse();
  assert(root);

  // ast2src
  freopen(file_src, "w", stdout);
  printA_Prog(stdout, root);
  fflush(stdout);
  fclose(stdout);

  // ast2xml
  freopen(file_ast, "w", stdout);
  printX_Prog(stdout, root);
  fflush(stdout);
  fclose(stdout);

  // type checking & translate
  T_funcDeclList fdl = transA_Prog(stderr, root, 8);
  // fflush(stdout);
  // fclose(stdout);

  while (fdl) {
    freopen(file_irp, "a", stdout);
    fprintf(stdout, "------Original IR Tree------\n");
    printIRP_set(IRP_parentheses);
    printIRP_FuncDecl(stdout, fdl->head);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    freopen(file_stm, "a", stdout);
    fprintf(stdout, "-----Function %s------\n", fdl->head->name);
    fprintf(stdout, "\n\n");
    T_stm s = fdl->head->stm;
    // canonicalize
    T_stmList sl = C_linearize(s);
    freopen(file_stm, "a", stdout);
    fprintf(stdout, "------Linearized IR Tree------\n");
    printStm_StmList(stdout, sl, 0);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);
    struct C_block b = C_basicBlocks(sl);
    freopen(file_stm, "a", stdout);
    fprintf(stdout, "------Basic Blocks------\n");
    for (C_stmListList sll = b.stmLists; sll; sll = sll->tail) {
      fprintf(stdout, "For Label=%s\n", S_name(sll->head->head->u.LABEL));
      printStm_StmList(stdout, sll->head, 0);
    }
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);
    sl = C_traceSchedule(b);
    freopen(file_stm, "a", stdout);
    fprintf(stdout, "------Canonical IR Tree------\n");
    printStm_StmList(stdout, sl, 0);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    // llvm instruction selection
    AS_instrList prologil = llvmprolog(fdl->head->name, fdl->head->args, fdl->head->rettype);
    AS_blockList bodybl = NULL;
    for (C_stmListList sll = b.stmLists; sll; sll = sll->tail) {
      AS_instrList bil = llvmbody(sll->head);

      AS_blockList bbl = AS_BlockList(AS_Block(bil), NULL);
      bodybl = AS_BlockSplice(bodybl, bbl);
    }
    AS_instrList epilogil = llvmpilog(b.label);

    G_nodeList bg = Create_bg(bodybl);

    freopen(file_ins, "a", stdout);
    fprintf(stdout, "\n------For function %s------\n", fdl->head->name);
    fprintf(stdout, "------Basic Block Graph------\n");
    Show_bg(stdout, bg);

    // put all the blocks into one AS list
    AS_instrList il = AS_traceSchedule(bodybl, prologil, epilogil, FALSE);

    fprintf(stdout, "------~Final traced AS instructions~------\n");
    AS_printInstrList(stdout, il, Temp_name());


    fdl = fdl->tail;
  }
  return 0;
}
