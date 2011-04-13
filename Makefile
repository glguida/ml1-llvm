lowl-mapper: y.tab.c lex.yy.c emit_llvm.c
	$(CC) $(CFLAGS) -DLOWL_TEST -o $@ $^

y.tab.c: mapper.y 
	$(YACC) -d mapper.y

lex.yy.c: mapper.l y.tab.c
	$(LEX) mapper.l


clean:
	-rm lex.yy.c y.tab.c y.tab.h lowl-map-test
