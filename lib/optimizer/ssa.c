#include "ssa.h"

#include "assem.h"
#include "graph.h"

#define __DEBUG
#undef __DEBUG

static S_table denv;         // dominator: block label -> G_nodeList
static S_table idomenv;      // immediate dominator: block label -> G_node
static S_table domftrenv;    // dominate frontier: block label -> G_nodeList
static S_table origenv;      // variable definition: block label -> Temp_tempList
static S_table liveinenv;    // variable live-in: block label -> Temp_tempList
static S_table phienv;       // phi function: block label -> Temp_tempList
static S_table phiinstrenv;  // place of phi function: block label -> AS_instrList
static S_table instrenv;     // block label -> AS_instrList
static S_table rmvenv;       // block label -> AS_instrList
static Temp_label curr_block;
static int tmp_num;
static G_nodeList defsites[1000];
static Temp_temp temptype[1000];
static int node_num;
static G_nodeList nodedom[200];  // dominates: #i dominates xxx
static stk varstk;
static AS_instrList bodyil_SSA;
static int tmpdef[1000];
static bool ssa[1000];
static int tmpversion[1000];

AS_instrList AS_instrList_to_SSA_LLVM(AS_instrList bodyil, G_nodeList lg, G_nodeList bg) {
  InitSSA(bg, bodyil);
  // Compute dominance
  computeDominator(bg);
  // Compute dominance frontiers
  computeDominanceFrontier(bg->head);
  variablessa(lg);
  // Compute defsites
  computeDefsites(bodyil, lg, bg);
  // Place phi functions
  placePhiFunction(bg);
  // Rename variables
  InitRename();
  renameVariable(bg->head);

  collectInstructions_llvm(bg);
  return bodyil_SSA;
}

AS_instrList AS_instrList_to_SSA_RPI(AS_instrList bodyil, G_nodeList lg, G_nodeList bg) {
  InitSSA(bg, bodyil);
  // Compute dominance
  computeDominator(bg);
  // Compute dominance frontiers
  computeDominanceFrontier(bg->head);
  variablessa(lg);
  // Compute defsites
  computeDefsites(bodyil, lg, bg);
  // Place phi functions
  placePhiFunction(bg);
  // Rename variables
  InitRename();
  renameVariable(bg->head);
  // eliminate phi functions
  eliminatePhiFunction(bg);
  collectInstructions_arm(bg);
  return bodyil_SSA;
}
void InitSSA(G_nodeList bg, AS_instrList bodyil) {
  node_num = bg->head->mygraph->nodecount;
  for (int i = 1; i < node_num; i++) {
    nodedom[i] = bg;
  }
  nodedom[0] = G_NodeList(bg->head, NULL);
  denv = S_empty();
  idomenv = S_empty();
  domftrenv = S_empty();
  origenv = S_empty();
  liveinenv = S_empty();
  phienv = S_empty();
  phiinstrenv = S_empty();
  instrenv = S_empty();
  rmvenv = S_empty();
  tmp_num = 0;
  for (int i = 99; i < 1000; i++) {
    defsites[i] = NULL;
    temptype[i] = NULL;
    ssa[i] = TRUE;
    tmpdef[i] = -1;
  }
  bodyil_SSA = NULL;
}
void computeDominator(G_nodeList bg) {
  while (bg) {
    G_node curr = bg->head;
    // Construct nodedom
    G_nodeList preds = curr->preds;
    G_nodeList doms = NULL;
    while (preds) {
      G_node pred = preds->head;
      doms = Intersect(doms, nodedom[pred->mykey]);
      preds = preds->tail;
    }
    doms = G_NodeList(curr, doms);
    nodedom[curr->mykey] = doms;
    // Construct denv
    int idomkey = -1;
    G_node idom = NULL;
    while (doms) {
      G_node dom = doms->head;
      AS_block b = dom->info;
      if (S_look(denv, b->label) != NULL) {
        G_nodeList ds = S_look(denv, b->label);
        ds = G_NodeList(curr, ds);
        S_enter(denv, b->label, ds);
      } else {
        G_nodeList ds = G_NodeList(curr, NULL);
        S_enter(denv, b->label, ds);
      }
      // Compute immediate dominator
      if (dom->mykey != curr->mykey && dom->mykey > idomkey) {
        idomkey = dom->mykey;
        idom = dom;
      }
      doms = doms->tail;
    }
    if (idom) {
      AS_block currb = curr->info;
      S_symbol currsym = currb->label;
      S_enter(idomenv, currsym, idom);
    }
    bg = bg->tail;
  }
}

void computeDominanceFrontier(G_node nd) {
  G_nodeList result = NULL;
  // Compute DF_local
  G_nodeList succs = nd->succs;
  while (succs) {
    G_node y = succs->head;
    AS_block yb = y->info;
    if (S_look(idomenv, yb->label)) {
      G_node yidom = S_look(idomenv, yb->label);
      if (yidom->mykey != nd->mykey) {
        result = G_NodeList(y, result);
      }
    }
    succs = succs->tail;
  }
  // Compute DF_up
  AS_block b = nd->info;
  G_nodeList chl = S_look(denv, b->label);
  while (chl) {
    G_node c = chl->head;
    if (c->mykey != nd->mykey) computeDominanceFrontier(c);
    AS_block cb = c->info;
    G_nodeList domftr = S_look(domftrenv, cb->label);
    while (domftr) {
      G_node w = domftr->head;
      if (!isDominate(nd, w) && !GnodeInGnodeList(w, result)) {
        result = G_NodeList(w, result);
      }
      domftr = domftr->tail;
    }
    chl = chl->tail;
  }

  S_enter(domftrenv, b->label, result);
}

void variablessa(G_nodeList lg) {
  while (lg) {
    G_node curr = lg->head;
    Temp_tempList defs = FG_def(curr);
    while (defs) {
      if (tmpdef[defs->head->num] != -1) {
        ssa[defs->head->num] = FALSE;
      } else {
        tmpdef[defs->head->num] = curr->mykey;
      }
      defs = defs->tail;
    }
    lg = lg->tail;
  }
}

void computeDefsites(AS_instrList bodyil, G_nodeList lg, G_nodeList bg) {
  // Construct origenv
  while (lg) {
    G_node curr = lg->head;
    AS_instr curr_instr = get_ithInstr(bodyil, curr->mykey);
    if (curr_instr->kind == I_LABEL) {
      curr_block = curr_instr->u.LABEL.label;
    }
    if (curr->mykey == 0) {
      S_enter(origenv, curr_block, FG_In(curr));
    }
    Temp_tempList defs = FG_def(curr);
    if (S_look(origenv, curr_block) == NULL) {
      S_enter(origenv, curr_block, defs);
    } else {
      Temp_tempList prev = S_look(origenv, curr_block);
      Temp_tempList tmp = Temp_TempListUnion(prev, defs);
      S_enter(origenv, curr_block, tmp);
    }
    Temp_tempList ins = FG_In(curr);
    if (S_look(liveinenv, curr_block) == NULL) {
      S_enter(liveinenv, curr_block, ins);
    } else {
      Temp_tempList prev = S_look(liveinenv, curr_block);
      Temp_tempList tmp = Temp_TempListUnion(prev, ins);
      S_enter(liveinenv, curr_block, tmp);
    }
    lg = lg->tail;
  }
  // Compute defsites of each variable
  while (bg) {
    G_node curr = bg->head;
    AS_block b = curr->info;
    if (S_look(origenv, b->label)) {
      Temp_tempList vars = S_look(origenv, b->label);
      while (vars) {
        Temp_temp v = vars->head;
        G_nodeList prev = defsites[v->num];
        defsites[v->num] = G_NodeList(curr, prev);
        temptype[v->num] = v;
        if (v->num > tmp_num) tmp_num = v->num;
        vars = vars->tail;
      }
    }
    bg = bg->tail;
  }
}

void placePhiFunction(G_nodeList bg) {
  // Record phi functions
  for (int i = 99; i <= tmp_num; i++) {
    G_nodeList gl = defsites[i];
    while (gl) {
      G_node rmv = gl->head;
      AS_block b = rmv->info;
      gl = gl->tail;
      G_nodeList df = S_look(domftrenv, b->label);
      while (df) {
        G_node y = df->head;
        AS_block yb = y->info;
        Temp_tempList phiy = S_look(phienv, yb->label);
        Temp_tempList iny = S_look(liveinenv, yb->label);
        Temp_temp ytmp = temptype[i];
        if (Temp_TempInTempList(ytmp, phiy) == FALSE &&
            Temp_TempInTempList(ytmp, iny) == TRUE && ssa[ytmp->num] == FALSE) {
          // insert a = phi(a,...a) at the top of block y
          AS_instr phiInstr = makePhiFunction(ytmp, y);
          if (S_look(phiinstrenv, yb->label) == NULL) {
            S_enter(phiinstrenv, yb->label, AS_InstrList(phiInstr, NULL));
          } else {
            AS_instrList tmp = S_look(phiinstrenv, yb->label);
            S_enter(phiinstrenv, yb->label, AS_InstrList(phiInstr, tmp));
          }
          phiy = Temp_TempList(ytmp, phiy);
          S_enter(phienv, yb->label, phiy);
          Temp_tempList origy = S_look(origenv, yb->label);
          if (Temp_TempInTempList(ytmp, origy) == FALSE) {
            gl = G_NodeList(y, gl);
          }
        }
        df = df->tail;
      }
    }
  }
  // Insert phi functions
  while (bg) {
    G_node h = bg->head;
    AS_block b = h->info;
    if (S_look(phiinstrenv, b->label)) {
      AS_instrList phis = S_look(phiinstrenv, b->label);
      AS_instrList tmp = AS_splice(phis, b->instrs->tail);
      tmp = AS_InstrList(b->instrs->head, tmp);
      S_enter(instrenv, b->label, tmp);
    } else {
      S_enter(instrenv, b->label, b->instrs);
    }
    bg = bg->tail;
  }
}

void InitRename() {
  varstk = stkEmpty();
  for (int i = 100; i <= tmp_num; i++) {
    stkPush(varstk, i, temptype[i]);
    tmpversion[i] = i;
  }
}
void renameVariable(G_node nd) {
  AS_block b = nd->info;
  // For each statement S in block n
  AS_instrList instrs = S_look(instrenv, b->label);
  AS_instrList origs = S_look(instrenv, b->label);
  AS_instrList h = instrs;
  while (instrs) {
    AS_instr instr = instrs->head;
    Temp_tempList use = NULL, def = NULL;
    if (!isPhiFunction(instr)) {
      if (instr->kind == I_OPER) {
        use = instr->u.OPER.src;
      } else if (instr->kind == I_MOVE) {
        use = instr->u.MOVE.src;
      }
      while (use) {
        use->head = srcVersion(use->head);
        use = use->tail;
      }
    }
    if (instr->kind == I_OPER) {
      def = instr->u.OPER.dst;
    } else if (instr->kind == I_MOVE) {
      def = instr->u.MOVE.dst;
    }
    while (def) {
      def->head = dstVersion(def->head);
      def = def->tail;
    }
    instrs = instrs->tail;
  }
  S_enter(instrenv, b->label, h);

  // For each successor Y of block n
  G_nodeList gl = nd->succs;
  while (gl) {
    G_node y = gl->head;
    int j = getPred(nd, y->preds);
    AS_block yb = y->info;
    AS_instrList yinstrs = S_look(instrenv, yb->label);
    AS_instrList hy = yinstrs;
    while (yinstrs) {
      AS_instr yinstr = yinstrs->head;
      // the jth operand of the phi-function is a
      if (isPhiFunction(yinstr)) {
        Temp_tempList phitmp = yinstr->u.OPER.src;
        if (phitmp == NULL) fprintf(stderr, "It's null!!\n");
        Temp_tempList phitmph = phitmp;
        for (int i = 0; i < j; ++i) phitmp = phitmp->tail;
        assert(phitmp);
        phitmp->head = srcVersion(phitmp->head);
        yinstrs->head = AS_Oper(yinstr->u.OPER.assem, yinstr->u.OPER.dst, phitmph,
                                yinstr->u.OPER.jumps);
      }
      yinstrs = yinstrs->tail;
    }
    S_enter(instrenv, yb->label, hy);
    gl = gl->tail;
  }

  // For each child X of n
  G_nodeList chl = S_look(denv, b->label);
  while (chl) {
    if (chl->head->mykey != nd->mykey) {
      renameVariable(chl->head);
    }
    chl = chl->tail;
  }

  // For each def of some variable a in the original S
  // pop stack[a]
  while (origs) {
    AS_instr orig = origs->head;
    Temp_tempList defs = NULL;
    if (orig->kind == I_OPER) {
      defs = orig->u.OPER.dst;
    } else if (orig->kind == I_MOVE) {
      defs = orig->u.MOVE.dst;
    }
    while (defs) {
      endVersion(defs->head);
      defs = defs->tail;
    }
    origs = origs->tail;
  }
}
void eliminatePhiFunction(G_nodeList bg) {
  string ir = (string)checked_malloc(IR_MAXLEN);
  while (bg) {
    G_node curr = bg->head;
    AS_block b = curr->info;
    if (S_look(phiinstrenv, b->label)) {
      AS_instrList instrs = S_look(phiinstrenv, b->label);
      while (instrs) {
        AS_instr instr = instrs->head;
        if (isPhiFunction(instr)) {
          Temp_temp dst = instr->u.OPER.dst->head;
          Temp_tempList vars = instr->u.OPER.src;
          Temp_labelList tgts = instr->u.OPER.jumps->labels;
          while (vars) {
            if (dst->type == T_int) {
              ir = (string)checked_malloc(IR_MAXLEN);
              sprintf(ir, "%%`d0 = add i64 %%`s0, 0");
            } else {  // T_float
              ir = (string)checked_malloc(IR_MAXLEN);
              sprintf(ir, "%%`d0 = fadd double %%`s0, 0.0");
            }
            AS_instr mov = AS_Oper(ir, Temp_TempList(dst, NULL),
                                   Temp_TempList(vars->head, NULL), NULL);
            Temp_label lb = tgts->head;
            if (S_look(rmvenv, lb)) {
              AS_instrList prev = S_look(rmvenv, lb);
              S_enter(rmvenv, lb, AS_InstrList(mov, prev));
            } else {
              S_enter(rmvenv, lb, AS_InstrList(mov, NULL));
            }
            tgts = tgts->tail;
            vars = vars->tail;
          }
        }
        instrs = instrs->tail;
      }
    }
    bg = bg->tail;
  }
}
void collectInstructions_arm(G_nodeList bg) {
  while (bg) {
    G_node h = bg->head;
    AS_block b = h->info;
    if (S_look(instrenv, b->label)) {
      AS_instrList instrs = S_look(instrenv, b->label);
      if (S_look(rmvenv, b->label)) {
        AS_instrList remil = S_look(rmvenv, b->label);
        AS_instrList il = instrs;
        while (TRUE) {
          if (instrs->tail && instrs->tail->tail == NULL)
            break;
          else
            instrs = instrs->tail;
        }
        instrs->tail = AS_splice(remil, instrs->tail);
        bodyil_SSA = AS_splice(bodyil_SSA, il);
      } else {
        bodyil_SSA = AS_splice(bodyil_SSA, instrs);
      }
    }
    bg = bg->tail;
  }
  AS_instrList bh = bodyil_SSA;
  AS_instrList bf = NULL;
  while (bodyil_SSA) {
    AS_instr curr = bodyil_SSA->head;
    if (isPhiFunction(curr)) {
      bf->tail = bodyil_SSA->tail;
      bodyil_SSA = bf;
    } else {
      bf = bodyil_SSA;
      bodyil_SSA = bodyil_SSA->tail;
    }
  }
  bodyil_SSA = bh;
}

void collectInstructions_llvm(G_nodeList bg) {
  while (bg) {
    G_node h = bg->head;
    AS_block b = h->info;
    if (S_look(instrenv, b->label)) {
      AS_instrList instrs = S_look(instrenv, b->label);
      bodyil_SSA = AS_splice(bodyil_SSA, instrs);
    }
    bg = bg->tail;
  }
}
/* Helper methods */
static G_nodeList Intersect(G_nodeList a, G_nodeList b) {
  if (!a) {
    return b;
  }
  if (!b) {
    return a;
  }
  bool found;
  G_nodeList scan = a;
  G_nodeList result = NULL;
  while (scan) {
    found = FALSE;
    for (G_nodeList l = b; l != NULL; l = l->tail) {
      if (scan->head == l->head) {
        found = TRUE;
        break;
      }
    }
    if (found) result = G_NodeList(scan->head, result);
    scan = scan->tail;
  }
  return result;
}

static bool isDominate(G_node a, G_node b) {
  AS_block blk = b->info;
  G_nodeList gl = S_look(denv, blk->label);
  while (gl) {
    G_node g = gl->head;
    if (g->mykey == a->mykey) return TRUE;
    gl = gl->tail;
  }
  return FALSE;
}

static AS_instr get_ithInstr(AS_instrList asl, int i) {
  if (i == 0) return asl->head;
  return get_ithInstr(asl->tail, i - 1);
}

static bool GnodeInGnodeList(G_node g, G_nodeList gl) {
  for (G_nodeList a = gl; a; a = a->tail) {
    if (g == a->head) return TRUE;
  }
  return FALSE;
}
static AS_instr makePhiFunction(Temp_temp var, G_node g) {
  int num = 0;
  string ir = (string)checked_malloc(IR_MAXLEN);
  if (var->type == T_int) {
    sprintf(ir, "%%`d0 = phi i64 ");
  } else {  // T_float
    sprintf(ir, "%%`d0 = phi double ");
  }
  G_nodeList preds = g->preds;
  Temp_tempList src = NULL;
  Temp_labelList tll = NULL, tll_last = NULL;
  while (preds) {
    if (preds->tail) {
      sprintf(ir + strlen(ir), "[%%`s%d, %%`j%d], ", num, num);
    } else {
      sprintf(ir + strlen(ir), "[%%`s%d, %%`j%d]", num, num);
    }
    src = Temp_TempList(var, src);
    AS_block b = preds->head->info;
    Temp_label tl = b->label;
    if (tll_last == NULL) {
      tll = tll_last = Temp_LabelList(tl, NULL);
    } else {
      tll_last->tail = Temp_LabelList(tl, NULL);
      tll_last = tll_last->tail;
    }
    num++;
    preds = preds->tail;
  }
  return AS_Oper(ir, Temp_TempList(var, NULL), src, AS_Targets(tll));
}

static bool isPhiFunction(AS_instr a) {
  string assem = NULL;
  if (a->kind == I_OPER) {
    assem = a->u.OPER.assem;
  } else {
    return FALSE;
  }
  for (int i = 0; i < strlen(assem); i++) {
    if (assem[i] == '[') return TRUE;
  }
  return FALSE;
}
static int getPred(G_node g, G_nodeList gl) {
  int i = 0;
  while (gl) {
    if (gl->head->mykey == g->mykey) return i;
    i++;
    gl = gl->tail;
  }
  return i;
}
static Temp_temp srcVersion(Temp_temp tmp) {
  if (tmp->num == 99) {
    return tmp;
  }
  int v = tmpversion[tmp->num];
  Temp_temp newtmp = stkTop(varstk, v);
  return newtmp;
}

static Temp_temp dstVersion(Temp_temp tmp) {
  if (ssa[tmp->num]) {
    stkPush(varstk, tmpversion[tmp->num], tmp);
    return tmp;
  }
  Temp_temp newtmp = Temp_newtemp(tmp->type);
  stkPush(varstk, tmp->num, newtmp);
  tmpversion[newtmp->num] = tmpversion[tmp->num];
  return newtmp;
}

static void endVersion(Temp_temp tmp) {
  int v = tmpversion[tmp->num];
  stkPop(varstk, v);
}
/* Stack */
static stk stkEmpty() {
  stk s = checked_malloc(sizeof(*s));
  for (int i = 0; i < STK_SIZE; i++) {
    s->top[i] = -1;
  }
  return s;
}
static void stkPush(stk s, int i, Temp_temp t) {
  if (!t) return;
  s->top[i]++;
  int top = s->top[i];
  s->data[i][top] = t;
}
static void stkPop(stk s, int i) {
  if (s->top[i] == -1) {
    fprintf(stderr, "Error in stkPop (%d)\n", i);
    exit(1);
  }
  s->top[i]--;
}
static Temp_temp stkTop(stk s, int i) {
  int top = s->top[i];
  if (top == -1) {
    fprintf(stderr, "Error in stkTop (%d)\n", i);
    exit(1);
  }
  return s->data[i][top];
}
/* TEST */
void testDominator(G_nodeList bg) {
  for (int i = 0; i < node_num; i++) {
    fprintf(stderr, "Node %d's dominator:\n", i);
    G_nodeList gl = nodedom[i];
    while (gl) {
      fprintf(stderr, "%d, ", gl->head->mykey);
      gl = gl->tail;
    }
    fprintf(stderr, "\n");
  }
  while (bg) {
    G_node curr = bg->head;
    AS_block b = curr->info;
    G_nodeList gl = S_look(denv, b->label);
    fprintf(stderr, "Node %s (id: %d) dominates:\n", S_name(b->label), curr->mykey);
    while (gl) {
      fprintf(stderr, "%d, ", gl->head->mykey);
      gl = gl->tail;
    }
    fprintf(stderr, "\n");
    if (S_look(idomenv, b->label)) {
      G_node idom = S_look(idomenv, b->label);
      fprintf(stderr, "Its idom is %d\n", idom->mykey);
    }
    bg = bg->tail;
  }
}

void testDF(G_nodeList bg) {
  while (bg) {
    G_node curr = bg->head;
    AS_block b = curr->info;
    fprintf(stderr, "Checking dominance frontier of node %d\n", curr->mykey);
    if (S_look(domftrenv, b->label)) {
      G_nodeList domftr = S_look(domftrenv, b->label);
      while (domftr) {
        fprintf(stderr, "%d, ", domftr->head->mykey);
        domftr = domftr->tail;
      }
      fprintf(stderr, "\n");
    }
    bg = bg->tail;
  }
}

void testOrig() {
  for (int i = 100; i < 1000; i++) {
    if (defsites[i]) {
      fprintf(stderr, "Current variable r%d defined in:\n", i);
      G_nodeList gl = defsites[i];
      while (gl) {
        fprintf(stderr, "%d, ", gl->head->mykey);
        gl = gl->tail;
      }
      fprintf(stderr, "\n");
    }
  }
}

void testPhi(G_nodeList bg) {
  while (bg) {
    G_node curr = bg->head;
    AS_block b = curr->info;
    if (S_look(phiinstrenv, b->label)) {
      fprintf(stderr, "Phi function at block %s:\n", S_name(b->label));
      AS_instrList instrs = S_look(phiinstrenv, b->label);
      while (instrs) {
        fprintf(stderr, "%s\n", instrs->head->u.OPER.assem);
        instrs = instrs->tail;
      }
    }
    bg = bg->tail;
  }
}

void testInsert(G_nodeList bg) {
  while (bg) {
    G_node h = bg->head;
    AS_block b = h->info;
    if (S_look(instrenv, b->label)) {
      AS_instrList instrs = S_look(instrenv, b->label);
      while (instrs) {
        fprintf(stderr, "%s\n", instrs->head->u.OPER.assem);
        instrs = instrs->tail;
      }
    }
    bg = bg->tail;
  }
}

void testssa() {
  for (int i = 99; i <= tmp_num; i++) {
    if (ssa[i] == TRUE) {
      fprintf(stderr, "%d: ssa\n", i);
    } else {
      fprintf(stderr, "%d\n", i);
    }
  }
}