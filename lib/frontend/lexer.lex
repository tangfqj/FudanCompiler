/* 1. declarations */

/* included code */
%{

#include <stdlib.h>
#include "fdmjast.h"
#include "parser.h"

int c;
int line = 1;
int pos = 1;

%}

%s COMMENT1 COMMENT2

punctuation [()\[\]{}=,;.!]
num   [1-9][0-9]*|0|[1-9][0-9]*\.[0-9]*|0\.[0-9]*|[1-9][0-9]*\.|0\.|\.[0-9]*
id    [a-z_A-Z][a-z_A-Z0-9]*
/* start conditions */

/* regexp nicknames */

%% /* 2. rules */
<INITIAL>"//" {
  pos += 2;
  BEGIN COMMENT1;
}
<INITIAL>"/*" {
  pos += 2;
  BEGIN COMMENT2;
}
<INITIAL>" "|\t {
  pos += 1;
}
<INITIAL>\n {
  line += 1;
  pos = 1;
}
<INITIAL>\r {}
<INITIAL>"public" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return PUBLIC;
}
<INITIAL>"int" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return INT;
}
<INITIAL>"main" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return MAIN;
}
<INITIAL>"class" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return CLASS;
}
<INITIAL>"float" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return FLOAT;
}
<INITIAL>"putnum" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return PUTNUM;
}
<INITIAL>"putch" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return PUTCH;
}
<INITIAL>"putarray" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return PUTARRAY;
}
<INITIAL>"starttime" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return STARTTIME;
}
<INITIAL>"stoptime" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return STOPTIME;
}
<INITIAL>"getnum" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return GETNUM;
}
<INITIAL>"getch" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return GETCH;
}
<INITIAL>"getarray" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return GETARRAY;
}
<INITIAL>"if" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return IF;
}
<INITIAL>"else" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return ELSE;
}
<INITIAL>"continue" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return CONTINUE;
}
<INITIAL>"break" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return BREAK;
}
<INITIAL>"return" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return RETURN;
}
<INITIAL>"while" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return WHILE;
}
<INITIAL>"new" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return NEW;
}
<INITIAL>"this" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return THIS;
}
<INITIAL>"extends" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return EXTENDS;
}
<INITIAL>"length" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return LENGTH;
}
<INITIAL>"true" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return True;
}
<INITIAL>"false" {
  yylval.pos = A_Pos(line, pos);
  pos += yyleng;
  return False;
}
<INITIAL>"+" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return ADD;
}
<INITIAL>"-" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return MINUS;
}
<INITIAL>"*" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return TIMES;
}
<INITIAL>"/" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return DIV;
}
<INITIAL>"||" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return OR;
}
<INITIAL>"&&" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return AND;
}
<INITIAL>"<" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return LT;
}
<INITIAL>"<=" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return LE;
}
<INITIAL>">" {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  return GT;
}
<INITIAL>">=" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return GE;
}
<INITIAL>"==" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return EQ;
}
<INITIAL>"!=" {
  yylval.pos = A_Pos(line, pos);
  pos += 2;
  return NEQ;
}
<INITIAL>{punctuation} {
  yylval.pos = A_Pos(line, pos);
  pos += 1;
  c = yytext[0];
  return c;
}
<INITIAL>{num} {
  yylval.exp = A_NumConst(A_Pos(line, pos), atof(yytext));
  pos += yyleng;
  return NUM;
}
<INITIAL>{id} {
  yylval.exp = A_IdExp(A_Pos(line, pos), String(yytext));
  pos += yyleng;
  return ID;
}
<INITIAL>. {
  printf("Illegal input \"%c\"\n", yytext[0]);
}

<COMMENT1>"\n" {
  line += 1;
  pos = 1;
  BEGIN INITIAL;
}
<COMMENT1>. {
  pos += 1;
}

<COMMENT2>"*/" {
  pos += 2;
  BEGIN INITIAL;
}
<COMMENT2>"\n" {
  line += 1;
  pos = 1;
}
<COMMENT2>. {
  pos += 1;
}
%% /* 3. programs */