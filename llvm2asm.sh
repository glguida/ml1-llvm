#!/bin/sh
# Usage:
# 	llvm2asm <LLVM source>

llvm-as $1 -o - | opt -std-compile-opts - | llc -f -o $1.s
