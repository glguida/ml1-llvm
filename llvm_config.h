#ifndef _LLVM_CONFIG_H
#define _LLVM_CONFIG_H

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
#define LLVM_PTRSIZE	64

#endif
