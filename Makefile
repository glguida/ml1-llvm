lowl-map-test: y.tab.c lex.yy.c emit_llvm.c
	$(CC) $(CFLAGS) -DLOWL_TEST -o $@ $^

y.tab.c: lowl.y 
	$(YACC) -d lowl.y

lex.yy.c: lowl.l y.tab.c
	$(LEX) lowl.l


clean:
	-rm lex.yy.c y.tab.c y.tab.h lowl-map-test
