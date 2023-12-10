// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : cache.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include "internal.h"

typedef struct Cache {
	Node *before;
	Node *after;
	int same;
	struct Cache *nxt;
} Cache;

static Cache	*stored = (Cache *) 0;
static unsigned long	Caches, CacheHits;

static int ismatch(const Node *, const Node *);
static int sameform(const Node *, const Node *);

void cache_dump(void)
{
	Cache *d;
	int nr=0;

	fprintf(stderr, "\nCACHE DUMP:\n");
	for (d = stored; d; d = d->nxt, nr++)
	{
		if (d->same) continue;
		fprintf(stderr, "B%3d: ", nr); dump(stderr, d->before); fprintf(stderr, "\n");
		fprintf(stderr, "A%3d: ", nr); dump(stderr, d->after); fprintf(stderr, "\n");
	}
	fprintf(stderr, "============\n");
}

Node * in_cache(Node *n)
{
	Cache *d;
	int nr=0;

	for (d = stored; d; d = d->nxt, nr++)
		if (isequal(d->before, n))
		{
			CacheHits++;
			if (d->same && ismatch(n, d->before)) return n;
			return dupnode(d->after);
		}
	return NULL;
}

Node * cached(Symtab symtab, Node *n)
{
	Cache *d;
	Node *m;

	if (!n) return n;
	if ((m = in_cache(n)))
		return m;

	Caches++;
	d = (Cache *) tl_emalloc(sizeof(Cache));
	d->before = dupnode(n);
	d->after  = Canonical(symtab, n); /* n is released */

	if (ismatch(d->before, d->after))
	{	d->same = 1;
		releasenode(1, d->after);
		d->after = d->before;
	}
	d->nxt = stored;
	stored = d;
	return dupnode(d->after);
}

void
cache_stats(void)
{
	fprintf(stderr, "cache stores     : %9ld\n", Caches);
	fprintf(stderr, "cache hits       : %9ld\n", CacheHits);
}

void
releasenode(int all_levels, Node *n)
{
	if (!n) return;

	if (all_levels)
	{	releasenode(1, n->lft);
		n->lft = NULL;
		releasenode(1, n->rgt);
		n->rgt = NULL;
	}
	tfree((void *) n);
}

Node *
tl_nn(int t, Node *ll, Node *rl)
{	Node *n = (Node *) tl_emalloc(sizeof(Node));

	n->ntyp = (short) t;
	n->lft  = ll;
	n->rgt  = rl;
	n->nxt  = NULL;

	return n;
}

Node * getnode(const Node *p)
{
	Node *n;

	if (!p)
		return NULL;

	n = tl_emalloc(sizeof(Node));
	n->ntyp = p->ntyp;
	n->sym  = p->sym; /* same name */
	n->lft  = p->lft;
	n->rgt  = p->rgt;

	return n;
}

Node * dupnode(const Node *n)
{
	Node *d;

	if (!n)
		return NULL;
	d = getnode(n);
	d->lft = dupnode(n->lft);
	d->rgt = dupnode(n->rgt);
	return d;
}

int one_lft(int ntyp, const Node *x, const Node *in)
{
	if (!x)  return 1;
	if (!in) return 0;

	if (sameform(x, in))
		return 1;

	if (in->ntyp != ntyp)
		return 0;

	if (one_lft(ntyp, x, in->lft))
		return 1;

	return one_lft(ntyp, x, in->rgt);
}

int all_lfts(int ntyp, const Node *from, const Node *in)
{
	if (!from) return 1;

	if (from->ntyp != ntyp)
		return one_lft(ntyp, from, in);

	if (!one_lft(ntyp, from->lft, in))
		return 0;

	return all_lfts(ntyp, from->rgt, in);
}

int sametrees(int ntyp, const Node *a, const Node *b)
{	/* toplevel is an AND or OR */
	/* both trees are right-linked, but the leafs */
	/* can be in different places in the two trees */

	if (!all_lfts(ntyp, a, b))
		return 0;

	return all_lfts(ntyp, b, a);
}

/* a better isequal() */
static int sameform(const Node *a, const Node *b)
{
	if (!a && !b) return 1;
	if (!a || !b) return 0;
	if (a->ntyp != b->ntyp) return 0;

	if (a->sym
	&&  b->sym
	&&  strcmp(a->sym->name, b->sym->name) != 0)
		return 0;

	switch (a->ntyp) {
	case TRUE:
	case FALSE:
		return 1;
	case PREDICATE:
		if (!a->sym || !b->sym) fatal("sameform...");
		return !strcmp(a->sym->name, b->sym->name);

	case NOT:
	case NEXT:
		return sameform(a->lft, b->lft);

	case U_OPER:
	case V_OPER:
		if (!sameform(a->lft, b->lft))
			return 0;
		if (!sameform(a->rgt, b->rgt))
			return 0;
		return 1;

	case AND:
	case OR:	/* the hard case */
		return sametrees(a->ntyp, a, b);

	default:
		fprintf(stderr, "type: %d\n", a->ntyp);
		fatal("cannot happen, sameform");
	}

	return 0;
}

int isequal(const Node *a, const Node *b)
{
	if (!a && !b)
		return 1;

	if (!a || !b)
	{	if (!a)
		{	if (b->ntyp == TRUE)
				return 1;
		} else
		{	if (a->ntyp == TRUE)
				return 1;
		}
		return 0;
	}
	if (a->ntyp != b->ntyp)
		return 0;

	if (a->sym
	&&  b->sym
	&&  strcmp(a->sym->name, b->sym->name) != 0)
		return 0;

	if (isequal(a->lft, b->lft)
	&&  isequal(a->rgt, b->rgt))
		return 1;

	return sameform(a, b);
}

static int ismatch(const Node *a, const Node *b)
{
	if (!a && !b) return 1;
	if (!a || !b) return 0;
	if (a->ntyp != b->ntyp) return 0;

	if (a->sym
	&&  b->sym
	&&  strcmp(a->sym->name, b->sym->name) != 0)
		return 0;

	if (ismatch(a->lft, b->lft)
	&&  ismatch(a->rgt, b->rgt))
		return 1;

	return 0;
}

int
any_term(Node *srch, Node *in)
{
	if (!in) return 0;

	if (in->ntyp == AND)
		return	any_term(srch, in->lft) ||
			any_term(srch, in->rgt);

	return isequal(in, srch);
}

int
any_and(Node *srch, Node *in)
{
	if (!in) return 0;

	if (srch->ntyp == AND)
		return	any_and(srch->lft, in) &&
			any_and(srch->rgt, in);

	return any_term(srch, in);
}

int
any_lor(Node *srch, Node *in)
{
	if (!in) return 0;

	if (in->ntyp == OR)
		return	any_lor(srch, in->lft) ||
			any_lor(srch, in->rgt);

	return isequal(in, srch);
}

int
anywhere(int tok, Node *srch, Node *in)
{
	if (!in) return 0;

	switch (tok) {
	case AND:	return any_and(srch, in);
	case  OR:	return any_lor(srch, in);
	case   0:	return any_term(srch, in);
	}
	fatal("cannot happen, anywhere");
	return 0;
}
