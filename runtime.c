#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "lowl.h"

void lowl_main();

/* Standard stack size. */
#define LOWL_STACKSZ	10*1024
char *lowl_stack;


/*
 * LOWL runtime init/fini.
 */
void
lowl_runtime_init(void)
{
	lowl_stack = malloc(LOWL_STACKSZ);
}

void
lowl_runtime_fini(void)
{
	free(lowl_stack);
}


/*
 * Execute the LOWL program.
 */
void
lowl_run(void)
{
	lowl_main(lowl_stack, lowl_stack+(LOWL_STACKSZ));
}


/*
 * Error handling support functions.
 */
void
lowl_goadd_jmperror(void)
{
	fprintf(stderr, "GOADD fail: variable too big.\n"
		"Please increase the switch instruction table.\n");
	exit(-1);
}

void
lowl_exit_jmperror(void)
{
	fprintf(stderr, "EXIT fail: this is a serious BUG in callgraph.\n"
		"Please report.\n");
	exit(-1);
}


/*
 * LOWL instructions support functions.
 */

extern lowlint_t LOWLVAR(SRCPT);
extern lowlint_t LOWLVAR(DSTPT);

void
lowl_puts(char *str)
{
	char *ptr = str;
	while ( *ptr != '\0' ) {
		if ( *ptr == '$' )
			putc('\n', stderr);
		else
			putc(*ptr, stderr);
		ptr++;
	}
}

uint8_t
lowl_digit(uint8_t c)
{
	return ( c >= '0' && c <= '9' ) ? 1 : 0;
}

uint8_t
lowl_punctuation(uint8_t c)
{
	return ( (c >= 'A' && c <= 'Z') 
		|| (c >= 'a' && c <= 'z')
		|| (c >= '0' && c <= '9') ) ? 0 : 1;
}

void
lowl_bmove(lowlint_t len)
{
	int i;
	char *src = (char *)(uintptr_t)LOWLVAR(SRCPT);
	char *dst = (char *)(uintptr_t)LOWLVAR(DSTPT);
	for ( i = (int)len - 1; i >= 0; i-- )
		*(dst + i) = *(src + i);
}

void
lowl_fmove(lowlint_t len)
{
	char *src = (char *)(uintptr_t)LOWLVAR(SRCPT);
	char *dst = (char *)(uintptr_t)LOWLVAR(DSTPT);
	memcpy(dst, src, len);
}


/*
 * Subroutine Stack.
 */
#define STACKSZ 24	/* Lowl manual says 'a dozen' are sufficient. */

lowlint_t stack[STACKSZ];
int stackp = -1;

void
lowl_pushlink(lowlint_t addr)
{
	if ( stackp == STACKSZ - 1 ) {
		fprintf(stderr, "Subrouting stack exhausted! Increase STACKSZ in ml1-llvm sources.\n");
		exit(-1);
	}
	stack[++stackp] = addr;
}

lowlint_t
lowl_poplink(void)
{
	if ( stackp < 0 ) {
		fprintf(stderr, "Subroutine stack underflow! This is a serious BUG in ml1-llvm. Please report.\n");
		exit(-1);
	}
	return stack[stackp--];
}

void
lowl_clearlink(void)
{
	stackp = -1;
}

