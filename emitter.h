#include <stdint.h>

extern long emitter_pc;

void emitter_init(void);
void emitter_fini(void);
void emitter_md_init(void);
void emitter_md_fini(void);
int  md_gosub(char *);
void oom(void);

#ifdef LOWL_ML1
void emit_hash(char *str);
void emit_thash();
void emit_rl(char *str, intptr_t nof);
void emit_linkr(char *v);
void emit_linkb();
#endif /* LOWL_ML1 */


void emit_newpc(int stp);
void emit_eol();
void emit_table_label(char *lbl);
void emit_label(char *lbl);
void emit_dcl(char *var);
void emit_equ(char *arg1, char *arg2);
void emit_ident(char *arg1, intptr_t num);
void emit_con(uintptr_t num);
void emit_nch(char c);
void emit_str(char *str);
void emit_lav(char *v, char rx);
void emit_lbv(char *v);
void emit_lal(intptr_t nof);
void emit_lcn(char cn);
void emit_lam(intptr_t nof);
void emit_lcm(intptr_t nof);
void emit_lai(char *v, char rx);
void emit_lci(char *v, char rx);
void emit_laa(char *v, char dc);
void emit_stv(char *v, char px);
void emit_sti(char *v, char px);
void emit_clear(char *v);
void emit_aav(char *v);
void emit_abv(char *v);
void emit_aal(intptr_t nof);
void emit_sav(char *v);
void emit_sbv(char *v);
void emit_sal(intptr_t nof);
void emit_sbl(intptr_t nof);
void emit_multl(intptr_t nof);
void emit_bump(char *v, uintptr_t nof);
void emit_andv(char *v);
void emit_andl(uintptr_t n);
void emit_orl(uintptr_t n);
void emit_cav(char *v);
void emit_cal(intptr_t nof);
void emit_ccn(char c);
void emit_ccl(char *s);
void emit_cai(char *v, char ax);
void emit_cci(char *v);
void emit_subr(char *v, int parnm, uintptr_t n);
void emit_exit(uintptr_t n, char *sub);
void emit_gosub(char *v, intptr_t dist);
void emit_goadd(char *v);
void emit_css();
void emit_go(char *lbl, intptr_t dist, char ex, char ctx);
void emit_goeq(char *lbl, intptr_t dist, char ex, char ctx);
void emit_gone(char *lbl, intptr_t dist, char ex, char ctx);
void emit_goge(char *lbl, intptr_t dist, char ex, char ctx);
void emit_gogr(char *lbl, intptr_t dist, char ex, char ctx);
void emit_gole(char *lbl, intptr_t dist, char ex, char ctx);
void emit_golt(char *lbl, intptr_t dist, char ex, char ctx);
void emit_gopc(char *lbl, intptr_t dist, char ex, char ctx);
void emit_gond(char *lbl, intptr_t dist, char ex, char ctx);
void emit_fstk(void);
void emit_bstk();
void emit_cfstk();
void emit_unstk(char *v);
void emit_fmove(void);
void emit_bmove(void);
void emit_mess(char *mess);
void emit_nb(char *comment);
void emit_prgst(char *prog);
void emit_prgen(void);
void emit_align(void);
