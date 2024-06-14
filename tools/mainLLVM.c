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

AS_instrList AS_instrList_to_SSA(AS_instrList bodyil, G_nodeList lg, G_nodeList bg);

static void print_to_ssa_file(string file_ssa, AS_instrList il) {
  freopen(file_ssa, "a", stdout);
  AS_printInstrList(stdout, il, Temp_name());
  fclose(stdout);
}

static AS_blockList instrList2BL(AS_instrList il) {
  AS_instrList b = NULL;
  AS_blockList bl = NULL;
  AS_instrList til = il;

  while (til) {
    if (til->head->kind == I_LABEL) {
      if (b) { //if we have a label but the current block is not empty, then we have to stop the block
        Temp_label l = til->head->u.LABEL.label;
        b = AS_splice(b, AS_InstrList(AS_Oper(String("br label `j0"), NULL, NULL, AS_Targets(Temp_LabelList(l, NULL))), NULL));
        //add a jump to the block to be stopped, only for LLVM IR
        bl = AS_BlockSplice(bl, AS_BlockList(AS_Block(b), NULL)); //add the block to the block list
#ifdef __DEBUG
        fprintf(stderr, "1----Start a new Block %s\n", Temp_labelstring(l)); fflush(stderr);
#endif
        b = NULL; //start a new block
      }
    }

    assert(b||til->head->kind == I_LABEL);  //if not a label to start a block, something's wrong!

#ifdef __DEBUG
    if (!b && til->head->kind == I_LABEL)
      fprintf(stderr, "2----Start a new Block %s\n", Temp_labelstring(til->head->u.LABEL.label)); fflush(stderr);
#endif

    //now add the instruction to the block
    b = AS_splice(b, AS_InstrList(til->head, NULL));

    if (til->head->kind == I_OPER && ((til->head->u.OPER.jumps && til->head->u.OPER.jumps->labels)||(
                                                                                                          !strcmp(til->head->u.OPER.assem,"ret i64 -1")||!strcmp(til->head->u.OPER.assem,"ret double -1.0")))) {
#ifdef __DEBUG
      fprintf(stderr, "----Got a jump, ending the block for label = %s\n", Temp_labelstring(b->head->u.LABEL.label)); fflush(stderr);
#endif
      bl = AS_BlockSplice(bl, AS_BlockList(AS_Block(b), NULL)); //got a jump, stop a block
      b = NULL; //and start a new block
    }
    til = til->tail;
  }
#ifdef __DEBUG
  fprintf(stderr, "----Processed all instructions\n"); fflush(stderr);
#endif
  return bl;
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
  string file_insxml = checked_malloc(IR_MAXLEN);
  sprintf(file_insxml, "%s.6.ins", file);
  string file_cfg= checked_malloc(IR_MAXLEN);
  sprintf(file_cfg, "%s.7.cfg", file);
  string file_ssa= checked_malloc(IR_MAXLEN);
  sprintf(file_ssa, "%s.8.ssa", file);

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
    b = C_basicBlocks(sl);
    // llvm instruction selection
    AS_instrList prologil = llvmprolog(fdl->head->name, fdl->head->args, fdl->head->ret_type);
    AS_blockList bodybl = NULL;
    for (C_stmListList sll = b.stmLists; sll; sll = sll->tail) {
      AS_instrList bil = llvmbody(sll->head);

      AS_blockList bbl = AS_BlockList(AS_Block(bil), NULL);
      bodybl = AS_BlockSplice(bodybl, bbl);
    }
    AS_instrList epilogil = llvmepilog(b.label);

    // put all the blocks into one AS list
    AS_instrList il = AS_traceSchedule(bodybl, prologil, epilogil, FALSE);

    freopen(file_ins, "a", stdout);
    fprintf(stdout, "\n------For function ----- %s\n\n", fdl->head->name);
    fprintf(stdout, "------~Final traced AS instructions~------\n");
    AS_printInstrList(stdout, il, Temp_name());
    fflush(stdout);
    fclose(stdout);

    // convert AS_instrList to SSA
    AS_instr prologi = il->head;
    AS_instrList bodyil = il->tail;
    il->tail = NULL;
    AS_instrList til = bodyil;
    AS_instr epilogi;
    if (til->tail == NULL) {
      epilogi = til->head;
      bodyil = NULL;
    } else {
      while (til->tail->tail) {
        til = til->tail;
      }
      epilogi = til->tail->head;
      til->tail = NULL;
    }

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

    G_nodeList bg = Create_bg(instrList2BL(bodyil));
    freopen(file_cfg, "a", stdout);
    fprintf(stdout, "------Basic Block Graph------\n");
    Show_bg(stdout, bg);
    fprintf(stdout, "\n\n");
    fflush(stdout);
    fclose(stdout);

    AS_instrList bodyil_in_SSA = AS_instrList_to_SSA(bodyil, lg, bg);

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
