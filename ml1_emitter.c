#include <stdio.h>
#include <string.h>
#include "emitter.h"
#include "lowl.h"
#define w(...) printf(__VA_ARGS__)


void
emitter_md_init(void)
{
	w("\n\n;\n; MD declarations.\n;\n"); 
	w("declare void @mderch(i8)\n");
	w("declare void @mdconv()\n");
	w("declare void @mdfind()\n");
	w("declare void @mdouch(i8)\n");
	w("declare i8 @mdread(i8*)\n");
	w("declare i8 @mdop()\n");
}

void
emitter_md_fini(void)
{
}

/*
 * Intercept calls to MD routines in GOSUB.
 * Return non-zero if it is an MD function, zero otherwhise.
 * If the function has to return, be sure to add a branch to
 * the next instruction.
 */
int
md_gosub(char *v)
{
	if ( !strcmp(v, "MDQUIT") ) {
		/*
		 * MDQUIT: Just exit the LOWL_main function.
		 */
		w("ret void\n");
		return 1;
	} else if ( !strcmp(v, "MDERCH") ) {
		/*
		 * MDERCH.
		 */
		static int cnt = 0;
		w("%%mderch.%d = load i8* %%C_REG;\n", cnt);
		w("call void @mderch(i8 %%mderch.%d)\n", cnt);
		w("br label %%LOWL_LINE_%ld\n", emitter_pc + 1);
		cnt++;
		return 1;
	}
	if ( !strcmp(v, "MDCONV") ) {
		/*
		 * MDCONV.
		 */
		w("call void @mdconv()\n");
		w("br label %%LOWL_LINE_%ld\n", emitter_pc + 1);
		return 1;
	} else if ( !strcmp(v, "MDFIND") ) {
		/*
		 * MDFIND.
		 */
		w("call void @mdfind()\n");
		w("br label %%LOWL_LINE_%ld\n", emitter_pc + 1 );
		return 1;
	} else if ( !strcmp(v, "MDOP") ) {
		/*
		 * MDOP: Call mdop helper function and simulate EXIT.
		 */
		static int cnt = 0;
		w("%%mdop.r.%d = call i8 @mdop()\n", cnt);
		w("%%mdop.c.%d = icmp eq i8 %%mdop.r.%d, 0\n", cnt, cnt);
		/* If Overflow EXIT 1, else EXIT 2 */
		w("br i1 %%mdop.c.%d, "
			"label %%LOWL_LINE_%ld, label %%LOWL_LINE_%ld\n", 
		  cnt, emitter_pc + 1, emitter_pc + 2);
		cnt++;
		return 1;
	} else if ( !strcmp(v, "MDOUCH") ) {
		/*
		 * MDOUCH.
		 */
		static int cnt = 0;
		w("%%mdouch.%d = load i8* %%C_REG;\n", cnt);
		w("call void @mdouch(i8 %%mdouch.%d)\n", cnt);
		w("br label %%LOWL_LINE_%ld\n", emitter_pc + 1);
		cnt++;
		return 1;
	} else if ( !strcmp(v, "MDREAD") ) {
		/*
		 * MDREAD.
		 */
		static int cnt = 0;
		w("%%mdread.r.%d = call i8 @mdread(i8* %%C_REG)\n", cnt);
		w("%%mdread.c.%d = icmp eq i8 %%mdread.r.%d, 2\n", cnt, cnt);
		w("br i1 %%mdread.c.%d, "
			"label %%LOWL_LINE_%ld, label %%LOWL_LINE_%ld\n",
		  cnt, emitter_pc + 2, emitter_pc +1);
		cnt++;
		return 1;
	}
	return 0;
}
