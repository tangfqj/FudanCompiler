#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdio.h>
#include "assem.h"
#include "flowgraph.h"
#include "ig.h"
#include "liveness.h"
#include "util.h"

AS_instrList regalloc(AS_instrList il, G_nodeList ig);

/* Struct */
typedef struct Temp_node_ *Temp_node;
struct Temp_node_ {
  int mykey;
  int degree;
  int color;
  Temp_temp mytemp;
  int adjs;
  int adj[200];
};
/* Stack */
typedef struct stack_ *stack;
struct stack_ {
  int top;
  Temp_node data[500];
};
stack stkEmpty();
void stkPush(stack s, Temp_node tn);
void stkPop(stack s);
Temp_node stkTop(stack s);
bool isEmpty(stack s);
/* Helper functions */
void InitReg();
void Preprocess(G_nodeList ig);
bool simplify();
void spill();
bool isFinished();
void color();
int findColor(Temp_node tn);
void modifyIL(AS_instrList il);
bool isRedundant(AS_instr inst);
int getReg(T_type type, bool is_src);
bool isPop(string assem);
/* TEST */
static void showDegree(G_nodeList ig);
static void testColor();
static void check();
#endif