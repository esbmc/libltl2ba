// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : ltl2ba.h *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#pragma once

#include <stdio.h>
#include <stdlib.h>

#define LTL2BA_VERSION_MAJOR 2
#define LTL2BA_VERSION_MINOR 1

/* LTL2BA_EMPTY_SET is passed to make_set() to create empty */
#define LTL2BA_EMPTY_SET   (-1)
#define LTL2BA_SET_SIZE(n) (n / (8 * sizeof(int)) + 1)
#define LTL2BA_Nhash       255

#ifdef __cplusplus
extern "C" {
#endif

const char * ltl2ba_version(void);

typedef struct ltl2ba_Symbol {
	char *name;
	struct ltl2ba_Symbol *next; /* linked list, symbol table */
} ltl2ba_Symbol;

typedef struct ltl2ba_Node {
	short ntyp;                /* node type */
	struct ltl2ba_Symbol *sym;
	struct ltl2ba_Node *lft;   /* tree */
	struct ltl2ba_Node *rgt;   /* tree */
	struct ltl2ba_Node *nxt;   /* if linked list (only used by parser) */
} ltl2ba_Node;

typedef struct ltl2ba_ATrans {
	int *to;
	int *pos;
	int *neg;
	struct ltl2ba_ATrans *nxt;
} ltl2ba_ATrans;

typedef struct ltl2ba_AProd {
	int astate;
	struct ltl2ba_ATrans *prod;
	struct ltl2ba_ATrans *trans;
	struct ltl2ba_AProd *nxt;
	struct ltl2ba_AProd *prv;
} ltl2ba_AProd;

typedef struct ltl2ba_GTrans {
	int *pos;
	int *neg;
	struct ltl2ba_GState *to;
	int *final;
	struct ltl2ba_GTrans *nxt;
} ltl2ba_GTrans;

typedef struct ltl2ba_GState {
	int id;
	int incoming;
	int *nodes_set;
	struct ltl2ba_GTrans *trans;
	struct ltl2ba_GState *nxt;
	struct ltl2ba_GState *prv;
} ltl2ba_GState;

typedef struct ltl2ba_BTrans {
	struct ltl2ba_BState *to;
	int *pos;
	int *neg;
	struct ltl2ba_BTrans *nxt;
} ltl2ba_BTrans;

typedef struct ltl2ba_BState {
	struct ltl2ba_GState *gstate;
	int id;
	int incoming;
	int final;
	struct ltl2ba_BTrans *trans;
	struct ltl2ba_BState *nxt;
	struct ltl2ba_BState *prv;
	int label;  /* DAN */
} ltl2ba_BState;

enum {
	LTL2BA_ALWAYS = 257,
	LTL2BA_AND,        /* 258 */
	LTL2BA_EQUIV,      /* 259 */
	LTL2BA_EVENTUALLY, /* 260 */
	LTL2BA_FALSE,      /* 261 */
	LTL2BA_IMPLIES,    /* 262 */
	LTL2BA_NOT,        /* 263 */
	LTL2BA_OR,         /* 264 */
	LTL2BA_PREDICATE,  /* 265 */
	LTL2BA_TRUE,       /* 266 */
	LTL2BA_U_OPER,     /* 267 */
	LTL2BA_V_OPER,     /* 268 */
	LTL2BA_NEXT,       /* 269 */
};

typedef ltl2ba_Symbol *ltl2ba_Symtab[LTL2BA_Nhash + 1];

typedef struct {
	int cexpr_idx;
	char *cexpr_expr_table[256];
} ltl2ba_Cexprtab;

typedef struct {
	ltl2ba_Node *tl_yylval;
	int tl_yychar;
	char yytext[2048];
} ltl2ba_Lexer;

typedef enum {
	LTL2BA_STATS     = 1 << 0, /* time and size stats */
	LTL2BA_SIMP_LOG  = 1 << 1, /* logical simplification */
	LTL2BA_SIMP_DIFF = 1 << 2, /* automata simplification */
	LTL2BA_SIMP_FLY  = 1 << 3, /* on the fly simplification */
	LTL2BA_SIMP_SCC  = 1 << 4, /* use scc simplification */
	LTL2BA_FJTOFJ    = 1 << 5, /* 2eme fj */
	LTL2BA_VERBOSE   = 1 << 6,
} ltl2ba_Flags;

typedef struct {
	int sym_size;  /* LTL2BA_SET_SIZE() of an upper bound on the number of
	                  predicates */
	int node_size; /* LTL2BA_SET_SIZE() of the number of states */
} ltl2ba_set_sizes;

typedef struct {
	ltl2ba_ATrans **transition;
	int *final_set;
	int node_id; /* really the number of nodes */
	int sym_id;  /* number of symbols */
	const char **sym_table;
	ltl2ba_set_sizes sz;
} ltl2ba_Alternating;

typedef struct {
	ltl2ba_GState *gstates, **init;
	int init_size, gstate_id, *final, scc_size;
	ltl2ba_set_sizes sz; /* copy from Alternating automaton */
} ltl2ba_Generalized;

typedef struct {
	ltl2ba_BState *bstates;
	int accept;
	ltl2ba_set_sizes sz; /* copy from Generalized automaton */
} ltl2ba_Buchi;

ltl2ba_Node *  Canonical(ltl2ba_Symtab symtab, ltl2ba_Node *);
ltl2ba_Node *  canonical(ltl2ba_Symtab symtab, ltl2ba_Node *);
ltl2ba_Node *  cached(ltl2ba_Symtab symtab, ltl2ba_Node *);
ltl2ba_Node *  dupnode(const ltl2ba_Node *);
ltl2ba_Node *  in_cache(ltl2ba_Node *);
ltl2ba_Node *  push_negation(ltl2ba_Symtab symtab, ltl2ba_Node *);
ltl2ba_Node *  right_linked(ltl2ba_Node *);
ltl2ba_Node *  tl_nn(int, ltl2ba_Node *, ltl2ba_Node *);

ltl2ba_Symbol *tl_lookup(ltl2ba_Symtab symtab, const char *);

int            isequal(const ltl2ba_Node *, const ltl2ba_Node *);

void           a_stats(void);
void           cache_stats(void);
void           cache_dump(void);

void *         tl_emalloc(int);
ltl2ba_ATrans *emalloc_atrans(int sym_size, int node_size);
void           free_atrans(ltl2ba_ATrans *, int);
void           free_all_atrans();
ltl2ba_GTrans *emalloc_gtrans(int sym_size, int node_size);
void           free_gtrans(ltl2ba_GTrans *, ltl2ba_GTrans *, int);
ltl2ba_BTrans *emalloc_btrans(int sym_size);
void           free_btrans(ltl2ba_BTrans *, ltl2ba_BTrans *, int);
void           releasenode(int, ltl2ba_Node *);
void           tfree(void *);

ltl2ba_Node *  tl_parse(ltl2ba_Symtab symtab, ltl2ba_Cexprtab *cexpr,
                        ltl2ba_Flags flags);

ltl2ba_Alternating mk_alternating(const ltl2ba_Node *, FILE *,
                                  const ltl2ba_Cexprtab *cexpr,
                                  ltl2ba_Flags flags);
ltl2ba_Generalized mk_generalized(const ltl2ba_Alternating *, FILE *,
                                  ltl2ba_Flags flags,
                                  const ltl2ba_Cexprtab *cexpr);
ltl2ba_Buchi mk_buchi(ltl2ba_Generalized *g, FILE *, ltl2ba_Flags,
                      const char *const *sym_table,
                      const ltl2ba_Cexprtab *cexpr);

void print_c_buchi(FILE *f, const ltl2ba_Buchi *b, const char *const *sym_table,
                   const ltl2ba_Cexprtab *cexpr, int sym_id,
                   const char *c_sym_name_prefix, const char *extern_header,
                   const char *cmdline);
void print_dot_buchi(FILE *f, const ltl2ba_Buchi *b,
                     const char *const *sym_table,
                     const ltl2ba_Cexprtab *cexpr);
void print_spin_buchi(FILE *f, const ltl2ba_Buchi *b, const char **sym_table);

ltl2ba_ATrans *merge_trans(const ltl2ba_set_sizes *sz, const ltl2ba_ATrans *,
                           const ltl2ba_ATrans *);
void do_merge_trans(const ltl2ba_set_sizes *sz, ltl2ba_ATrans **,
                    const ltl2ba_ATrans *, const ltl2ba_ATrans *);

int *new_set(int);
int *clear_set(int *, int);
int *make_set(int, int);
void copy_set(int *, int *, int);
int *dup_set(int *, int);
void do_merge_sets(int *, int *, int *, int);
int *intersect_sets(int *, int *, int);
void add_set(int *, int);
void rem_set(int *, int);
void spin_print_set(FILE *, const char *const *sym_table, int *, int *,
                    int sym_size);
void dot_print_set(FILE *, const char *const *sym_table,
                   const ltl2ba_Cexprtab *cexpr, int *, int *, int sym_size,
                   int);
void print_set(FILE *, int *, int);
int  empty_set(int *, int);
int  empty_intersect_sets(int *, int *, int);
int  same_sets(int *, int *, int);
int  included_set(int *, int *, int);
int  in_set(int *, int);
int *list_set(int *, int);

void print_sym_set(FILE *f, const char *const *sym_table,
                   const ltl2ba_Cexprtab *cexpr, int *l, int size);

/* implemented by driver (e.g. main.c) */
void  dump(FILE *, const ltl2ba_Node *);
char *emalloc(int);
void  fatal(const char *);
void  put_uform(FILE *);
void  tl_explain(int);
int   tl_Getchar(void);
void  tl_UnGetchar(void);
void  tl_yyerror(ltl2ba_Lexer *lex, char *);

#ifdef __cplusplus
}
#endif
