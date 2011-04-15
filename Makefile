# This variable defines the location of ML/I LOWL sources.
ML1SRC?= ml1.lwl

# This variable defines the bisize of LOWL register.
#
# It should be big enough to include a pointer, and the default
# setting (64 bit) will work also in 32 bit machines.
# If you are on a 32 bit architecture and want to save some
# memory, add argument LOWL_REGSIZE=32 to 'make'.
# See comment in llvm_config.h if interested in LLVM internals.
LOWL_REGSIZE?= 64

CPPFLAGS+= -DLOWL_REGSIZE=$(LOWL_REGSIZE)

ml1: runtime.c ml1.c ml1_hash.c ml1.llvm.s
	$(CC) $(CPPFLAGS) $(CFLAGS) runtime.c ml1.c ml1_hash.c  ml1.llvm.s -o $@

%.llvm.s: %.bc
	llc $(LLC_OPTS) -f $^ -o $@

%.bc: %.llvm
	llvm-as $^ -o - | opt -std-compile-opts -o $@

ml1.llvm: ml1-mapper $(ML1SRC)
	./ml1-mapper < $(ML1SRC) > ml1.llvm

ml1.lwl:

ml1-mapper: y.tab.c lex.yy.c emitter.c ml1_emitter.c ml1_hash.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DLOWL_ML1 -o $@ $^

y.tab.c: mapper.y 
	$(YACC) -d mapper.y

lex.yy.c: mapper.l y.tab.c
	$(LEX) mapper.l

clean:
	-rm lex.yy.c y.tab.c y.tab.h ml1-mapper *.llvm *.bc *.llvm.s
