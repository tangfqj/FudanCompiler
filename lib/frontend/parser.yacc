/* 1. declarations */

/* included code */
%{

#include <stdio.h>
#include <stdlib.h>
#include "fdmjast.h"

extern int line;
extern int yylex();
extern void yyerror(char*);
extern int  yywrap();

extern A_prog root;

%}

/* yylval */
%union {
    A_pos pos;
    A_prog prog;
    A_mainMethod mainMethod;
    A_classDecl classDecl;
    A_classDeclList classDeclList;
    A_methodDecl methodDecl;
    A_methodDeclList methodDeclList;
    A_formal formal;
    A_formalList formalList;
    A_varDecl varDecl;
    A_varDeclList varDeclList;
    A_stmList stmList;
    A_stm stm;
    A_exp exp;
    A_expList expList;
    A_type type;
}
/* termianl symbols */
%token<pos> PUBLIC INT MAIN CLASS FLOAT
%token<pos> PUTNUM PUTCH PUTARRAY GETNUM GETCH GETARRAY STARTTIME STOPTIME
%token<pos> IF ELSE CONTINUE BREAK RETURN WHILE THIS NEW EXTENDS
%token<pos> True False LENGTH
%token<pos> ADD MINUS TIMES DIV UMINUS AND OR LT LE GT GE EQ NEQ
%token<pos> '(' ')' '[' ']' '{' '}' '=' ',' ';' '.' '!'
%token<exp> NUM ID

/* non-termianl symbols */
%type<prog> PROG
%type<mainMethod> MAINMETHOD
%type<stmList> STMLIST
%type<stm> STM
%type<expList> EXPLIST
%type<expList> EXPREST
%type<expList> CONSTLIST
%type<expList> CONSTREST
%type<exp> EXP
%type<exp> NUMCONST
%type<classDeclList> CLASSDECLLIST
%type<classDecl> CLASSDECL
%type<varDeclList> VARDECLLIST
%type<varDecl> VARDECL
%type<methodDeclList> METHODDECLLIST
%type<methodDecl> METHODDECL
%type<formalList> FORMALLIST
%type<formalList> FORMALREST
%type<formal> FORMAL
%type<type> TYPE

/* start symbol */
%start PROG

/* precedence */
%left OR
%left AND
%left EQ NEQ
%left LT LE GT GE
%left ADD MINUS
%left TIMES DIV
%left UMINUS
%right '!'
%left IF
%left ELSE

%% /* 2. rules */

PROG: MAINMETHOD CLASSDECLLIST {
  root = A_Prog(A_Pos($1->pos->line, $1->pos->pos), $1, $2);
  $$ = root;
} ;

MAINMETHOD: PUBLIC INT MAIN '(' ')' '{' VARDECLLIST STMLIST '}' {
  $$ = A_MainMethod($1, $7, $8);
} | INT MAIN '(' ')' '{' {
  yyerror("Missing keyword \'public\'");
} ;

VARDECLLIST: /* empty */ {
  $$ = NULL;
} | VARDECL VARDECLLIST {
  $$ = A_VarDeclList($1, $2);
} ;

VARDECL: TYPE ID ';' {
  $$ = A_VarDecl(A_Pos($1->pos->line, $1->pos->pos), $1, $2->u.v, NULL);
} | TYPE ID '=' NUMCONST ';' {
  $$ = A_VarDecl(A_Pos($1->pos->line, $1->pos->pos), $1, $2->u.v, A_ExpList($4, NULL));
} |TYPE ID '=' '{' CONSTLIST '}' ';' {
  $$ = A_VarDecl(A_Pos($1->pos->line, $1->pos->pos), $1, $2->u.v, $5);
} ;

NUMCONST: NUM {
  $$ = $1;
} | MINUS NUM %prec UMINUS {
  $$ = A_NumConst(A_Pos($2->pos->line, $2->pos->pos - 1), -($2->u.num));
} ;

CONSTLIST: /* empty */ {
  $$ = NULL;
} | NUMCONST CONSTREST {
  $$ = A_ExpList($1, $2);
} ;

CONSTREST: /* empty */ {
  $$ = NULL;
} | ',' NUMCONST CONSTREST {
  $$ = A_ExpList($2, $3);
} ;

STMLIST: /* empty */ {
  $$ = NULL;
} | STM STMLIST {
  $$ = A_StmList($1, $2);
} ;

STM: '{' STMLIST '}' {
  $$ = A_NestedStm($1, $2);
} | IF '(' EXP ')' STM  ELSE STM {
  $$ = A_IfStm($1, $3, $5, $7);
} | IF '(' EXP ')' STM %prec IF{
  $$ = A_IfStm($1, $3, $5, NULL);
} | WHILE '(' EXP ')' STM {
  $$ = A_WhileStm($1, $3, $5);
} | WHILE '(' EXP ')' ';' {
  $$ = A_WhileStm($1, $3, NULL);
} | EXP '=' EXP ';' {
  $$ = A_AssignStm(A_Pos($1->pos->line, $1->pos->pos), $1, $3);
} | EXP '=' EXP ',' ';' {
  yyerror("Extra comma in assignment statement");
} | EXP '[' ']' '=' '{' EXPLIST '}' ';' {
  $$ = A_ArrayInit(A_Pos($1->pos->line, $1->pos->pos), $1, $6);
} | EXP '.' ID '(' EXPLIST ')' ';' {
  $$ = A_CallStm(A_Pos($1->pos->line, $1->pos->pos), $1, $3->u.v, $5);
} | CONTINUE ';' {
  $$ = A_Continue($1);
} | BREAK ';' {
  $$ = A_Break($1);
} | RETURN EXP ';' {
  $$ = A_Return($1, $2);
} | PUTNUM '(' EXP ')' ';' {
  $$ = A_Putnum($1, $3);
} | PUTCH '(' EXP ')' ';' {
  $$ = A_Putch($1, $3);
} | PUTARRAY '(' EXP ',' EXP ')' ';' {
  $$ = A_Putarray($1, $3, $5);
} | STARTTIME '(' ')' ';' {
  $$ = A_Starttime($1);
} | STOPTIME '(' ')' ';' {
  $$ = A_Stoptime($1);
} ;

EXPLIST: /* empty */ {
  $$ = NULL;
} | EXP EXPREST {
  $$ = A_ExpList($1, $2);
} ;

EXPREST: /* empty */ {
  $$ = NULL;
} | ',' EXP EXPREST {
  $$ = A_ExpList($2, $3);
} ;

EXP: NUM {
  $$ = $1;
} | True {
  $$ = A_BoolConst($1, TRUE);
} | False {
  $$ = A_BoolConst($1, FALSE);
} | LENGTH '(' EXP ')' {
  $$ = A_LengthExp($1, $3);
} | GETNUM '(' ')' {
  $$ = A_Getnum($1);
} | GETCH '(' ')' {
  $$ = A_Getch($1);
} | GETARRAY '(' EXP ')' {
  $$ = A_Getarray($1, $3);
} | ID {
  $$ = $1;
} | THIS {
  $$ = A_ThisExp($1);
} | NEW INT '[' EXP ']' {
  $$ = A_NewIntArrExp($1, $4);
} | NEW FLOAT '[' EXP ']' {
  $$ = A_NewFloatArrExp($1, $4);
} | NEW ID '(' ')' {
  $$ = A_NewObjExp($1, $2->u.v);
} | '!' EXP {
  $$ = A_NotExp($1, $2);
} | MINUS EXP %prec UMINUS {
  $$ = A_MinusExp($1, $2);
} | '(' EXP ')' {
  $2->pos->pos -= 1;
  $$ = $2;
} | '(' '{' STMLIST '}' EXP ')' {
  $$ = A_EscExp($1, $3, $5);
} | EXP ADD EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_plus, $3);
} | EXP MINUS EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_minus, $3);
} | EXP TIMES EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_times, $3);
} | EXP DIV EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_div, $3);
} | EXP AND EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_and, $3);
} | EXP OR EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_or, $3);
} | EXP LT EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_less, $3);
} | EXP LE EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_le, $3);
} | EXP GT EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_greater, $3);
} | EXP GE EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_ge, $3);
} | EXP EQ EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_eq, $3);
} | EXP NEQ EXP {
  $$ = A_OpExp(A_Pos($1->pos->line, $1->pos->pos), $1, A_ne, $3);
} | EXP '.' ID {
  $$ = A_ClassVarExp(A_Pos($1->pos->line, $1->pos->pos), $1, $3->u.v);
} | EXP '.' ID '(' EXPLIST ')' {
  $$ = A_CallExp(A_Pos($1->pos->line, $1->pos->pos), $1, $3->u.v, $5);
} | EXP ',' ID {
  yyerror("Unexpected comma in expression");
} | EXP '[' EXP ']' {
  $$ = A_ArrayExp(A_Pos($1->pos->line, $1->pos->pos), $1, $3);
} ;

CLASSDECLLIST: /* empty */ {
  $$ = NULL;
} | CLASSDECL CLASSDECLLIST {
  $$ = A_ClassDeclList($1, $2);
} ;

CLASSDECL: PUBLIC CLASS ID '{' VARDECLLIST METHODDECLLIST '}' {
  $$ = A_ClassDecl($1, $3->u.v, NULL, $5, $6);
} | PUBLIC CLASS ID EXTENDS ID '{' VARDECLLIST METHODDECLLIST '}' {
  $$ = A_ClassDecl($1, $3->u.v, $5->u.v, $7, $8);
} ;

METHODDECLLIST: /* empty */ {
  $$ = NULL;
} | METHODDECL METHODDECLLIST {
  $$ = A_MethodDeclList($1, $2);
} ;

METHODDECL: PUBLIC TYPE ID '(' FORMALLIST ')' '{' VARDECLLIST STMLIST '}' {
  $$ = A_MethodDecl($1, $2, $3->u.v, $5, $8, $9);
} ;

// Type -> class id | int | int '[' ']' | float | float '[' ']'
TYPE: CLASS ID {
  $$ = A_Type($1, A_idType, $2->u.v);
} | INT {
  $$ = A_Type($1, A_intType, NULL);
} | INT '[' ']' {
  $$ = A_Type($1, A_intArrType, NULL);
} | FLOAT {
  $$ = A_Type($1, A_floatType, NULL);
} | FLOAT '[' ']' {
  $$ = A_Type($1, A_floatArrType, NULL);
} ;

FORMALLIST: /* empty */ {
  $$ = NULL;
} | FORMAL FORMALREST {
  $$ = A_FormalList($1, $2);
} ;

FORMALREST: /* empty */ {
  $$ = NULL;
} | ',' FORMAL FORMALREST {
  $$ = A_FormalList($2, $3);
} ;

FORMAL: TYPE ID {
  $$ = A_Formal(A_Pos($1->pos->line, $1->pos->pos), $1, $2->u.v);
} ;
%% /* 3. programs */

void yyerror(char *s) {
  fprintf(stderr, "Error: %s at line %d\n", s, line);
  exit(1);
}

int yywrap() {
  return(1);
}