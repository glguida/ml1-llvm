#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "lowl.h"

#define ML1_VERSION "LOWL-to-LLVM version " LOWL_VERSION " (AJB)"

#define SVARS_NO 22
lowlint_t ml1_svars[SVARS_NO + 1];
#define SVAR(_x) ml1_svars[SVARS_NO - (_x)]

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
	/* Initial S10 value: 1 */
	SVAR(10) = 1;

	/* Initial S21 value: 1 */
	SVAR(21) = 1;

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

/*
 * Main function and argument parsing.
 */

#define MAX_OUF 4
#define MAX_INF 5
int oufs = 0;
FILE *output[MAX_OUF];
int infs = 0;
FILE *input[MAX_INF];
FILE *debug = NULL;
size_t wspace = 0;
int opt_v = 0;

void
version(void)
{
	fprintf(stderr, "ML/I: %s\n", ML1_VERSION);
}

void
usage(char *name)
{
	version();
	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "\t%s [-v] [-w workspace] [-o outpufile]+ "
		"[-d debugfile] [file ...]\n\n", name);
	exit(-1);
}

static FILE *
get_file(char *file, char *mode)
{
	FILE *f;

	f = fopen(file, mode);
	if ( f == NULL ) {
		perror(file);
		exit(-1);
	}
	return f;
}

static void
add_ofile(char *file)
{
	if ( oufs > MAX_OUF - 1 ) {
		fprintf(stderr, "Too many output files.\n");
		exit(-1);
	}
	if ( !strcmp(file, "-") ) {
		output[oufs++] = stdout;
		return;
	}
	output[oufs++] = get_file(file, "w");
}

static void
add_ifile(char *file)
{
	if ( infs > MAX_INF - 1 ) {
		fprintf(stderr, "Too many input files.\n");
		exit(-1);
	}
	if ( !strcmp(file, "-") ) {
		input[infs++] = stdin;
		return;
	}
	input[infs++] = get_file(file, "r");
}

void
arg_parse(int argc, char *argv[])
{
	int argno = 0;
#define next_arg() (++argno < argc )
	while ( next_arg() ) {
		if ( !strcmp(argv[argno], "-") )
			add_ifile(argv[argno]);
		else if ( *argv[argno] == '-' ) {
			if ( !strcmp(argv[argno], "-v") )
				opt_v = 1;
			else if ( !strcmp(argv[argno], "-w") 
				  && wspace == 0 
				  && next_arg() )
					wspace = strtoul(argv[argno], NULL, 0);
			else if ( !strcmp(argv[argno], "-d") 
				  && debug == stderr 
				  && next_arg() )
					debug = get_file(argv[argno], "w");
			else if ( !strcmp(argv[argno], "-o") 
				  && next_arg() )
					add_ofile(argv[argno]);
			else usage(argv[0]);
		} else add_ifile(argv[argno]);
	}
}

int
main(int argc, char *argv[])
{
	/* Initialize standard I/O files. */
	debug = stderr;
	output[0] = stdout;

	/* Parse arguments. */
	arg_parse(argc, argv);

	/* If no input has been specified, set infs to 1, as we're
	 * going to use stdin as input. */
	if ( infs == 0 ) {
		infs++;
		input[0] = stdin;
	}

	if ( opt_v )
		version();

	/* Initialize ML/I LOWL. */
	ml1_init();

	/* Initialize LOWL runtime. */
	lowl_runtime_init(wspace, debug);

	/* Run ML/I LOWL code. */
	lowl_run();

	/* Exit now. */
	lowl_runtime_fini();
	ml1_fini();

	return 0;
}


/*
 * ML/I MD runtime support functions.
 */


void
mderch(uint8_t c)
{
	putc(c, debug);
}


void
mdouch(uint8_t c)
{
	lowlint_t ouflags = SVAR(21);
	if ( ouflags & 1 )
		putc(c, output[0]);
	if ( (ouflags & 2) || (SVAR(22) != 0) )
		if ( output[1] != NULL )
			putc(c, output[1]);
	if ( ouflags & 4)
		if ( output[2] != NULL )
			putc(c, output[2]);
	if ( ouflags & 8)
		if ( output[3] != NULL )
			putc(c, output[3]);
}


uint8_t
mdread(uint8_t *c)
{
	int r;
	int inno = SVAR(10);
	if ( inno == 0 )
		return 1;
	if ( inno > infs ) {
		fprintf(debug, "S10 has illegal value, viz %d\n", inno);
		exit(-2);
	}
	r = getc(input[inno - 1]);
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

	n = ml1_hash((char *)LOWLVAR(IDPT), LOWLVAR(IDLEN));
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
