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
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

/* EMPTY_SET is passed to make_set() to create empty */
#define EMPTY_SET (-1)
#define SET_SIZE(elements) (elements/(8 * sizeof(int)) + 1)

typedef struct Symbol {
char		*name;
	struct Symbol	*next;	/* linked list, symbol table */
} Symbol;

typedef struct Node {
	short		ntyp;	/* node type */
	struct Symbol	*sym;
	struct Node	*lft;	/* tree */
	struct Node	*rgt;	/* tree */
	struct Node	*nxt;	/* if linked list */
} Node;

typedef struct Graph {
	Symbol		*name;
	Symbol		*incoming;
	Symbol		*outgoing;
	Symbol		*oldstring;
	Symbol		*nxtstring;
	Node		*New;
	Node		*Old;
	Node		*Other;
	Node		*Next;
	unsigned char	isred[64], isgrn[64];
	unsigned char	redcnt, grncnt;
	unsigned char	reachable;
	struct Graph	*nxt;
} Graph;

typedef struct Mapping {
	char	*from;
	Graph	*to;
	struct Mapping	*nxt;
} Mapping;

typedef struct ATrans {
  int *to;
  int *pos;
  int *neg;
  struct ATrans *nxt;
} ATrans;

typedef struct AProd {
  int astate;
  struct ATrans *prod;
  struct ATrans *trans;
  struct AProd *nxt;
  struct AProd *prv;
} AProd;


typedef struct GTrans {
  int *pos;
  int *neg;
  struct GState *to;
  int *final;
  struct GTrans *nxt;
} GTrans;

typedef struct GState {
  int id;
  int incoming;
  int *nodes_set;
  struct GTrans *trans;
  struct GState *nxt;
  struct GState *prv;
} GState;

typedef struct BTrans {
  struct BState *to;
  int *pos;
  int *neg;
  struct BTrans *nxt;
} BTrans;

typedef struct BState {
  struct GState *gstate;
  int id;
  int incoming;
  int final;
  struct BTrans *trans;
  struct BState *nxt;
  struct BState *prv;
  int label;  /* DAN */
} BState;

enum {
	ALWAYS=257,
	AND,		/* 258 */
	EQUIV,		/* 259 */
	EVENTUALLY,	/* 260 */
	FALSE,		/* 261 */
	IMPLIES,	/* 262 */
	NOT,		/* 263 */
	OR,		/* 264 */
	PREDICATE,	/* 265 */
	TRUE,		/* 266 */
	U_OPER,		/* 267 */
	V_OPER,		/* 268 */
	NEXT,		/* 269 */
};

#define ZN	(Node *)0
#define ZS	(Symbol *)0
#define Nhash	255
#define True	tl_nn(TRUE,  ZN, ZN)
#define False	tl_nn(FALSE, ZN, ZN)
#define Not(a)	push_negation(symtab, tl_nn(NOT, a, ZN))
#define rewrite(n)	canonical(symtab, right_linked(n))

#define Debug(x)	{ if (0) fprintf(stderr, x); }
#define Debug2(x,y)	{ if (tl_verbose) fprintf(stderr, x,y); }
#define Dump(x)		{ if (0) dump(x); }
#define Explain(x)	{ if (tl_verbose) tl_explain(x); }

#define Assert(x, y)	{ if (!(x)) { tl_explain(y); \
			  fatal(": assertion failed\n"); } }
#define min(x,y)        ((x<y)?x:y)

#undef TL_EMALLOC_VERBOSE

typedef Symbol *tl_Symtab[Nhash + 1];

typedef struct {
  int cexpr_idx;
  char *cexpr_expr_table[256];
} tl_Cexprtab;

typedef struct {
  Node *tl_yylval;
  int tl_yychar;
  char yytext[2048];
} tl_Lexer;

typedef enum {
	TL_STATS     = 1 << 0, /* time and size stats */
	TL_SIMP_LOG  = 1 << 1, /* logical simplification */
	TL_SIMP_DIFF = 1 << 2, /* automata simplification */
	TL_SIMP_FLY  = 1 << 3, /* on the fly simplification */
	TL_SIMP_SCC  = 1 << 4, /* use scc simplification */
	TL_FJTOFJ    = 1 << 5, /* 2eme fj */
	TL_VERBOSE   = 1 << 6,
} tl_Flags;

Node	*Canonical(tl_Symtab symtab, Node *);
Node	*canonical(tl_Symtab symtab, Node *);
Node	*cached(tl_Symtab symtab, Node *);
Node	*dupnode(const Node *);
Node	*getnode(const Node *);
Node	*in_cache(Node *);
Node	*push_negation(tl_Symtab symtab, Node *);
Node	*right_linked(Node *);
Node	*tl_nn(int, Node *, Node *);

Symbol	*tl_lookup(tl_Symtab symtab, const char *);

char	*emalloc(int);

int	anywhere(int, Node *, Node *);
int	dump_cond(Node *, Node *, int);
int	isequal(const Node *, const Node *);
int	tl_Getchar(void);

void	*tl_emalloc(int);
ATrans  *emalloc_atrans();
void    free_atrans(ATrans *, int);
void    free_all_atrans();
GTrans  *emalloc_gtrans();
void    free_gtrans(GTrans *, GTrans *, int);
BTrans  *emalloc_btrans();
void    free_btrans(BTrans *, BTrans *, int);
void	a_stats(void);
void	addtrans(Graph *, char *, Node *, char *);
void	cache_stats(void);
void	dump(const Node *);
void	fatal(const char *);
void	fsm_print(void);
void	releasenode(int, Node *);
void	tfree(void *);
void	tl_explain(int);
void	tl_UnGetchar(void);
Node *	tl_parse(tl_Symtab symtab, tl_Cexprtab *cexpr, tl_Flags flags);
void	tl_yyerror(tl_Lexer *lex, char *);

typedef struct {
  ATrans **transition;
  int *final_set;
  int node_id; /* really the number of nodes */
  int sym_id; /* number of symbols */
  const char **sym_table;
} Alternating;

typedef struct {
  GState *gstates, **init;
  int init_size, gstate_id, *final, scc_size;
} Generalized;

typedef struct {
  BState *bstates;
  int accept;
} Buchi;

Alternating mk_alternating(const Node *, const tl_Cexprtab *cexpr, tl_Flags flags);
Generalized mk_generalized(const Alternating *, tl_Flags flags);
Buchi       mk_buchi(Generalized *g, tl_Flags);

void print_c_buchi(const Buchi *b, const char *const *sym_table,
                   const tl_Cexprtab *cexpr, int sym_id,
                   const char *c_sym_name_prefix);
void print_dot_buchi(const Buchi *b, const char *const *sym_table, const tl_Cexprtab *cexpr);
void print_spin_buchi(const Buchi *b, const char **sym_table);

ATrans *dup_trans(const ATrans *);
ATrans *merge_trans(const ATrans *, const ATrans *);
void do_merge_trans(ATrans **, const ATrans *, const ATrans *);

int  *new_set(int);
int  *clear_set(int *, int);
int  *make_set(int , int);
void copy_set(int *, int *, int);
int  *dup_set(int *, int);
void merge_sets(int *, int *, int);
void do_merge_sets(int *, int *, int *, int);
int  *intersect_sets(int *, int *, int);
void add_set(int *, int);
void rem_set(int *, int);
void spin_print_set(const char *const *sym_table, int *, int*);
void dot_print_set(const char *const *sym_table, const tl_Cexprtab *cexpr, int *, int*, int);
void print_set(int *, int);
int  empty_set(int *, int);
int  empty_intersect_sets(int *, int *, int);
int  same_set(int *l1, int *l2, int size);
int  same_sets(int *, int *, int);
int  included_set(int *, int *, int);
int  in_set(int *, int);
int  *list_set(int *, int);

void print_sym_set(const char *const *sym_table, const tl_Cexprtab *cexpr, int *l, int size);

int timeval_subtract (struct timeval *, struct timeval *, struct timeval *);

void put_uform(void);
