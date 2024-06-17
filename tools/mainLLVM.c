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
#include "ig.h"
#include "ssa.h"

A_prog root;

extern int yyparse();

static void print_to_ssa_file(string file_ssa, AS_instrList il) {
  freopen(file_ssa, "a", stdout);
  AS_printInstrList(stdout, il, Temp_name());
  fclose(stdout);
}


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
  string file_cfg= checked_malloc(IR_MAXLEN);
  sprintf(file_cfg, "%s.6.cfg", file);
  string file_ssa= checked_malloc(IR_MAXLEN);
  sprintf(file_ssa, "%s.7.ssa", file);

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

  while (fdl) {
    T_stm s = fdl->head->stm; //get the statement list of the function
    freopen(file_irp, "a", stdout);
    fprintf(stdout, "------Original IR Tree------\n");
    printIRP_set(IRP_parentheses);
    printIRP_FuncDecl(stdout, fdl->head);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    T_stmList sl = C_linearize(s);
    freopen(file_stm, "a", stdout);
    fprintf(stdout, "------Linearized IR Tree------\n");
    printStm_StmList(stdout, sl, 0);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    struct C_block b = C_basicBlocks(sl); //break the linearized IR tree into basic blocks
    freopen(file_stm, "a", stdout);
    fprintf(stdout, "------Basic Blocks------\n");
    for (C_stmListList sll = b.stmLists; sll; sll = sll->tail) {
      // Each block is a list of statements starting with a label, ending with a jump
      fprintf(stdout, "For Label=%s\n", S_name(sll->head->head->u.LABEL));
      printStm_StmList(stdout, sll->head, 0); //print the statements in the block
    }
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    // llvm instruction selection
    AS_instrList prologil = llvmprolog(fdl->head->name, fdl->head->args, fdl->head->ret_type);
    AS_blockList bodybl = NULL; //making an empty body
    for (C_stmListList sll = b.stmLists; sll; sll = sll->tail) {
      AS_instrList bil = llvmbody(sll->head);
      AS_blockList bbl = AS_BlockList(AS_Block(bil), NULL);
      bodybl = AS_BlockSplice(bodybl, bbl); //putting the block into the list of blocks
    }
    AS_instrList epilogil = llvmepilog(b.label);

    //Now make a control flow graph (CFG) of the function.
    G_nodeList bg = Create_bg(bodybl); // CFG

    freopen(file_ins, "a", stdout);
    fprintf(stdout, "\n------For function ----- %s\n\n", fdl->head->name);
    fprintf(stdout, "------Basic Block Graph---------\n");
    Show_bg(stdout, bg);
    //put all the blocks into one AS list
    AS_instrList il = AS_traceSchedule(bodybl, prologil, epilogil, FALSE);

    printf("------~Final traced AS instructions ---------\n");
    AS_printInstrList(stdout, il, Temp_name());

    AS_instr prologi = il->head;
#ifdef __DEBUG
    fprintf(stderr, "prologi->assem = %s\n", prologi->u.OPER.assem);
    fflush(stderr);
#endif
    AS_instrList bodyil = il->tail;
    il->tail = NULL; // remove the prologi from the inslist_func
    AS_instrList til = bodyil;
    AS_instr epilogi;
    if (til->tail == NULL) {
#ifdef __DEBUG
      fprintf(stderr, "Empty body");
      fflush(stderr);
#endif
      epilogi = til->head;
      bodyil = NULL;
    } else {
      while (til ->tail->tail != NULL) {
#ifdef __DEBUG
        fprintf(stderr, "til->head->kind = %s\n", til->head->u.OPER.assem);
        fflush(stderr);
#endif
        til = til->tail;
      }
      epilogi = til->tail->head;
#ifdef __DEBUG
      fprintf(stderr, "epilogi->assem = %s\n", epilogi->u.OPER.assem);
      fflush(stderr);
#endif
      til->tail=NULL;
    }

#ifdef __DEBUG
    fprintf(stderr, "------ now we've seperated body into prolog, body, and epilog -----\n");
    fflush(stderr);
#endif

    /* doing the control graph and print to *.8.cfg*/
    // get the control flow and print out the control flow graph to *.8.cfg
    G_graph fg = FG_AssemFlowGraph(bodyil);
    freopen(file_cfg, "a", stdout);
    fprintf(stdout, "------Flow Graph------\n");
    fflush(stdout);
    G_show(stdout, G_nodes(fg), (void*)FG_show);
    fflush(stdout);
    fclose(stdout);

    // data flow analysis
    freopen(file_cfg, "a", stdout);
    G_nodeList lg = Liveness(G_nodes(fg));
    freopen(file_cfg, "a", stdout);
    fprintf(stdout, "/* ------Liveness Graph------*/\n");
    Show_Liveness(stdout, lg);
    fflush(stdout);
    fclose(stdout);

    // print the basic block graph again
    freopen(file_cfg, "a", stdout);
    fprintf(stdout, "------Basic Block Graph------\n");
    Show_bg(stdout, bg);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    AS_instrList bodyil_in_SSA = AS_instrList_to_SSA_LLVM(bodyil, lg, bg);

    //print the AS_instrList to the ssa file
    AS_instrList finalssa = AS_splice(AS_InstrList(prologi, bodyil_in_SSA), AS_InstrList(epilogi, NULL));
    print_to_ssa_file(file_ssa, finalssa);
    fdl = fdl->tail;
  }
  // print the runtime functions for the 7.ssa file
  freopen(file_ssa, "a", stdout);
  fprintf(stdout, "declare void @starttime()\n");
  fprintf(stdout, "declare void @stoptime()\n");
  fprintf(stdout, "declare i64* @malloc(i64)\n");
  fprintf(stdout, "declare void @putch(i64)\n");
  fprintf(stdout, "declare void @putint(i64)\n");
  fprintf(stdout, "declare void @putfloat(double)\n");
  fprintf(stdout, "declare i64 @getint()\n");
  fprintf(stdout, "declare i64 @getch(i64)\n");
  fprintf(stdout, "declare float @getfloat()\n");
  fprintf(stdout, "declare i64* @getarray(i64)\n");
  fprintf(stdout, "declare i64* @getfarray(i64)\n");
  fprintf(stdout, "declare void @putarray(i64, i64*)\n");
  fprintf(stdout, "declare void @putfarray(i64, i64*)\n");
  fclose(stdout);

  return 0;
}
