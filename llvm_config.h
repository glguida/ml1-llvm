#ifndef _LLVM_CONFIG_H
#define _LLVM_CONFIG_H

#include <stdint.h>

/*
 * LLVM pointer aliasing rules prevent us from using  GEP to
 * transform pointers to integers, do some operations with
 * these values, re-cast it and dereference it.
 * This unfortunately means that we need to use inttoptr and
 * ptrtoint, which require an explicit integer size.
 *
 * One option would be to select the pointer size to use to
 * a safe maximum (64-bit), hoping that the LLVM target will
 * be able to support those operations.
 *
 * What we do here instead is to set the pointer size as
 * sizeof(uintptr_t), even if it makes the LLVM assembler
 * generated architecture specific. Optionally, you can 
 * set the value to 32 or 64 bit at compilation time. See
 * comment in Makefile.
 */
#ifdef LOWL_REGSIZE
#define LLVM_PTRSIZE	LOWL_REGSIZE
#else
#define LLVM_PTRSIZE	((int)sizeof(uintptr_t)*8)
#endif

#ifdef LOWL_REGSIZE

#if LOWL_REGSIZE == 64
#define PRIdLWI 	PRId64
typedef int64_t lowlint_t;
#elif LOWL_REGSIZE == 32
#define PRIdLWI		PRId32
typedef int32_t lowlint_t;
#else
#error "LOWL_REGSIZE set to an unsupported value (32 or 64)
#endif /* LOWL_REGSIZE */

#else /* !LOWL_REGSIZE */

#define PRIdLWI		PRIdPTR
typedef intptr_t lowlint_t;

#endif /* !LOWL_REGSIZE */


/*
 * Buffer size for lowlint_t itoa.
 * This must be >= Log10(max number) + 1 (sign) + 1 (null)
 */
#define LOWLINT_ITOA_LEN 24	/* Let's be overprotective. */

#endif /* _LLVM_CONFIG_H */
