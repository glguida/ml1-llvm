# This variable defines the location of ML/I LOWL sources.
ML1SRC?= ml1.lwl

# The LOWL_REGSIZE variable controls the bisize of LOWL register.
#
# The default is automatically set to the size of the pointer in current
# machine. To override this behaviour, you can set the environment variable
# LOWL_REGSIZE to 32 or 64 to specify the register size.
ifdef LOWL_REGSIZE
CPPFLAGS+= -DLOWL_REGSIZE=$(LOWL_REGSIZE)
endif

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
