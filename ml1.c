#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include "lowl.h"

#define SVARS_NO 9
lowlint_t ml1_svars[SVARS_NO + 1];

extern lowlint_t LOWLVAR(OPSW);
extern lowlint_t LOWLVAR(OP1);
extern lowlint_t LOWLVAR(MEVAL);
extern lowlint_t LOWLVAR(IDPT);
extern lowlint_t LOWLVAR(IDLEN);
extern lowlint_t LOWLVAR(HASHPT);
extern lowlint_t LOWLVAR(HTABPT);
extern lowlint_t LOWLVAR(SVARPT);

void
ml1_init(void)
{
	/* ML/I MD Spec: Last element must
	 * contains the numbers of SVARS. */
	ml1_svars[SVARS_NO] = SVARS_NO;

	/* ML/I MD Spec: Save last element address to SVARPT */
	LOWLVAR(SVARPT) = (lowlint_t)(uintptr_t)(ml1_svars + SVARS_NO);
}

void
ml1_fini(void)
{
}

int
main(int argc, char *argv)
{
	/* Initialize LOWL runtime. */
	lowl_runtime_init();

	/* Initialize ML/I vars. */
	ml1_init();

	/* Run ML/I LOWL code. */
	lowl_run();

	/* Exit now. */
	lowl_runtime_fini();
	ml1_fini();
}


/*
 * ML/I MD runtime support functions.
 */


void
mderch(uint8_t c)
{
	putc(c, stderr);
}


void
mdouch(uint8_t c)
{
	putc(c, stdout);
}


uint8_t
mdread(uint8_t *c)
{
	int r;
	r = getchar();
	if ( r == EOF ) {
		return 1;
	}
	*c = r;
	return 2;
}


void
mdconv(void)
{
	static char buf[LOWLINT_ITOA_LEN];
	LOWLVAR(IDLEN) = snprintf(buf, LOWLINT_ITOA_LEN, "%"PRIdLWI, 
				  LOWLVAR(MEVAL));
	LOWLVAR(IDPT) = (uintptr_t)buf;
}


void
mdfind(void)
{
	lowlint_t n;

	n = ml1_hash(LOWLVAR(IDPT), LOWLVAR(IDLEN));
	LOWLVAR(HTABPT) = LOWLVAR(HASHPT) + n * (LLVM_PTRSIZE/8);
}


uint8_t
mdop()
{
	lowlint_t op1 = LOWLVAR(OP1);
	lowlint_t meval = LOWLVAR(MEVAL);

	switch ( LOWLVAR(OPSW) ) {
	case 1:	/* Multiplication. */
		meval = op1 * meval;
		LOWLVAR(MEVAL) = meval;
		return 1;
		break;
	default: /* Division. */
		{
			/* Divide rounding to the lowest number. 
			   Use ANSI C's integer division (that rounds
			   to zero) and fix the result if needed. */
			ldiv_t res;
			int sign = 0; /* Zero is positive. */

			if ( meval == 0 )
				return 0; /* Overflow. */
			/* Calculate the sign of the result. */
			if ( meval < 0 ) sign = !sign;
			if ( op1 < 0 ) sign = !sign;

			res = ldiv(op1, meval);
			if ( sign && (res.rem != 0) ) {
				/* Fix it. */
				res.quot -= 1;
			}			
			LOWLVAR(MEVAL) = res.quot;
			return 1;
		}
		break;
	}
}
