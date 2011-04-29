#include <stdio.h>
#include <string.h>
#include "lowl.h"

/* This file is linked against both the lowl-mapper
 * and the final executable (runtime).
 * The __RUNTIME define controls which code the 
 * compiler will see. */
#ifdef __RUNTIME

int
main(int argc, char *argv[])
{
	/* Simply execute LOWL code. */
	lowl_runtime_init(0, stderr);
	lowl_run();
	lowl_runtime_fini();
	return 0;
}

/*
 * LOWLTEST MD support functions.
 */

void
mderch(uint8_t c)
{
	putc(c, stderr);
}

#else /* LOWL-MAPPER support functions. */

#include "emitter.h"

void
emitter_md_init(void)
{
	printf("\n\n;\n; MD declarations.\n;\n");
	printf("declare void @mderch(i8)\n");
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
		printf("ret void\n");
		return 1;
	} else if ( !strcmp(v, "MDERCH") ) {
		/*
		 * MDERCH.
		 */
		static int cnt = 0;
		printf("%%mderch.%d = load i8* %%C_REG;\n", cnt);
		printf("call void @mderch(i8 %%mderch.%d)\n", cnt);
		printf("br label %%LOWL_LINE_%ld\n", emitter_pc + 1);
		cnt++;
		return 1;
	};
	return 0;
}

#endif
