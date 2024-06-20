This is the mini programming language we are going to compile in this project.
```html
Program -> MainMethod ClassDecl*
MainMethod -> public int main '(' ')' '{' VarDecl* Statement* '}'

ClassDecl -> class id [extends id] '{' VarDecl* MethodDecl* '}'

Type -> class id | int | int '[' ']' | float | float '[' ']'
VarDecl -> Type id ';' ｜ Type id '=' CONST ';' |
Type id '=' '{' ConstList '}' ';'

ConstList -> CONST ConstRest* | \empty
ConstRest -> ',' CONST

MethodDecl -> public Type id '(' FormalList ')' '{' VarDecl* Statement* '}'

FormalList -> Type id FormalRest* | \empty
FormalRest -> ',' Type id

Statement ->
'{' Statement* '}' |
if '(' Exp ')' Statement else Statement |
if '(' Exp ')' Statement |
while '(' Exp ')' Statement |
while '(' Exp ')' ';' |
id = Exp ';' |
id '[' Exp ']' = Exp ';' |
id '['  ']' = '{' ExpList '}' ';' |
Exp '.' id '(' ExpList ')' ';' |
continue ';' | break ';' |
return Exp ';' |
putint '(' Exp ')' ';' | putch '(' Exp ')' ';' |
putint '(' Exp ')' ';' | putch '(' Exp ')' ';' |
putarray '(' Exp ',' Exp ')' ';' |
starttime '(' ')' ';' | stoptime '(' ')' ';'

Exp -> Exp op Exp |
Exp '[' Exp ']' |
Exp '.' id '(' ExpList ')' |
Exp '.' id |
CONST |
true | false |
id | this | new int '[' Exp ']' | new float '[' Exp ']' |
new id '(' ')' | '!' Exp | '-' Exp | '(' Exp ')' |
'(' '{' Statement* '}' Exp ')' |   //escape expression
getint '(' ')' | getch '(' ')' | getarray '(' Exp ')'

ExpList -> Exp ExpRest* | \empty
ExpRest -> ',' Exp
```


The semantic of an FDMJ program with the above grammar is similar to that for programming language of C and Java. Here we give a few notes about it:

- The root of the grammar is `Program`.
- The binary operations (`op`) are `+, -, *, /, ||, &&, <, <=, >, >=, ==, !=`.
- `CONST` is either an integer `(+|-)[0-9]+` or a float number `(+|-)[0-9]*.[0-9]+`.
- `id` is any string consisting of `[a-z], [A-Z], [0-9]` and `_` (the underscore) of any length.
- Boolean binary operations (`||` and `&&`) follow the "shortcut" semantics. For example, in `(true || ({a=1;} false))`, `a=1` is not executed.
- All variables in a class are taken as “public” variables (as defined in Java).
- A subclass inherits all the methods and variables from its superclass (if any, recursively). To simplify, we assume that a subclass may override methods declared in its ancestor classes, but must have the same signature (i.e., the same list of types, albeit may use different id’s). And a variable declared in a subclass cannot use the same id as any variable declared in any of its ancestor classes.
