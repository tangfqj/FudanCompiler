#ifndef SSA_H
#define SSA_H

#include "assem.h"
#include "assemblock.h"
#include "bg.h"
#include "graph.h"
#include "liveness.h"
#include "symbol.h"
#include "util.h"

AS_instrList AS_instrList_to_SSA(AS_instrList bodyil, G_nodeList fg, G_nodeList bg);
void InitSSA(G_nodeList bg);
void computeDominator(G_nodeList bg);
void computeDominanceFrontier(G_node nd);
void variablessa(G_nodeList lg);
void computeDefsites(AS_instrList bodyil, G_nodeList lg, G_nodeList bg);
void placePhiFunction(G_nodeList bg);
void InitRename();
void renameVariable(G_node nd);
void eliminatePhiFunction(G_nodeList bg);
void collectInstructions(G_nodeList bg);
/* Helper methods */
static G_nodeList Intersect(G_nodeList a, G_nodeList b);
static bool isDominate(G_node a, G_node b); // test whether a dominates b
static AS_instr get_ithInstr(AS_instrList asl, int i);
static bool GnodeInGnodeList(G_node g, G_nodeList gl);
static AS_instr makePhiFunction(Temp_temp var, G_node g);
static bool isPhiFunction(AS_instr a);
static int getPred(G_node g, G_nodeList gl);
static Temp_temp srcVersion(Temp_temp tmp);
static Temp_temp dstVersion(Temp_temp tmp);
static void endVersion(Temp_temp tmp);
/* Stack */
#define STACK_SIZE 1000
typedef struct stack_ *stack;
struct stack_{
  int top[STACK_SIZE];
  Temp_temp data[STACK_SIZE][STACK_SIZE];
};

static stack stkEmpty();
static void stkPush(stack s, int i, Temp_temp t);
static void stkPop(stack s, int i);
static Temp_temp stkTop(stack s, int i);
/* TEST */
void testDominator(G_nodeList bg);
void testDF(G_nodeList bg);
void testOrig();
void testPhi(G_nodeList bg);
void testInsert(G_nodeList bg);
void testssa();
#endif