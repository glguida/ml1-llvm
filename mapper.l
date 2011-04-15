digit 		[0-9]
letter		[a-zA-Z]

symbol  	{letter}({digit}|{letter})*

%{
#include <stdlib.h>
#include "y.tab.h"

int get_idsym(char *, uintptr_t *);

void yyerror(char *);
%}

%%

[ \t]		/* Ignore whitepaces. */;

"DCL"		{	return DCL;	}
"EQU"		{	return EQU;	}
"IDENT"		{	return IDENT;	}
"CON"		{	return CON;	}
"NCH"		{	return NCH;	}
"STR"		{	return STR;	}
"LAV"		{	return LAV;	}
"LBV"		{	return LBV;	}
"LAL"		{	return LAL;	}
"LCN"		{	return LCN;	}
"LAM"		{	return LAM;	}
"LCM"		{	return LCM;	}
"LAI"		{	return LAI;	}
"LCI"		{	return LCI;	}
"LAA"		{	return LAA;	}
"STV"		{	return STV;	}
"STI"		{	return STI;	}
"CLEAR"		{	return CLEAR;	}
"AAV"		{	return AAV;	}
"ABV"		{	return ABV;	}
"AAL"		{	return AAL;	}
"SAV"		{	return SAV;	}
"SBV"		{	return SBV;	}
"SAL"		{	return SAL;	}
"SBL"		{	return SBL;	}
"MULTL"		{	return MULTL;	}
"BUMP"		{	return BUMP;	}
"ANDV"		{	return ANDV;	}
"ANDL"		{ 	return ANDL;	}
"CAV"		{	return CAV;	}
"CAL"		{	return CAL;	}
"CCL"		{	return CCL;	}
"CCN"		{	return CCN;	}
"CAI"		{	return CAI;	}
"CCI"		{	return CCI;	}
"SUBR"		{	return SUBR;	}
"EXIT"		{	return EXIT;	}
"GOSUB"		{	return GOSUB;	}
"GOADD"		{	return GOADD;	}
"CSS"		{	return CSS;	}
"GO"		{	return GO;	}
"GOEQ"		{	return GOEQ;	}
"GONE"		{	return GONE;	}
"GOGE"		{	return GOGE;	}
"GOGR"		{	return GOGR;	}
"GOLE"		{	return GOLE;	}
"GOLT"		{	return GOLT;	}
"GOPC"		{	return GOPC;	}
"GOND"		{	return GOND;	}
"FSTK"		{	return FSTK;	}
"BSTK"		{	return BSTK;	}
"CFSTK"		{	return CFSTK;	}
"UNSTK"		{	return UNSTK;	}
"FMOVE"		{	return FMOVE;	}
"BMOVE"		{	return BMOVE;	}
"MESS"		{	return MESS;	}
"NB"		{	return NB;	}
"PRGST"		{	return PRGST;	}
"PRGEN"		{	return PRGEN;	}
"ALIGN"		{	return ALIGN;	}

"NLREP"		{	return NLREP;	}
"SPREP"		{	return SPREP;	}
"TABREP"	{	return TABREP;	}
"QUTREP"	{	return QUTREP;	}

"OF"		{	return OF;	}
"LCH"		{	return LCH;	}
"LNM"		{	return LNM;	}
"LICH"		{	return LICH;	}

 /* ML/I Extensions. */
"HASH"		{	return HASH;	}
"THASH"		{	return THASH;	}
"RL"		{	return RL;	}
"WTHS"		{	return WTHS;	}
"STOPCD"	{	return STOPCD;	}
"SLREP"		{	return SLREP;	}
"LHV"		{	return LHV;	}
"LINKR"		{	return LINKR;	}
"LINKB"		{	return LINKB;	}
"ORL"		{	return ORL;	}

"'"[^'\n]+"'"	{
			/* Remove lead and tail quote. */
			yylval.str = strdup(yytext + 1);
			yylval.str[yyleng - 2] = 0;
			return STRING;
		}

{symbol}	{
			uintptr_t idval;
			if ( get_idsym(yytext, &idval) ) {
				yylval.num = idval;
				return NUMBER;
			}
			yylval.str = strdup(yytext);
			return SYMBOL;
		}

(0|[1-9]({digit})*)	{ 
			yylval.num = atoi(yytext);
		  	return NUMBER;
		}

"["{symbol}"]"	{
			/* Remove lead and tail bracket. */
			yylval.str = strdup(yytext + 1);
			yylval.str[yyleng - 2] = 0;
			return LABEL;
		}

[,+\-*/:><()=$]	{
			return *yytext;
		}



\n		{
			yylineno++;
			return EOL;
		}

.		{
			yyerror("Invalid Character.");
			yyerror(yytext);
			exit(-1);
		}

%%

int yywrap(void)
{
	return 1;
}
