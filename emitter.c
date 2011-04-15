#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lowl.h"

/*
 * Support functions
 */

#define w(...) printf(__VA_ARGS__)

/* Scream when you see a bug! */
#define EMIT_PANIC(_s)				\
	fprintf(stderr, "%s:%d : %s\n", 	\
		__FILE__, __LINE__, (_s));	\
	exit(-1);

/* Out of memory. */
static void oom(void)
{
	fprintf(stderr, "Out of memory!\n");
}

/*
 * LLVM Functions and separation between code and data:
 *
 * As code in LLVM assembler must be included into functions,
 * we need to realize when we start generating code so that
 * we can create one big function that contains all the code.
 *
 * Based on the following assumptions,
 *
 * - LOWL programs will define data and code sections in two
 *   separate stages,
 * - Data are defined before code,
 * - The first opcode will be tagged by a label (the LOWL 
 *   manual specifies that this label will be begin, but
 *   this is irrelevant to us),
 *
 * this emitter creates a function at the first call of
 * emit_label(), which gets called only when a label is
 * associated with an instruction.
 */
int function_created = 0;

/*
 * LOWL Program Counter.
 * We simulate a program counter to let jump to specific or
 * relative addresses possible.
 * To be sure that in the code we have the value of current
 * PC, we start at 0 and we increment it when we finish
 * emitting an instruction. */
long emitter_pc = 0;

#ifdef LOWL_ML1
/*
 * ML/I Hash Table support.
 * The MD specification for ML/I requires to build an hash
 * chain in the LOWL table. We do that in two phases:
 * - First, we memorize in the tbl structure chain
 *   number it must be stored into and the table offset.
 * - Later, when dumping the table, we use hash_getlink()
 *   function to retrieve the address of the last element
 *   stored in the chain. We store this value in our link
 *   and update register our offset as the last entry in
 *   the chain.
 */
lowlint_t hash_links[ML1_HASHSZ];

lowlint_t
hash_getlink(unsigned chain)
{
	assert ( chain < ML1_HASHSZ );
	return hash_links[chain];
}

void
hash_savelink(unsigned chain, unsigned long offset)
{
	assert ( chain < ML1_HASHSZ );
	hash_links[chain] = offset;
}

void
hash_emitlink(unsigned chain)
{
	assert ( chain < ML1_HASHSZ );
	lowlint_t link = hash_getlink(chain);
	if ( link == 0 ) {
		/* NULL pointer, don't add @LOWLTABLE offset. */
		w("%%LLNUM 0");
		return;
	}
	/* Get table offset of next entry from hash_getlink()
	 * and calculate pointer. */
	w("%%LLNUM ptrtoint(i8* getelementptr(%s, %%LLNUM %"PRIdLWI") to %%LLNUM)\n",
	  "i8* bitcast (%lowltabty* @LOWLTAB to i8*)",
	  hash_getlink(chain));
}

void
hash_emitthash(void)
{
	int i;
	/* Emit array of links. Since all the hash table have
	 * been scanned, this will generate the head of the 
	 * hash chains. */
	w("[ %d x %%LLNUM ] [ ", ML1_HASHSZ);
	for ( i = 0; i < ML1_HASHSZ; i++ ) {
		if ( i != 0 ) w(", ");
		hash_emitlink(i);
	}
	w(" ]");
}
#endif

/* Table support:
 * LLVM assembler does not allow to specify a data segment
 * the way that machine-specific untyped assembler do.
 *
 * In order to create the LOWL tables, this mapper's
 * approach is to scan for the whole file, collecting
 * information about the LOWL program table, and
 * dump it later in form of a packed structure.
 *
 * Labels are stored as constants retaining the value of
 * the offset in this table. LAA X, C handles this special
 * case.
 */
struct tble {
	int type;
	union {
		char 		ch;
		char 		*str;
		uintptr_t	num;
#ifdef LOWL_ML1
		struct {
			int 		chain;
			unsigned long	off;
		} h;
#endif
	} u;
	struct tble *next;
};

enum {
	TBL_CH,
	TBL_STR,
	TBL_NUM,
#ifdef LOWL_ML1
	TBL_HASH,
	TBL_THASH,
#endif
};

struct tble *last, *tbl = NULL;
unsigned long tbl_size = 0;

void
tbl_append(struct tble *ptr)
{
	if ( tbl == NULL )
		tbl = last = ptr;
	else {
		last->next = ptr;
		last = ptr;
	}

	/* Collect Table Size. */
	switch(ptr->type) {
	case TBL_CH:
		tbl_size++;
		break;
	case TBL_STR:
		tbl_size += strlen(ptr->u.str);
		break;
	case TBL_NUM:
		tbl_size += LLVM_PTRSIZE / 8;
		break;
#ifdef LOWL_ML1
	case TBL_HASH:
		tbl_size += LLVM_PTRSIZE / 8;
		break;
	case TBL_THASH:
		tbl_size += ML1_HASHSZ * (LLVM_PTRSIZE/8);
		break;
#endif
	default:
		EMIT_PANIC("Wrong TBL Type!");
	}
}

void
tbl_dump(void)
{
	struct tble *ptr;

	/* The LOWL Table is represented in LLVM as a packed structure.
	 * We scan the table two times, first time to get the types,
	 * second one to the values. */
	w("\n\n;\n; LOWL Table\n;\n");

	/* First pass. Give the structure a type name. */
	w("%%lowltabty = type < { ");
	ptr = tbl;
	while ( ptr != NULL ) {
		if ( ptr != tbl ) w(", ");
		switch (ptr->type)
		{
		case TBL_CH:
			w("i8");
			break;
		case TBL_NUM:
			w("%%LLNUM");
			break;
		case TBL_STR:
			w("[ %d x i8 ]", (int)strlen(ptr->u.str));
			break;
#ifdef LOWL_ML1
		case TBL_HASH:
			w("%%LLNUM");
			break;
		case TBL_THASH:
			w("[ %d x %%LLNUM]", ML1_HASHSZ);
			break;
#endif
		default:
			EMIT_PANIC("Wrong TBL Type!");
		}
		ptr = ptr->next;
	}
	w(" } >\n");

	/* Second pass. Write declaration. */
	w("@LOWLTAB = internal global %%lowltabty < { ");
	ptr = tbl;
	while ( ptr != NULL ) {
		if ( ptr != tbl ) w(", ");
		switch (ptr->type)
		{
		case TBL_CH:
			w("i8 %d", ptr->u.ch);
			break;
		case TBL_NUM:
			w("%%LLNUM %ld", ptr->u.num);
			break;
		case TBL_STR:
			w("[ %d x i8 ] c\"%s\"", 
			  (int)strlen(ptr->u.str), ptr->u.str);
			break;
#ifdef LOWL_ML1
		case TBL_HASH:
			hash_emitlink(ptr->u.h.chain);
			/* Save current offset to be used by next entry in
			 * the chain. */
			hash_savelink(ptr->u.h.chain, ptr->u.h.off);
			break;
		case TBL_THASH:
			hash_emitthash();
			break;
#endif
		default:
			EMIT_PANIC("Wrong TBL Type!");
		}
		ptr = ptr->next;
	}
	w(" } >; \n ");
}

/*
 * Implicit data declaration.
 * Strings used in MESS are implicit in LOWL but in LLVM must
 * be declared outside of the function. Save them in a list and
 * declare later.
 */
struct strdecl {
	int id;
	char *str;
	struct strdecl *next;
} *strdecls = NULL;
int strings = 0;

int
str_declare(char *str)
{
	struct strdecl *ptr = malloc(sizeof(struct strdecl));
	if ( ptr == NULL ) oom();
	ptr->str = str;
	ptr->id = strings;
	ptr->next = strdecls;
	strdecls = ptr;
	return strings++;
}

void
str_dump()
{
	struct strdecl *ptr = strdecls;
	while ( ptr != NULL )
	{
		w("@STR%d = internal constant [ %d x i8 ] c\"%s\\00\";\n",
		  ptr->id, (int)strlen(ptr->str)+1, ptr->str);
		ptr = ptr->next;
	}
}

/*
 * Call Graph Analysis.
 *
 * In order to implement indirect branches in LLVM, apart
 * from using a dispatch basic block which would pollute
 * terribly the CFG, we need to know, in 'indirectbr'
 * or 'switch', all the possible outcomes.
 * This is implemented in two phases: 
 * - First, we collect, when emitting gosub, the source and
 *   destination labels.
 *   EXIT n in subroutine will emit code that will jump to a
 *   basic block whose emission is postponed.
 * - Then, after we scanned the whole code, we can create the
 *   EXIT n basic block, with proper 'switch' instruction  and
 *   the full list of possible destinations.
 *   The  EXIT n basic block  will do the following:
 *     - Get from lowl_stack the pc of the caller.
 *     - Do an indirect branch based on the exit code (known 
 *       at emitting time) to the destination.
 */
struct callgraphe {
	char *symbol;
	int exitnr;
	int parnm;
	int linkr;
	struct cg_pclist {
		long pc;
		struct cg_pclist *next;
	} *pclist;
	struct callgraphe *next;
};
struct callgraphe *callgraph = NULL;

void
callgraph_add(char *dst, long src_pc)
{
	struct cg_pclist *pcl;
	struct callgraphe *cge;

	/* Create cg_pclist structure. */
	pcl = malloc(sizeof(struct cg_pclist));
	if ( pcl == NULL ) oom();
	pcl->pc = src_pc;


	/* Search for dst's pc_list */
	cge = callgraph;
	while (	cge != NULL ) {
		/* Exit if found. */
		if ( !strcmp(cge->symbol, dst) )
			break;
		cge = cge->next;
	}
	/* If not found, create. */
	if ( cge == NULL ) {
		cge = malloc(sizeof(struct callgraphe));
		if ( cge == NULL ) oom();
		cge->symbol = dst;
		cge->pclist = NULL;
		cge->next = callgraph;
		callgraph = cge;
	}
	/* Add pc to cge's pclist */
	pcl->next = cge->pclist;
	cge->pclist = pcl;
}

void
callgraph_addsubr(char *subr, char parnm, char exitnr, int linkr)
{
	struct callgraphe *cge;
	/* Search for Subr */
	cge = callgraph;
	while (	cge != NULL ) {
		/* Exit if found. */
		if ( !strcmp(cge->symbol, subr) )
			break;
		cge = cge->next;
	}
	/* If not found, create. */
	if ( cge == NULL ) {
		cge = malloc(sizeof(struct callgraphe));
		if ( cge == NULL ) oom();
		cge->symbol = subr;
		cge->pclist = NULL;
		cge->next = callgraph;
		callgraph = cge;
	}
	cge->exitnr = exitnr;
	cge->parnm = parnm;
	cge->linkr = linkr;
}

void
callgraph_emit_exitbb(struct callgraphe *ptr)
{
	int i;
	struct cg_pclist *pcl;

	static int cnt = 0;

	for ( i = 1; i <= ptr->exitnr; i++ ) {
		w("lowl_exit_%s_%d:\n", ptr->symbol, i);
		if ( ptr->linkr ) {
			w("%%exitaddr.%d = load %%LLNUM* @LINKPT\n", cnt);
		} else {
			w("%%exitaddr.%d = call %%LLNUM @lowl_poplink();\n",
			  cnt);
		}
		w("switch %%LLNUM %%exitaddr.%d, label %%exit_jmperr [ ", cnt);
		pcl = ptr->pclist;
		while ( pcl != NULL ) {
			w(" %%LLNUM %lu, label %%LOWL_LINE_%ld ",
			   pcl->pc, pcl->pc + i);
			pcl = pcl->next;
		}
		w("] \n");
		cnt++;
	}
}

void
callgraph_dump()
{
	struct callgraphe *ptr;
	struct cg_pclist *pcl;
	ptr = callgraph;
	while ( ptr != NULL ) {
		w("\n");
		callgraph_emit_exitbb(ptr);
		w("; %s is called from: ", ptr->symbol);
		pcl = ptr->pclist;
		while ( pcl != NULL ) {
			w("%ld,", pcl->pc);
			pcl = pcl->next;
		}
		w("\n");
		if ( ptr->linkr )
			w("; %s is a linkroutine.\n", ptr->symbol);
		else
			w("; %s has %d exits and %s PARNM.\n", ptr->symbol,
			ptr->exitnr, ptr->parnm ? "has" : "does not have");
		ptr = ptr->next;
	}
	w("\n");
}

/*
 * Emitter setup
 */

/* Initialization. */
void
emitter_init(void)
{
	/* Print some basic banner. */
	w(";\n; This file has been autogenerated by LOWL-LLVM mapper.\n");
	w(";\n\n\n");
	/* Define basic types. */
	w("; Basic types definitions.\n");
	w("%%LLNUM = type i%d; Numerical is %d bits\n",
	  LLVM_PTRSIZE, LLVM_PTRSIZE);
	w("\n");
	w("; External declarations.\n");
	w("declare void @lowl_puts(i8*);\n");
	w("declare void @lowl_pushlink(%%LLNUM);\n");
	w("declare %%LLNUM @lowl_poplink();\n");
	w("declare void @lowl_clearlink();\n");
	w("declare void @lowl_goadd_jmperror();\n");
	w("declare void @lowl_exit_jmperror();\n");
	w("declare i8 @lowl_punctuation(i8);\n");
	w("declare i8 @lowl_digit(i8);\n");
	w("declare void @lowl_bmove(%%LLNUM)\n");
	w("declare void @lowl_fmove(%%LLNUM)\n");

	emitter_md_init();
}

/* Finalization. */
void
emitter_fini(void)
{
	/* Terminate the function, just in case. */
	w("ret void\n\n");

	/* Emit exit basic blocks. */
	callgraph_dump();

	/* Terminate and close the LLVM function. */
	w("\n; End of LOWL code\nret void;\n}\n\n");

	/* Declare MESS strings. */
	str_dump();

	w("\n\n");
}


/*
 * EMITTER FUNCTIONS START HERE.
 */


void emit_table_label(char *lbl)
{
	/* Table labels are simple constants whose value is
	 * the offset from the table start. Even if we haven't dumped
	 * the table still, the current offset can be found in current
	 * tbl_size value. */
	w("@%s = constant %%LLNUM %ld;    [%s]\n", lbl, tbl_size, lbl);
}


void emit_label(char *lbl)
{
	/* See comment before function_created. */
	if ( !function_created ) {
		/* Dump LOWL Table now. */
		tbl_dump();
		w("\n");
		w("\n;\n; LOWL LLVM function\n");
		w("define void @lowl_main(%%LLNUM %%ffpt, %%LLNUM %%lfpt)\n");
		w("{\n");
		w("; Allocate LOWL registers on stack.\n");
		w("%%A_REG = alloca %%LLNUM\n");
		w("%%B_REG = alloca %%LLNUM\n");
		w("%%C_REG = alloca i8\n");
		w("; Allocate compare result register on stack.\n");
		w("%%CMP = alloca %%LLNUM\n");
		w("; Initialize LOWL stack.\n");
		w("store %%LLNUM %%ffpt, %%LLNUM* @FFPT\n");
		w("store %%LLNUM %%lfpt, %%LLNUM* @LFPT\n");
		w("br label %%BEGIN;   Jump to BEGIN\n");
		w("\n");
		w(";\n; Support Basic Blocks\n;\n");
		w("goadd_jmperr:\n");
		w("call void @lowl_goadd_jmperror();\n");
		w("unreachable\n");
		w("exit_jmperr:\n");
		w("call void @lowl_exit_jmperror();\n");
		w("unreachable\n");
		w("\n");
		function_created = 1;
	} else {
		/* LLVM assembler can't fall through labels,
		 * as internally it thinks in basic blocks.
		 * When adding a label in a middle of some
		 * code, add an inconditional branch to it
		 * so that the two basic blocks are
		 * continguous. */
		w("br label %%%s\n\n", lbl);
	}
	w("%s:\n", lbl);
}



void emit_newpc(int stp)
{

	/* To keep track of jumps relative to current
	 * location of the text files, for example
	 * in GOADD or EXIT, we create a basic block
	 * for each code statement.
	 * This is unfortunate, but it would require
	 * at least a second pass to implement this
	 * in a more saner way.
	 * As everywhere else, though, we can trust
	 * LLVM for eliminating most of this useless
	 * code. */
	emitter_pc++;
	if ( !stp )
		w("br label %%LOWL_LINE_%ld", emitter_pc);
	w("\nLOWL_LINE_%ld:\n", emitter_pc);
}


void emit_eol()
{
	w("\n");
}


void emit_dcl(char *var)
{

	w("@%s = global %%LLNUM zeroinitializer;    DCL %s\n", var, var);
}


void emit_equ(char *arg1, char *arg2)
{
	w("@%s = alias %%LLNUM* @%s;    EQU %s %s\n",
	  arg1, arg2, arg1, arg2);
}


void emit_ident()
{
	/* IDENT handled in the parser. */
}


void emit_con(uintptr_t num)
{
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_NUM;
	tble->u.num = num;
	tbl_append(tble);
}


void emit_nch(char c)
{
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_CH;
	tble->u.ch = c;
	tbl_append(tble);
}


void emit_str(char *str)
{
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_STR;
	tble->u.str = str;
	tbl_append(tble);
}


/* ML/I LOWL Table Items extensions. */
void emit_hash(char *str)
{
#ifdef LOWL_ML1
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_HASH;
	tble->u.h.chain = ml1_hash(str, strlen(str));
	tble->u.h.off = tbl_size;
	tbl_append(tble);
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif

}


void emit_thash()
{
#ifdef LOWL_ML1
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_THASH;
	tbl_append(tble);
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif
}


void emit_rl(char *str, intptr_t nof)
{
#ifdef LOWL_ML1
	/* Do not calculate the pointer, use the subsidiary expr instead. */
	struct tble *tble;
	tble = malloc(sizeof(struct tble));
	if ( tble == NULL )
		oom();
	tble->type = TBL_NUM;
	tble->u.num = nof;
	tbl_append(tble);
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif
}


void emit_lav(char *v, char rx)
{
	static int lav_cnt = 0;
	w("%%lav.%d = load %%LLNUM* @%s;    LAV %s, %c\n", lav_cnt, v, v, rx);
	w("store %%LLNUM %%lav.%d, %%LLNUM* %%A_REG;\n", lav_cnt);
	lav_cnt++;
}

void emit_lbv(char *v)
{
	static int lbv_cnt = 0;
	w("%%lbv.%d = load %%LLNUM* @%s;    LBV %s\n", lbv_cnt, v, v);
	w("store %%LLNUM %%lbv.%d, %%LLNUM* %%B_REG;\n", lbv_cnt);
	lbv_cnt++;
}


void emit_lal(intptr_t nof)
{
	w("store %%LLNUM %ld, %%LLNUM* %%A_REG;    LAL %ld\n",
	  nof, nof);
}


void emit_lcn(char cn)
{
	w("store i8 %d, i8* %%C_REG;    LCN '%d'\n", cn, cn);
}


void emit_lam(intptr_t nof)
{
	static int lam_cnt = 0;
	w("%%lam.%d = load %%LLNUM* %%B_REG;    LAM %ld\n", lam_cnt, nof);
	w("%%lam.2.%d = add %%LLNUM %%lam.%d, %ld\n", lam_cnt, lam_cnt, nof);
	w("%%lam.3.%d = inttoptr %%LLNUM %%lam.2.%d to %%LLNUM*\n",
	  lam_cnt, lam_cnt);
	w("%%lam.4.%d = load %%LLNUM* %%lam.3.%d\n", lam_cnt, lam_cnt);
	w("store %%LLNUM %%lam.4.%d, %%LLNUM* %%A_REG\n", lam_cnt);
	w("store %%LLNUM %%lam.2.%d, %%LLNUM* %%B_REG\n", lam_cnt);
	lam_cnt++;
}


void emit_lcm(intptr_t nof)
{
	static int cnt = 0;
	w("%%lcm.c.%d = load %%LLNUM* %%B_REG\n", cnt);
	w("%%lcm.n.%d = add %%LLNUM %%lcm.c.%d, %ld\n", cnt, cnt, nof);
	w("%%lcm.p.%d = inttoptr %%LLNUM %%lcm.n.%d to i8*\n", cnt, cnt);
	w("%%lcm.r.%d = load i8* %%lcm.p.%d\n", cnt, cnt);
	w("store i8 %%lcm.r.%d\n, i8* %%C_REG\n", cnt);
	w("store %%LLNUM %%lcm.n.%d\n, %%LLNUM* %%B_REG\n", cnt);
	cnt++;
}


void emit_lai(char *v)
{
	static int cnt = 0;
	w("%%lai.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%lai.p.%d = inttoptr %%LLNUM %%lai.v.%d to %%LLNUM*\n", cnt, cnt);
	w("%%lai.r.%d = load %%LLNUM* %%lai.p.%d\n", cnt, cnt);
	w("store %%LLNUM %%lai.r.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_lci(char *v)
{
	static int cnt = 0;
	w("%%lci.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%lci.p.%d = inttoptr %%LLNUM %%lci.v.%d to i8*\n", cnt, cnt);
	w("%%lci.r.%d = load i8* %%lci.p.%d\n", cnt, cnt);
	w("store i8 %%lci.r.%d, i8* %%C_REG\n", cnt);
	cnt++;
}


void emit_laa(char *v, char dc)
{
	static int cnt = 0;
	if ( dc == 'D' ) {
		w("%%laa.p.%d = getelementptr %%LLNUM* @%s\n", cnt, v);
		w("%%laa.v.%d = ptrtoint %%LLNUM* %%laa.p.%d to %%LLNUM\n",
		  cnt, cnt);
	} else {
		w("%%laa.o.%d = load %%LLNUM* @%s\n", cnt, v);
		w("%%laa.t.%d = ptrtoint %%lowltabty* @LOWLTAB to %%LLNUM\n",
		  cnt);
		w("%%laa.v.%d = add %%LLNUM %%laa.t.%d, %%laa.o.%d\n",
		  cnt, cnt, cnt);
	}
	w("store %%LLNUM %%laa.v.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_stv(char *v, char px)
{
	static int stv_cnt = 0;
	w("%%stv.%d = load %%LLNUM* %%A_REG;    STV %s, %c\n", stv_cnt, v, px);
	w("store %%LLNUM %%stv.%d, %%LLNUM* @%s\n", stv_cnt, v);
	stv_cnt++;
}


void emit_sti(char *v)
{
	static int cnt = 0;
	w("%%sti.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%sti.p.%d = inttoptr %%LLNUM %%sti.v.%d to %%LLNUM*\n", cnt, cnt);
	w("%%sti.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("store %%LLNUM %%sti.a.%d, %%LLNUM* %%sti.p.%d\n", cnt, cnt);
	cnt++;
}


void emit_clear(char *v)
{
	w("store %%LLNUM 0, %%LLNUM* @%s;    CLEAR %s\n", v, v);
}


void emit_aav(char *v)
{
	static int aav_cnt = 0;
	w("%%aav.%d = load %%LLNUM* %%A_REG;    ABV %s\n", aav_cnt, v);
	w("%%aav.2.%d = load %%LLNUM* @%s\n", aav_cnt, v);
	w("%%aav.3.%d = add %%LLNUM %%aav.%d, %%aav.2.%d\n",
	  aav_cnt, aav_cnt, aav_cnt);
	w("store %%LLNUM %%aav.3.%d, %%LLNUM* %%A_REG\n", aav_cnt);
	aav_cnt++;
}


void emit_abv(char *v)
{
	static cnt = 0;
	w("%%abv.%d = load %%LLNUM* %%B_REG;    AAV %s\n", cnt, v);
	w("%%abv.2.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%abv.3.%d = add %%LLNUM %%abv.%d, %%abv.2.%d\n",
	  cnt, cnt, cnt);
	w("store %%LLNUM %%abv.3.%d, %%LLNUM* %%B_REG\n", cnt);
	cnt++;
}


void emit_aal(intptr_t nof)
{
	static cnt = 0;
	w("%%aal.%d = load %%LLNUM* %%A_REG;    AAL %ld\n", cnt, nof);
	w("%%aal.2.%d = add %%LLNUM %%aal.%d, %ld\n", cnt, cnt, nof);
	w("store %%LLNUM %%aal.2.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_sav(char *v)
{
	static int sav_cnt = 0;
	w("%%sav.%d = load %%LLNUM* %%A_REG;    SAV %s\n", sav_cnt, v);
	w("%%sav.2.%d = load %%LLNUM* @%s\n", sav_cnt, v);
	w("%%sav.3.%d = sub %%LLNUM %%sav.%d, %%sav.2.%d\n",
	  sav_cnt, sav_cnt, sav_cnt);
	w("store %%LLNUM %%sav.3.%d, %%LLNUM* %%A_REG\n", sav_cnt);
	sav_cnt++;
}


void emit_sbv(char *v)
{
	static int cnt = 0;
	w("%%sbv.%d = load %%LLNUM* %%B_REG;    SBV %s\n", cnt, v);
	w("%%sbv.2.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%sbv.3.%d = sub %%LLNUM %%sbv.%d, %%sbv.2.%d\n",
	  cnt, cnt, cnt);
	w("store %%LLNUM %%sbv.3.%d, %%LLNUM* %%B_REG\n", cnt);
	cnt++;
}


void emit_sal(intptr_t nof)
{
	static int sal_cnt = 0;
	w("%%sal.%d = load %%LLNUM* %%A_REG;    SAL %ld\n", sal_cnt, nof);
	w("%%sal.3.%d = sub %%LLNUM %%sal.%d, %ld\n",
	  sal_cnt, sal_cnt, nof);
	w("store %%LLNUM %%sal.3.%d, %%LLNUM* %%A_REG\n", sal_cnt);
	sal_cnt++;
}


void emit_sbl(intptr_t nof)
{
	static cnt = 0;
	w("%%sbl.b.%d = load %%LLNUM* %%B_REG\n", cnt);
	w("%%sbl.r.%d = sub %%LLNUM %%sbl.b.%d, %ld\n", cnt, cnt, nof);
	w("store %%LLNUM %%sbl.r.%d, %%LLNUM* %%B_REG\n", cnt);
	cnt++;
}


void emit_multl(intptr_t nof)
{
	static cnt = 0;
	w("%%mul.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("%%mul.r.%d = mul %%LLNUM %%mul.a.%d, %ld\n", cnt, cnt, nof);
	w("store %%LLNUM %%mul.r.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_bump(char *v, uintptr_t nof)
{
	static int cnt = 0;
	w("%%bump.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%bump.r.%d = add %%LLNUM %%bump.v.%d, %ld\n", cnt, cnt, nof);
	w("store %%LLNUM %%bump.r.%d, %%LLNUM* @%s\n", cnt, v);
	cnt++;
}


void emit_andv(char *v)
{
	static int cnt = 0;
	w("%%andv.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%andv.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("%%andv.r.%d = and %%LLNUM %%andv.v.%d, %%andv.a.%d\n",
	  cnt, cnt, cnt);
	w("store %%LLNUM %%andv.r.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_andl(uintptr_t n)
{
	static int cnt = 0;
	w("%%andl.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("%%andl.r.%d = and %%LLNUM %lu, %%andl.a.%d\n", cnt, n, cnt);
	w("store %%LLNUM %%andl.r.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_orl(uintptr_t n)
{
#ifdef LOWL_ML1
	static int cnt = 0;
	w("%%orl.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("%%orl.r.%d = or %%LLNUM %lu, %%orl.a.%d\n", cnt, n, cnt);
	w("store %%LLNUM %%orl.r.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif
}


void emit_cav(char *v)
{
	static int cnt = 0;
	w("%%cav.a.%d = load %%LLNUM* %%A_REG;\n", cnt);
	w("%%cav.v.%d = load %%LLNUM* @%s;\n", cnt, v);
	w("%%cav.cmp.%d = sub %%LLNUM %%cav.a.%d, %%cav.v.%d;\n", cnt,cnt,cnt);
	w("store %%LLNUM %%cav.cmp.%d, %%LLNUM* %%CMP\n", cnt);
	cnt++;
}


void emit_cal(intptr_t nof)
{
	static int cal_cnt = 0;
	w("%%cal.%d = load %%LLNUM* %%A_REG;   CAL %ld\n", cal_cnt, nof);
	w("%%cal_cmp.%d = sub %%LLNUM %%cal.%d, %ld\n", cal_cnt, cal_cnt, nof);
	w("store %%LLNUM %%cal_cmp.%d, %%LLNUM* %%CMP\n", cal_cnt);
	cal_cnt++;
}

void emit_ccn(char c)
{
	static int cnt = 0;
	w("%%ccl.c.%d = load i8* %%C_REG\n", cnt);
	w("%%ccl.cmp.%d = sub i8 %%ccl.c.%d , %d\n", cnt, cnt, c);
	w("%%ccl.r.%d = sext i8 %%ccl.cmp.%d to %%LLNUM\n", cnt, cnt);
	w("store %%LLNUM %%ccl.r.%d, %%LLNUM* %%CMP\n", cnt);
	cnt++;
}

void emit_ccl(char *s)
{
	emit_ccn(*s);
}


void emit_cai(char *v)
{
	static int cnt = 0;
	w("%%cai.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%cai.p.%d = inttoptr %%LLNUM %%cai.v.%d to %%LLNUM*\n", cnt, cnt);
	w("%%cai.r.%d = load %%LLNUM* %%cai.p.%d\n", cnt, cnt);
	w("%%cai.a.%d = load %%LLNUM* %%A_REG\n", cnt, cnt);
	w("%%cai.cmp.%d = sub %%LLNUM %%cai.a.%d, %%cai.r.%d\n", cnt, cnt, cnt);
	w("store %%LLNUM %%cai.cmp.%d, %%LLNUM* %%CMP\n", cnt);
	cnt++;
}

void emit_cci(char *v)
{
	static int cnt = 0;
	w("%%cci.v.%d = load %%LLNUM* @%s\n", cnt, v);
	w("%%cci.p.%d = inttoptr %%LLNUM %%cci.v.%d to i8*\n", cnt, cnt);
	w("%%cci.r.%d = load i8* %%cci.p.%d\n", cnt, cnt);
	w("%%cci.c.%d = load i8* %%C_REG\n", cnt);
	w("%%cci.s.%d = sub i8 %%cci.c.%d, %%cci.r.%d\n", cnt, cnt, cnt);
	w("%%cci.cmp.%d = zext i8 %%cci.s.%d to %%LLNUM\n", cnt, cnt);
	w("store %%LLNUM %%cci.cmp.%d\n, %%LLNUM* %%CMP\n", cnt);
	cnt++;
}

void emit_subr(char *v, int parnm, uintptr_t n)
{
	callgraph_addsubr(v, parnm, n, 0);
	w("br label %%%s\n", v); 
	w("%s:\n", v);
	if ( parnm ) {
		static int cnt = 0;
		w("%%subr.%d = load %%LLNUM* %%A_REG\n", cnt);
		w("store %%LLNUM %%subr.%d, %%LLNUM* @PARNM\n", cnt);
		cnt++;
	}
}


void emit_exit(uintptr_t n, char *sub)
{
	/* See comment before callgraph functions. */
	w("br label %%lowl_exit_%s_%ld;\n", sub, n);
}


void emit_linkr(char *v)
{
#ifdef LOWL_ML1
	if ( strcmp(v, "STKARG") ) {
		/* STKARG should be the only linkroutine
		 * supported needed by the ML/I MD module. */
		EMIT_PANIC("The only linkroutine should be STKARG!");
	}

	callgraph_addsubr(v, 0, 1, 1);
	w("br label %%%s\n", v);
	w("%s:\n", v);
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif
}


void emit_linkb()
{
#ifdef LOWL_ML1
	emit_exit(1, "STKARG");
#else
	EMIT_PANIC("LOWL mapper compiled without ML/I exentions.");
#endif
}


void emit_gosub(char *v)
{
	int linkroutine = 0;

	/*
	 * MD Functions.
	 * 
	 * We intercept MD functions call here, to avoid 
	 * following LOWL routines calling rules.

	 */
	if ( md_gosub(v) )
		return;

#ifdef LOWL_ML1
	/* Linkroutine.Do not save on stack. */
	if ( !strcmp(v, "STKARG") ) {
		linkroutine = 1;	
	}
#endif

	/*
 	 * MI routines.
	 */

	callgraph_add(v, emitter_pc);
	if ( linkroutine ) {
		/* Save next PC to LINKPT. */
		w("store %%LLNUM %ld, %%LLNUM* @LINKPT\n", emitter_pc);
	} else {
		/* Save next PC to stack. */
		w("call void @lowl_pushlink(%%LLNUM %ld)\n", emitter_pc);
	}
	/* Branch to subroutine label */
	w("br label %%%s\n;      GOSUB %s\n", v, v);
}


void emit_goadd(char *v)
{
#define SWSTM(_x) w("%%LLNUM " #_x ", label %%LOWL_LINE_%ld ", \
			 emitter_pc + _x + 1)
	static int cnt = 0;
	w("%%goadd.%d = load %%LLNUM* @%s;      GOADD %s\n", cnt, v, v);
	w("switch %%LLNUM %%goadd.%d, label %%goadd_jmperr [ ", cnt);
	SWSTM(0); SWSTM(1); SWSTM(2); SWSTM(3); SWSTM(4); SWSTM(5);
	SWSTM(6); SWSTM(7); SWSTM(8); SWSTM(9); SWSTM(10); SWSTM(11);
	SWSTM(12); SWSTM(13); SWSTM(14); SWSTM(15); SWSTM(16);
	w("] \n");
	cnt++;
#undef SWSTM
}


void emit_css()
{
	w("call void @lowl_clearlink()\n");
	w("br label %%LOWL_LINE_%ld\n", emitter_pc + 1);
}


void emit_go(char *lbl, intptr_t dist, char ex, char ctx)
{
	w("br label %%%s;    GO %s, %ld, %c, %c\n",
	  lbl, lbl, dist, ex, ctx);
}


void emit_goeq(char *lbl, intptr_t dist, char ex, char ctx)
{
	static int goeq_cnt = 0;
	w("%%goeq_cmp.%d = load %%LLNUM* %%CMP;\n", goeq_cnt);
	w("%%goeq.%d = icmp eq %%LLNUM %%goeq_cmp.%d, 0;"
	  "     GOEQ %s, %ld, %c, %c\n",
	  goeq_cnt, goeq_cnt, lbl, dist, ex, ctx);
	w("br i1 %%goeq.%d, label %%%s, label %%goeq_false.%d;\n",
	  goeq_cnt, lbl, goeq_cnt);
	w("goeq_false.%d:\n", goeq_cnt);
	goeq_cnt++;
}


void emit_gone(char *lbl, intptr_t dist, char ex, char ctx)
{
	static int gone_cnt = 0;
	w("%%gone_cmp.%d = load %%LLNUM* %%CMP;\n", gone_cnt);
	w("%%gone.%d = icmp ne %%LLNUM %%gone_cmp.%d, 0;"
	  "     GONE %s, %ld, %c, %c\n",
	  gone_cnt, gone_cnt, lbl, dist, ex, ctx);
	w("br i1 %%gone.%d, label %%%s, label %%gone_false.%d;\n",
	  gone_cnt, lbl, gone_cnt);
	w("gone_false.%d:\n", gone_cnt);
	gone_cnt++;
}


void emit_goge(char *lbl, intptr_t dist, char ex, char ctx)
{
	static int cnt = 0;
	w("%%goge_cmp.%d = load %%LLNUM* %%CMP;\n", cnt);
	w("%%goge.%d = icmp sge %%LLNUM %%goge_cmp.%d, 0;"
	  "    GOGE %s, %ld, %c, %c\n",
	  cnt, cnt, lbl, dist, ex, ctx);
	w("br i1 %%goge.%d, label %%%s, label %%goge_false.%d\n",
	  cnt, lbl, cnt);
	w("goge_false.%d:\n", cnt);
	cnt++;
}


void emit_gogr(char *lbl, intptr_t dist, char ex, char ctx)
{

	static int cnt = 0;
	w("%%gogr_cmp.%d = load %%LLNUM* %%CMP;\n", cnt);
	w("%%gogr.%d = icmp sgt %%LLNUM %%gogr_cmp.%d, 0;"
	  "    GOGR %s, %ld, %c, %c\n",
	  cnt, cnt, lbl, dist, ex, ctx);
	w("br i1 %%gogr.%d, label %%%s, label %%gogr_false.%d\n",
	  cnt, lbl, cnt);
	w("gogr_false.%d:\n", cnt);
	cnt++;
}


void emit_gole(char *lbl, intptr_t dist, char ex, char ctx)
{
	static int cnt = 0;
	w("%%gole_cmp.%d = load %%LLNUM* %%CMP;\n", cnt);
	w("%%gole.%d = icmp sle %%LLNUM %%gole_cmp.%d, 0;"
	  "    GOLE %s, %ld, %c, %c\n",
	  cnt, cnt, lbl, dist, ex, ctx);
	w("br i1 %%gole.%d, label %%%s, label %%gole_false.%d\n",
	  cnt, lbl, cnt);
	w("gole_false.%d:\n", cnt);
	cnt++;
}


void emit_golt(char *lbl, intptr_t dist, char ex, char ctx)
{
	static int cnt = 0;
	w("%%golt_cmp.%d = load %%LLNUM* %%CMP;\n", cnt);
	w("%%golt.%d = icmp slt %%LLNUM %%golt_cmp.%d, 0;"
	  "    GOLT %s, %ld, %c, %c\n",
	  cnt, cnt, lbl, dist, ex, ctx);
	w("br i1 %%golt.%d, label %%%s, label %%golt_false.%d\n",
	  cnt, lbl, cnt);
	w("golt_false.%d:\n", cnt);
	cnt++;
}


void emit_gopc(char *lbl)
{
	static int cnt = 0;
	w("%%gopc.c.%d = load i8* %%C_REG\n", cnt);
	w("%%gopc.r.%d = call i8 @lowl_punctuation(i8 %%gopc.c.%d)\n",
	  cnt, cnt);
	w("%%gopc.b.%d = icmp ne i8 %%gopc.r.%d, 0\n", cnt, cnt);
	w("br i1 %%gopc.b.%d, label %%%s, label %%gopc_false.%d\n",
	  cnt, lbl, cnt);
	w("gopc_false.%d:\n", cnt);
	cnt++;
}


void emit_gond(char *lbl)
{
	static int cnt = 0;
	w("%%gond.c.%d = load i8* %%C_REG\n", cnt);
	w("%%gond.r.%d = call i8 @lowl_digit(i8 %%gond.c.%d)\n", cnt, cnt);
	w("%%gond.b.%d = icmp eq i8 %%gond.r.%d, 0\n", cnt, cnt);
	w("br i1 %%gond.b.%d, label %%%s, label %%gond_false.%d\n",
	  cnt, lbl, cnt);
	w("gond_false.%d:\n", cnt);
	w("%%gond.n.%d = sub i8 %%gond.c.%d, 48\n", cnt, cnt);
	w("%%gond.a.%d = zext i8 %%gond.n.%d to %%LLNUM\n", cnt, cnt);
	w("store %%LLNUM %%gond.a.%d, %%LLNUM* %%A_REG\n", cnt);
	cnt++;
}


void emit_fstk()
{
	static int cnt = 0;
	w("%%fstk.v.%d = load %%LLNUM* @FFPT\n", cnt);
	w("%%fstk.p.%d = inttoptr %%LLNUM %%fstk.v.%d to %%LLNUM*\n", cnt, cnt);
	w("%%fstk.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("store %%LLNUM %%fstk.a.%d, %%LLNUM* %%fstk.p.%d\n", cnt, cnt);
	w("%%fstk.nv.%d = add %%LLNUM %%fstk.v.%d, %d\n",
	  cnt, cnt, LLVM_PTRSIZE/8);
	w("store %%LLNUM %%fstk.nv.%d, %%LLNUM* @FFPT\n", cnt);
	cnt++;
}


void emit_bstk()
{
	static int cnt = 0;
	w("%%bstk.cv.%d = load %%LLNUM* @LFPT\n", cnt);
	w("%%bstk.nv.%d = sub %%LLNUM %%bstk.cv.%d, %d\n",
	  cnt, cnt, LLVM_PTRSIZE/8);
	w("store %%LLNUM %%bstk.nv.%d, %%LLNUM* @LFPT\n", cnt);
	w("%%bstk.a.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("%%bstk.p.%d = inttoptr %%LLNUM %%bstk.nv.%d to %%LLNUM*\n",
	  cnt, cnt);
	w("store %%LLNUM %%bstk.a.%d, %%LLNUM* %%bstk.p.%d\n", cnt, cnt);

	cnt++;
}


void emit_cfstk()
{
	static int cnt = 0;
	w("%%cfstk.v.%d = load %%LLNUM* @FFPT\n", cnt);
	w("%%cfstk.p.%d = inttoptr %%LLNUM %%cfstk.v.%d to i8*\n", cnt, cnt);
	w("%%cfstk.c.%d = load i8* %%C_REG\n", cnt);
	w("store i8 %%cfstk.c.%d, i8* %%cfstk.p.%d\n", cnt, cnt);
	w("%%cfstk.np.%d = getelementptr i8* %%cfstk.p.%d, i32 1\n", cnt, cnt);
	w("%%cfstk.nv.%d = ptrtoint i8* %%cfstk.np.%d to %%LLNUM\n", cnt, cnt);
	w("store %%LLNUM %%cfstk.nv.%d, %%LLNUM* @FFPT\n", cnt);
	cnt++;
}


void emit_unstk(char *v)
{
	static int cnt = 0;
	w("%%unstk.v.%d = load %%LLNUM* @LFPT\n", cnt);
	w("%%unstk.p.%d = inttoptr %%LLNUM %%unstk.v.%d to %%LLNUM*\n",
	  cnt, cnt);
	w("%%unstk.val.%d = load %%LLNUM* %%unstk.p.%d\n", cnt, cnt);
	w("store %%LLNUM %%unstk.val.%d, %%LLNUM* @%s\n", cnt, v);
	w("%%unstk.nv.%d = add %%LLNUM %%unstk.v.%d, %d\n",
	  cnt, cnt, LLVM_PTRSIZE/8);
	w("store %%LLNUM %%unstk.nv.%d, %%LLNUM* @LFPT\n", cnt);
	cnt++;
}

void emit_fmove()
{
	/* We could use LLVM's memcpy intrinsic, but the name
	 * seems to change from version to version. */
	static int cnt = 0;
	w("%%fmove.len.%d = load %%LLNUM* %%A_REG\n", cnt);
	w("call void @lowl_fmove(%%LLNUM %%fmove.len.%d)\n", cnt);
	cnt++;
}


void emit_bmove()
{
	static int cnt = 0;
	w("%%bmove.len.%d = load %%LLNUM* %%A_REG;\n", cnt);
	w("call void @lowl_bmove(%%LLNUM %%bmove.len.%d)\n", cnt);
	cnt++;
}


void emit_mess(char *mess)
{
	int strid;

	/* Save string for later declaration. */
	strid = str_declare(mess);
	/* Get a pointer to the string with proper cast. */
	w("%%putsptr.%d = getelementptr [ %d x i8 ]* @STR%d, i64 0, i64 0\n",
	  strid, (int)strlen(mess) + 1, strid);
	/* And call the function. */
	w("call void @lowl_puts(i8* %%putsptr.%d)\n", strid);
}


void emit_nb(char *comment)
{
	w("; %s\n", comment);
}


void emit_prgst()
{
}


void emit_prgen()
{
}


void emit_align()
{
}


