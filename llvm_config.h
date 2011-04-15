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
 * One option would be to select the size as sizeof(uintptr_t),
 * but this would defeat the purpose of keeping the generated
 * LLVM assembler file portable.
 *
 * This define selects the pointer size to use. At the moment,
 * 64-bit (8 bytes) seems to be well supported by all LLVM
 * backends.
 */
#define LLVM_PTRSIZE	LOWL_REGSIZE

#if LOWL_REGSIZE == 64
#define PRIdLWI 	PRId64
typedef int64_t lowlint_t;
#elif LOWL_REGSIZE == 32
#define PRIdLWI		PRId32
typedef int32_t lowlint_t;
#endif

/*
 * Buffer size for lowlint_t itoa.
 * This must be >= Log10(max number) + 1 (sign) + 1 (null)
 */
#define LOWLINT_ITOA_LEN 24	/* Let's be overprotective. */

#endif /* _LLVM_CONFIG_H */
