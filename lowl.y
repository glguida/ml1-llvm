%{
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "config.h"
#include "emit.h"

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

%token LCH LNM LICH 
%token OF 
%token ',' '+' '-' '*' '(' ')' 
%token NLREP SPREP TABREP QUTREP
%token EOL 

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
	;

code_statement:
	LAV v ',' rx		{ emit_lav($2,$4); }
	| LBV v			{ emit_lbv($2); }
	| LAL nof		{ emit_lal($2); }
	| LCN charname		{ emit_lcn($2); }
	| LAM nof		{ emit_lam($2); }
	| LCM nof		{ emit_lcm($2); }
	| LAI v ',' rx		{ emit_lai($2,$4); }
	| LCI v ',' rx		{ emit_lci($2,$4); }
	| LAA v ',' dc		{ emit_laa($2,$4); }
	| STV v ',' px		{ emit_stv($2,$4); }
	| STI v ',' px		{ emit_sti($2,$4); }
	| CLEAR v		{ emit_clear($2); }
	| AAV v			{ emit_aav($2); }
	| ABV v			{ emit_abv($2); }
	| AAL nof		{ emit_aal($2); }
	| SAV v			{ emit_sav($2); }
	| SBV v			{ emit_sbv($2);	}
	| SAL nof		{ emit_sal($2); }
	| SBL nof		{ emit_sbl($2); }
	| MULTL nof		{ emit_multl($2); }
	| BUMP v ',' nof	{ emit_bump($2, $4); }
	| ANDV v 		{ emit_andv($2); }
	| ANDL NUMBER		{ emit_andl($2); }
	| CAV v ',' ax		{ emit_cav($2); }
	| CAL nof		{ emit_cal($2); }
	| CCL STRING		{ _CHECK_CHAR($2); emit_ccl($2); }
	| CCN charname		{ emit_ccl($2); /* CCL == CCN here. */ }
	| CAI v ',' ax		{ emit_cai($2,$4); }
	| CCI v 		{ emit_cci($2); }
	| SUBR SYMBOL ',' parnm ',' NUMBER 	{ emit_subr($2, $4, $6); }
	| EXIT NUMBER ',' SYMBOL 		{ emit_exit($2, $4); }
	| GOSUB SYMBOL ',' distance 		{ emit_gosub($2, $4); }
	| GOADD v				{ emit_goadd($2); }
	| CSS					{ emit_css(); }
	| GO v ',' distance ',' ex ',' ctx	{ emit_go($2,$4,$6,$8); }
	| GOEQ v ',' distance ',' ex ',' ctx  	{ emit_goeq($2,$4,$6,$8); }
	| GONE v ',' distance ',' ex ',' ctx  	{ emit_gone($2,$4,$6,$8); }
	| GOGE v ',' distance ',' ex ',' ctx  	{ emit_goge($2,$4,$6,$8); }
	| GOGR v ',' distance ',' ex ',' ctx  	{ emit_gogr($2,$4,$6,$8); }
	| GOLE v ',' distance ',' ex ',' ctx  	{ emit_gole($2,$4,$6,$8); }
	| GOLT v ',' distance ',' ex ',' ctx  	{ emit_golt($2,$4,$6,$8); }
	| GOPC v ',' distance ',' ex ',' ctx  	{ emit_gopc($2,$4,$6,$8); }
	| GOND v ',' distance ',' ex ',' ctx  	{ emit_gond($2,$4,$6,$8); }
	| FSTK			{ emit_fstk(); }
	| BSTK			{ emit_bstk(); }
	| CFSTK			{ emit_cfstk(); }
	| UNSTK v		{ emit_unstk($2); }
	| FMOVE			{ emit_fmove(); }
	| BMOVE			{ emit_bmove(); }
	| MESS STRING		{ emit_mess($2); }
	| NB STRING		{ emit_nb($2); }
	| PRGST STRING		{ emit_prgst($2); }
	| PRGEN			{ emit_prgen(); }
	| ALIGN			{ emit_align(); }
	;

charname:
	NLREP			{ $$ = '\n'; }
	| SPREP			{ $$ = ' '; }
	| TABREP		{ $$ = '\t'; }
	| QUTREP		{ $$ = '"'; }
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
