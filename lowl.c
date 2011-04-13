#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

void LLOWL_main();

int
main()
{
	char *LOWL_stack = malloc(4096);
	LLOWL_main(LOWL_stack, LOWL_stack+4096);
}

void
lowl_goadd_jmperror(void)
{
	fprintf(stderr, "GOADD fail: variable too big.\n"
		"Please enlarge the switch instruction.\n");
	exit(-1);
}

void
lowl_exit_jmperror(void)
{
	fprintf(stderr, "EXIT fail: this is a serious BUG in callgraph.\n"
		"Please report.\n");
	exit(-1);
}

void
lowl_puts(char *str)
{
	char *ptr = str;
	while ( *ptr != '\0' ) {
		if ( *ptr == '$' )
			putchar('\n');
		else
			putchar(*ptr);
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
lowl_bmove(uint8_t *dst, uint8_t *src, lowlint_t len)
{
	int i;
	for ( i = (int)len - 1; i >= 0; i-- )
		*(dst + i) = *(src + i);
}


/*
 * SUBROUTINE STACK.
 */
/* The LLOWL manual says that a dozen would be sufficient.
 * Let's be generous. */
#define STACKSZ 24

uint32_t stack[STACKSZ];
int stackp = -1;

void
lowl_pushlink(uint32_t addr)
{
	if ( stackp == STACKSZ - 1 )
		panic("Stack overflow! Consider resizing.");
	stack[++stackp] = addr;
}

uint32_t
lowl_poplink(void)
{
	if ( stackp < 0 ) {
		panic("stack underflow!");
	}
	return stack[stackp--];
}
