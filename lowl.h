#ifndef _LOWL_H
#define _LOWL_H
#include <inttypes.h>
#include "llvm_config.h"

/* Macro to access LOWL defined variables,
 * in case we implement a namespace for
 * them in the future. */
#define LOWLVAR(_x) _x

#define LCH_VAL 	1
#define LICH_VAL 	1
#define LNM_VAL		(LLVM_PTRSIZE/8)

#ifdef LOWL_ML1
#include "ml1.h"

#define WTHS_VAL	((lowlint_t)~0)
#define LHV_VAL		(ML1_HASHSZ * (LLVM_PTRSIZE/8))
#endif

void lowl_runtime_init(void);
void lowl_runtime_fini(void);
void lowl_run(void);

#endif /* _LOWL_H */
