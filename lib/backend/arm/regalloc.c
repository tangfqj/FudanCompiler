#include "regalloc.h"
#define REG_INT 7
#define REG_FLOAT 30
static Temp_node neighbors[1000]; // mykey
static bool deal[1000]; // mykey
static int spill_offset[1000]; // temp_num
static bool spill_int[2];
static bool spill_float[2];
static int spillcnt;
static stack stk;
static int temp_count;
static int colormap[1000]; // temp_num
static AS_instrList bodyil_alloc;
AS_instrList regalloc(AS_instrList il, G_nodeList ig_local) {
  if (!il)  return NULL;
  InitReg();
  Preprocess(ig_local);
  while (TRUE) {
    bool changed = TRUE;
    while (changed) changed = simplify();
    if (isFinished()) break;
    //fprintf(stderr, "Touched spill\n");
    spill();
  }
  color();
  modifyIL(il);
  return bodyil_alloc;
}

/* Helper functions */
void InitReg() {
  for (int i = 0; i < 1000; i++) {
    neighbors[i] = (Temp_node)checked_malloc(sizeof * neighbors[i]);
    neighbors[i]->mykey = i;
    neighbors[i]->degree = 0;
    neighbors[i]->mytemp = NULL;
    neighbors[i]->color = -1;
    neighbors[i]->adjs = 0;
    for (int j = 0; j < 200; j++) {
      neighbors[i]->adj[j] = -1;
    }
    deal[i] = FALSE;
    colormap[i] = -1;
  }
  stk = stackEmpty();
  bodyil_alloc = NULL;
  spillcnt = 0;
  for (int i = 0; i < 2; i++) {
    spill_int[i] = FALSE;
    spill_float[i] = FALSE;
  }
}
void Preprocess(G_nodeList ig) {
  temp_count = ig->head->mygraph->nodecount;
  while (ig) {
    G_node n = ig->head;
    Temp_temp ntmp = (Temp_temp) G_nodeInfo(n);
    neighbors[n->mykey]->mytemp = ntmp;
    G_nodeList succ = n->succs;
    while (succ) {
      G_node s = succ->head;
      Temp_temp stmp = (Temp_temp) G_nodeInfo(s);
      if (stmp->type == ntmp->type) {
        int ndeg = neighbors[n->mykey]->degree;
        neighbors[n->mykey]->adj[ndeg] = s->mykey;
        neighbors[n->mykey]->degree++;
        int sdeg = neighbors[s->mykey]->degree;
        neighbors[s->mykey]->adj[sdeg] = n->mykey;
        neighbors[s->mykey]->degree++;
      }
      succ = succ->tail;
    }
    ig = ig->tail;
  }
  for (int i = 0; i < temp_count; i++) {
    neighbors[i]->adjs = neighbors[i]->degree;
  }
  // precolor
  for (int i = 0; i < temp_count; i++) {
    if (neighbors[i]->mytemp->num < 99) {
      neighbors[i]->color = neighbors[i]->mytemp->num;
      colormap[neighbors[i]->mytemp->num] = neighbors[i]->mytemp->num;
      deal[i] = TRUE;
      for (int j = 0; j < neighbors[i]->adjs; j++) {
        int adj = neighbors[i]->adj[j];
        neighbors[adj]->degree--;
      }
    }
  }
  for (int i = 0; i < 99; i++) {
    colormap[i] = i;
  }
}

bool simplify() {
  bool flag = FALSE;
  for (int i = 0; i < temp_count; i++) {
    if (neighbors[i]->mytemp->type == T_int) {
      if (neighbors[i]->degree < REG_INT && deal[i] == FALSE) {
        //fprintf(stderr, "Simplify int temp %d\n", neighbors[i]->mytemp->num);
        deal[i] = TRUE;
        stackPush(stk, neighbors[i]);
        for (int j = 0; j < neighbors[i]->adjs; j++) {
          int adj = neighbors[i]->adj[j];
          neighbors[adj]->degree--;
        }
        flag = TRUE;
      }
    }
    else {
      if (neighbors[i]->degree < REG_FLOAT && deal[i] == FALSE) {
        //fprintf(stderr, "Simplify float temp %d\n", neighbors[i]->mytemp->num);
        deal[i] = TRUE;
        stackPush(stk, neighbors[i]);
        for (int j = 0; j < neighbors[i]->adjs; j++) {
          int adj = neighbors[i]->adj[j];
          neighbors[adj]->degree--;
        }
        flag = TRUE;
      }
    }
  }
  return flag;
}

void spill() {
  int maxdeg = 0;
  int s = -1;
  for (int i = 0; i < temp_count; i++) {
    if (deal[i] == FALSE) {
      int deg = neighbors[i]->degree;
      if (deg > maxdeg) {
        maxdeg = deg;
        s = i;
      }
    }
  }
  if (s < 0) {
    fprintf(stderr, "Error: spill failed\n");
    exit(1);
  }
  //fprintf(stderr, "Spill temp %d\n", neighbors[s]->mytemp->num);
  deal[s] = TRUE;
  spillcnt++;
  spill_offset[neighbors[s]->mytemp->num] = spillcnt * 4;
  for (int j = 0; j < neighbors[s]->adjs; j++) {
    int adj = neighbors[s]->adj[j];
    neighbors[adj]->degree--;
  }
}

bool isFinished() {
  // Check if all temps are simplified or spilled
  int j = 0;
  while (j < temp_count) {
    if (deal[j] == FALSE)  return FALSE;
    else j++;
  }
  if (j == temp_count)  return TRUE;
}

void color() {
  while (!isEmpty(stk)) {
    Temp_node tn = stackTop(stk);
    stackPop(stk);
    tn->color = findColor(tn);
    colormap[tn->mytemp->num] = tn->color;
  }
}
void modifyIL(AS_instrList il) {
  bodyil_alloc = il;
  // Add instructions for spill
  assert(il != NULL);
  AS_instrList prev = il;
  il = il->tail;
  string ir = (string) checked_malloc(IR_MAXLEN);
  while (il) {
    AS_instr curr = il->head;
    Temp_tempList src = NULL, dst = NULL;
    switch (curr->kind) {
      case I_OPER:
        src = curr->u.OPER.src;
        dst = curr->u.OPER.dst;
        break;
      case I_MOVE:
        src = curr->u.MOVE.src;
        dst = curr->u.MOVE.dst;
        break;
      case I_LABEL:
        break;
    }
    while (src) {
      Temp_temp t = src->head;
      if (colormap[t->num] == -1) {
        //fprintf(stderr, "Spilled temp %d from src\n", t->num);
        int rg = getReg(t->type, TRUE);
        int offset = spill_offset[t->num];
        ir = (string) checked_malloc(IR_MAXLEN);
        sprintf(ir, "ldr %%r%d, [fp, #%d]", rg, offset);
        AS_instr ldr = AS_Oper(ir, NULL, NULL, NULL);
        src->head = Temp_reg(rg, t->type);
        prev->tail = AS_InstrList(ldr, il);
        prev = prev->tail;
      } else {
        src->head = Temp_reg(colormap[t->num], t->type);
      }
      src = src->tail;
    }
    while (dst) {
      Temp_temp t = dst->head;
      if (colormap[t->num] == -1) {
        //fprintf(stderr, "Spilled temp %d from dst\n", t->num);
        int rg = getReg(t->type, FALSE);
        int offset = spill_offset[t->num];
        ir = (string) checked_malloc(IR_MAXLEN);
        sprintf(ir, "str %%r%d, [fp, #%d]", rg, offset);
        AS_instr str = AS_Oper(ir, NULL, NULL, NULL);
        dst->head = Temp_reg(rg, t->type);
        il->tail = AS_InstrList(str, il->tail);
      } else {
        dst->head = Temp_reg(colormap[t->num], t->type);
      }
      dst = dst->tail;
    }
    prev = prev->tail;
    il = il->tail;
  }
  // Eliminate redundant move ops
  AS_instrList bh = bodyil_alloc;
  AS_instrList bf = NULL;
  while (bodyil_alloc) {
    AS_instr curr = bodyil_alloc->head;
    if (isRedundant(curr)) {
      bf->tail = bodyil_alloc->tail;
      bodyil_alloc = bf;
    }
    else {
      bf = bodyil_alloc;
      bodyil_alloc = bodyil_alloc->tail;
    }
  }
  bodyil_alloc = bh;
}
int findColor(Temp_node tn) {
  if (tn->mytemp->num < 99) {
    return tn->mytemp->num;
  }
  if (tn->adjs == 0) {
    return 0;
  }
  if (tn->mytemp->type == T_int) {
    int color[REG_INT + 1] = {0};
    for (int i = 0; i < tn->adjs; i++) {
      int adj = tn->adj[i];
      if (neighbors[adj]->color != -1) {
        color[neighbors[adj]->color] = 1;
      }
    }
    for (int i = 1; i < REG_INT + 1; i++) {
      if (color[i] == 0) {
        return i;
      }
    }
  }
  else {
    int color[REG_FLOAT + 1] = {0};
    for (int i = 0; i < tn->adjs; i++) {
      int adj = tn->adj[i];
      if (neighbors[adj]->color != -1) {
        color[neighbors[adj]->color] = 1;
      }
    }
    for (int i = 1; i < REG_FLOAT; i++) {
      if (color[i] == 0) {
        return i;
      }
    }
  }
  // Should not reach this point
  return -1;
}

bool isRedundant(AS_instr inst) {
  if (inst->kind != I_MOVE) {
    return FALSE;
  }
  Temp_temp dst = inst->u.MOVE.dst->head;
  Temp_temp src = inst->u.MOVE.src->head;
  if (src->type == dst->type && src->num == dst->num) {
    return TRUE;
  }
  return FALSE;
}

int getReg(T_type type, bool is_src) {
  if (type == T_int) {
    if (!is_src) {
      spill_int[0] = FALSE;
      spill_int[1] = FALSE;
      return 8;
    }
    if (spill_int[0] == TRUE){
      spill_int[1] = TRUE;
      return 9;
    }
    spill_int[0] = TRUE;
    return 8;
  }
  else {
    if (!is_src) {
      spill_float[0] = FALSE;
      spill_float[1] = FALSE;
      return 30;
    }
    if (!spill_float[0]){
      spill_float[1] = TRUE;
      return 31;
    }
    spill_float[0] = TRUE;
    return 30;
  }
}

/* Stack */
stack stackEmpty() {
  stack s = (stack)checked_malloc(sizeof *stk);
  s->top = -1;
  return s;
}
void stackPush(stack s, Temp_node tn) {
  s->top++;
  s->data[s->top] = tn;
}
void stackPop(stack s) {
  if (s->top < 0) {
    fprintf(stderr, "Error: stack underflow\n");
    exit(1);
  }
  s->top--;
}
Temp_node stackTop(stack s) {
  if (s->top < 0) {
    fprintf(stderr, "Error: stack underflow\n");
    exit(1);
  }
  return s->data[s->top];
}
bool isEmpty(stack s) {
  if (s->top == -1)   return TRUE;
  return FALSE;
}
/* TEST */
static void showDegree(G_nodeList ig) {
  while (ig) {
    G_node n = ig->head;
    fprintf(stderr, "Node %d (%d) has degree %d\n", neighbors[n->mykey]->mytemp->num, n->mykey, neighbors[n->mykey]->adjs);
    fprintf(stderr, "Adjacency list: ");
    for (int i = 0; i < neighbors[n->mykey]->adjs; i++) {
      fprintf(stderr, "%d ", neighbors[n->mykey]->adj[i]);
    }
    fprintf(stderr, "\n");
    ig = ig->tail;
  }
}

static void testColor() {
  for (int i = 0; i < temp_count; i++) {
    fprintf(stderr, "Temp %d has color %d\n", neighbors[i]->mytemp->num, neighbors[i]->color);
    //fprintf(stderr, "Double check colormap: %d\n", colormap[neighbors[i]->mytemp->num]);
  }
}

static void check() {
  for (int i = 0; i < temp_count; i++) {
    fprintf(stderr, "Temp %d has degree %d\n", neighbors[i]->mytemp->num, neighbors[i]->degree);
  }
}
