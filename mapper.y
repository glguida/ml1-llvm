%{
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "lowl.h"
#include "emitter.h"

/* Checks for optional characters. */
#define _CHECK_CHAR(_str)				\
	if ( strlen((_str)) != 1 ) {			\
		yyerror("expected literal character");	\
		exit(-1);				\
	}

#define _CHECK_CH1(_str, _c1)				\
	if ( strlen((_str)) != 1 ) {			\
		yyerror("Expected character");		\
		exit(-1);				\
	}						\
	if ( *(_str) != (_c1) )			 {	\
		yyerror("Wrong character in statement");\
		exit(-1);				\
	}

#define _CHECK_CH2(_str, _c1, _c2)			\
	if ( strlen((_str)) != 1 ) {			\
		yyerror("Expected character");		\
		exit(-1);				\
	}						\
	if ( *(_str) != (_c1) && *(_str) != (_c2) ) {	\
		yyerror("Wrong character in statement");\
		exit(-1);				\
	}
		
#define _CHECK_CH3(_str, _c1, _c2, _c3)			\
	if ( strlen((_str)) != 1 ) {			\
		yyerror("Expected character");		\
		exit(-1);				\
	}						\
	if ( *(_str) != (_c1) 				\
	     && *(_str) != (_c2)			\
	     && *(_str) != (_c3) ) {			\
		yyerror("Wrong character in statement");\
		exit(-1);				\
	}
#define _CHECK_CHSTR(_str, _str2, _c)			\
	if ( strlen((_str)) != 1 			\
	     && strcmp((_str), (_str2)) ) {		\
		yyerror("Wrong symbol in statement");	\
		exit(-1);				\
	}						\
	if ( *(_str) != _c ) {				\
		yyerror("Wrong character in statement");\
		exit(-1);				\
	}

/*
 * Special macro for code statements.
 * This helps keeping track of the pseudo-PC.
 * CEMIT is used for continuation instruction, while
 * SEMIT is used for stopping instruction.
 */
#define CEMIT(_f) { emit_##_f; emit_newpc(0); }
#define SEMIT(_f) { emit_##_f; emit_newpc(1); }

extern int yylineno;
void yyerror(char *s);
int yylex();

void add_idsym(char *sym, uintptr_t val);

%}

%union {
char		chr;
char 		*str;
uintptr_t 	num;
}

%token <str> LABEL
%token <str> SYMBOL
%token <str> STRING
%token <num> NUMBER

%token DCL EQU IDENT
%token CON NCH STR
%token LAV LBV LAL LCN LAM LCM LAI LCI LAA STV STI
%token CLEAR AAV ABV AAL SAV SBV SAL SBL MULTL BUMP ANDV ANDL
%token CAV CAL CCL CCN CAI CCI 
%token SUBR EXIT GOSUB GOADD CSS
%token GO GOEQ GONE GOGE GOGR GOLE GOLT GOPC GOND
%token FSTK BSTK CFSTK UNSTK FMOVE BMOVE 
%token MESS NB
%token PRGST PRGEN
%token ALIGN

%token LCH LNM LICH LHV
%token OF 
%token ',' '+' '-' '*' '(' ')' 
%token NLREP SPREP TABREP QUTREP
%token EOL 

/* ML/I extensions tokens. */
%token HASH THASH RL WTHS STOPCD SLREP LINKR LINKB ORL
%type <chr> ml1_charname

%type <str> v
%type <num> nof distance signnum of_macro expr parnm
%type <chr> rx dc px ax ex ctx charname

%left '+' '-'
%left '*'
%%

program:
	program command
	|
	;

command:
	table_label table_statement EOL
	| label code_statement EOL
	| var_statement EOL
	| EOL			{ emit_eol(); }
	;

table_label:
	LABEL			{ emit_table_label($1); }
	|
	;
label:
	LABEL			{ emit_label($1); }
	|
	;

var_statement:
	DCL v			{ emit_dcl($2); }
	| EQU v ',' v		{ emit_equ($2, $4); }
	| IDENT v ',' NUMBER	{
					add_idsym($2, $4);
					emit_ident($2, $4);
				}
	;

table_statement:
	CON nof		{ emit_con($2); }
	| NCH charname		{ emit_nch($2); }
	| STR STRING		{ emit_str($2); }
	| ml1_table_statement
	;

code_statement:
	LAV v ',' rx		{ CEMIT(lav($2,$4)); }
	| LBV v			{ CEMIT(lbv($2)); }
	| LAL nof		{ CEMIT(lal($2)); }
	| LCN charname		{ CEMIT(lcn($2)); }
	| LAM nof		{ CEMIT(lam($2)); }
	| LCM nof		{ CEMIT(lcm($2)); }
	| LAI v ',' rx		{ CEMIT(lai($2,$4)); }
	| LCI v ',' rx		{ CEMIT(lci($2,$4)); }
	| LAA v ',' dc		{ CEMIT(laa($2,$4)); }
	| STV v ',' px		{ CEMIT(stv($2,$4)); }
	| STI v ',' px		{ CEMIT(sti($2,$4)); }
	| CLEAR v		{ CEMIT(clear($2)); }
	| AAV v			{ CEMIT(aav($2)); }
	| ABV v			{ CEMIT(abv($2)); }
	| AAL nof		{ CEMIT(aal($2)); }
	| SAV v			{ CEMIT(sav($2)); }
	| SBV v			{ CEMIT(sbv($2));	}
	| SAL nof		{ CEMIT(sal($2)); }
	| SBL nof		{ CEMIT(sbl($2)); }
	| MULTL nof		{ CEMIT(multl($2)); }
	| BUMP v ',' nof	{ CEMIT(bump($2, $4)); }
	| ANDV v 		{ CEMIT(andv($2)); }
	| ANDL NUMBER		{ CEMIT(andl($2)); }
	| CAV v ',' ax		{ CEMIT(cav($2)); }
	| CAL nof		{ CEMIT(cal($2)); }
	| CCL STRING		{ _CHECK_CHAR($2); CEMIT(ccl($2)); }
	| CCN charname		{ CEMIT(ccn($2)); }
	| CAI v ',' ax		{ CEMIT(cai($2,$4)); }
	| CCI v 		{ CEMIT(cci($2)); }
	| SUBR SYMBOL ',' parnm ',' NUMBER 	{ CEMIT(subr($2, $4, $6)); }
	| EXIT NUMBER ',' SYMBOL 		{ SEMIT(exit($2, $4)); }
	| GOSUB SYMBOL ',' distance 		{ SEMIT(gosub($2, $4)); }
	| GOADD v				{ SEMIT(goadd($2)); }
	| CSS					{ CEMIT(css()); }
	| GO v ',' distance ',' ex ',' ctx	{ SEMIT(go($2,$4,$6,$8)); }
	| GOEQ v ',' distance ',' ex ',' ctx  	{ CEMIT(goeq($2,$4,$6,$8)); }
	| GONE v ',' distance ',' ex ',' ctx  	{ CEMIT(gone($2,$4,$6,$8)); }
	| GOGE v ',' distance ',' ex ',' ctx  	{ CEMIT(goge($2,$4,$6,$8)); }
	| GOGR v ',' distance ',' ex ',' ctx  	{ CEMIT(gogr($2,$4,$6,$8)); }
	| GOLE v ',' distance ',' ex ',' ctx  	{ CEMIT(gole($2,$4,$6,$8)); }
	| GOLT v ',' distance ',' ex ',' ctx  	{ CEMIT(golt($2,$4,$6,$8)); }
	| GOPC v ',' distance ',' ex ',' ctx  	{ CEMIT(gopc($2,$4,$6,$8)); }
	| GOND v ',' distance ',' ex ',' ctx  	{ CEMIT(gond($2,$4,$6,$8)); }
	| FSTK			{ CEMIT(fstk()); }
	| BSTK			{ CEMIT(bstk()); }
	| CFSTK			{ CEMIT(cfstk()); }
	| UNSTK v		{ CEMIT(unstk($2)); }
	| FMOVE			{ CEMIT(fmove()); }
	| BMOVE			{ CEMIT(bmove()); }
	| MESS STRING		{ CEMIT(mess($2)); }
	| NB STRING		{ emit_nb($2); }
	| PRGST STRING		{ emit_prgst($2); }
	| PRGEN			{ emit_prgen(); }
	| ALIGN			{ emit_align(); }
	| ml1_code_statement
	;

charname:
	NLREP			{ $$ = '\n'; }
	| SPREP			{ $$ = ' '; }
	| TABREP		{ $$ = '\t'; }
	| QUTREP		{ $$ = '"'; }
	| ml1_charname		{ $$ = $1; }
	;

v:	SYMBOL			{ $$ = $1; }
	;

distance:
	signnum			{ $$ = $1; }
	| SYMBOL		{
					_CHECK_CH1($1, 'X');
					$$ = 0;
				}
	;

nof:
	signnum			{ $$ = $1; }
	| of_macro		{ $$ = $1; }
	;

signnum:
	NUMBER			{ $$ = $1; }
	| '-' NUMBER		{ $$ = -$2; }
	;

of_macro:
	OF '(' expr ')'		{ $$ = $3; }
	| '-' OF '(' expr ')'	{ $$ = -$4; }
	;

expr:
	NUMBER
	| LCH			{ $$ = LCH_VAL; }
	| LNM			{ $$ = LNM_VAL; }
	| LICH			{ $$ = LICH_VAL; }
	| LHV			{ $$ = LHV_VAL; } /* ML/I */
	| expr '+' expr		{ $$ = $1 + $3; }
	| expr '-' expr		{ $$ = $1 - $3; }
	| expr '*' expr		{ $$ = $1 * $3; }
	;


rx: 	SYMBOL			{
					_CHECK_CH2($1, 'R', 'X');
					$$ = *$1;
				}
	;
dc:	SYMBOL			{
					_CHECK_CH2($1, 'D', 'C');
					$$ = *$1;
				}
	;
px:	SYMBOL			{
					_CHECK_CH2($1, 'P', 'X');
					$$ = *$1;
				}
	;
ax:	SYMBOL			{
					_CHECK_CH2($1, 'A', 'X');
					$$ = *$1;
				}
	;
ex:	SYMBOL			{
					_CHECK_CH2($1, 'E', 'X');
					$$ = *$1;
				}
	;
ctx:	SYMBOL			{
					_CHECK_CH3($1, 'C', 'T', 'X');
					$$ = *$1;
				}
	;
parnm:
	SYMBOL			{
					if ( !strcmp($1, "PARNM") )
						$$ = 1;
					else {
						_CHECK_CH1($1, 'X');
						$$ = 0;
					}
				}
	;

ml1_table_statement:
	HASH STRING		{ emit_hash($2); } 	/* ML/I */
	| THASH			{ emit_thash();	}	/* ML/I */
	| RL SYMBOL ',' nof 	{ emit_rl($2,$4); }	/* ML/I */
	| WTHS			{ emit_con(WTHS_VAL); } /* ML/I */
	;

ml1_code_statement:
	LINKR SYMBOL		{ emit_linkr($2); }	/* ML/I */
	| LINKB			{ emit_linkb(); }	/* ML/I */
	| ORL nof		{ emit_orl($2); }	/* ML/I */
	;

ml1_charname:
	STOPCD			{ $$ = 0; } 	/* ML/I */
	| SLREP			{ $$ = 255; }	/* ML/I */
	;
%%

#define IDSYM_HASHSZ 13
struct idsym_e {
	char 		*sym;
	uintptr_t	val;
	struct idsym_e 	*next;
} *idsym_tbl[IDSYM_HASHSZ];

int hashfn(char *sym)
{
	return (*sym) % IDSYM_HASHSZ;
}

void
add_idsym(char *sym, uintptr_t val)
{
	int slot;
	struct idsym_e *e;
	slot = hashfn(sym);

	e = malloc(sizeof(struct idsym_e));
	if ( e == NULL ) panic("Out of memory");
	e->sym = sym;
	e->val = val;
	e->next = idsym_tbl[slot];
	idsym_tbl[slot] = e;
}

int
get_idsym(char *sym, uintptr_t *val)
{
	int slot;
	struct idsym_e *e;

	slot = hashfn(sym);
	e = idsym_tbl[slot];
	while ( e != NULL ) {
		if ( !strcmp(sym, e->sym) ) {
			*val = e->val;
			return 1;
		}
		e = e->next;
	}
	return 0;
}

void yyerror(char *s)
{
	fprintf(stderr, "%d:%s\n", yylineno, s);
}

int main(int argc, char **argv)
{
	emitter_init();
	yyparse();
	emitter_fini();
	return 0;
}
