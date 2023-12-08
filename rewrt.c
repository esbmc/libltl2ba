// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : rewrt.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include "ltl2ba.h"

static Node	*can = LTL2BA_ZN;

static char	dumpbuf[2048];

static void
sdump(Node *n)
{
	switch (n->ntyp) {
	case PREDICATE:	strcat(dumpbuf, n->sym->name);
			break;
	case U_OPER:	strcat(dumpbuf, "U");
			goto common2;
	case V_OPER:	strcat(dumpbuf, "V");
			goto common2;
	case OR:	strcat(dumpbuf, "|");
			goto common2;
	case AND:	strcat(dumpbuf, "&");
common2:		sdump(n->rgt);
common1:		sdump(n->lft);
			break;
	case NEXT:	strcat(dumpbuf, "X");
			goto common1;
	case NOT:	strcat(dumpbuf, "!");
			goto common1;
	case TRUE:	strcat(dumpbuf, "T");
			break;
	case FALSE:	strcat(dumpbuf, "F");
			break;
	default:	strcat(dumpbuf, "?");
			break;
	}
}

static Symbol *
DoDump(tl_Symtab symtab, Node *n)
{
	if (!n) return LTL2BA_ZS;

	if (n->ntyp == PREDICATE)
		return n->sym;

	dumpbuf[0] = '\0';
	sdump(n);
	return tl_lookup(symtab, dumpbuf);
}

Node *
right_linked(Node *n)
{
	if (!n) return n;

	if (n->ntyp == AND || n->ntyp == OR)
		while (n->lft && n->lft->ntyp == n->ntyp)
		{	Node *tmp = n->lft;
			n->lft = tmp->rgt;
			tmp->rgt = n;
			n = tmp;
		}

	n->lft = right_linked(n->lft);
	n->rgt = right_linked(n->rgt);

	return n;
}

Node *
canonical(tl_Symtab symtab, Node *n)
{	Node *m;	/* assumes input is right_linked */

	if (!n) return n;
	if ((m = in_cache(n)))
		return m;

	n->rgt = canonical(symtab, n->rgt);
	n->lft = canonical(symtab, n->lft);

	return cached(symtab, n);
}

Node *
push_negation(tl_Symtab symtab, Node *n)
{	Node *m;

	LTL2BA_Assert(n->ntyp == NOT, n->ntyp);

	switch (n->lft->ntyp) {
	case TRUE:
		releasenode(0, n->lft);
		n->lft = LTL2BA_ZN;
		n->ntyp = FALSE;
		break;
	case FALSE:
		releasenode(0, n->lft);
		n->lft = LTL2BA_ZN;
		n->ntyp = TRUE;
		break;
	case NOT:
		m = n->lft->lft;
		releasenode(0, n->lft);
		n->lft = LTL2BA_ZN;
		releasenode(0, n);
		n = m;
		break;
	case V_OPER:
		n->ntyp = U_OPER;
		goto same;
	case U_OPER:
		n->ntyp = V_OPER;
		goto same;
	case NEXT:
		n->ntyp = NEXT;
		n->lft->ntyp = NOT;
		n->lft = push_negation(symtab, n->lft);
		break;
	case  AND:
		n->ntyp = OR;
		goto same;
	case  OR:
		n->ntyp = AND;

same:		m = n->lft->rgt;
		n->lft->rgt = LTL2BA_ZN;

		n->rgt = LTL2BA_Not(m);
		n->lft->ntyp = NOT;
		m = n->lft;
		n->lft = push_negation(symtab, m);
		break;
	}

	return LTL2BA_rewrite(n);
}

static void addcan(tl_Symtab symtab, int tok, Node *n)
{
	Node	*m, *prev = LTL2BA_ZN;
	Node	**ptr;
	Node	*N;
	Symbol	*s, *t; int cmp;

	if (!n) return;

	if (n->ntyp == tok)
	{
		addcan(symtab, tok, n->rgt);
		addcan(symtab, tok, n->lft);
		return;
	}
#if 0
	if ((tok == AND && n->ntyp == TRUE)
	||  (tok == OR  && n->ntyp == FALSE))
		return;
#endif
	N = dupnode(n);
	if (!can)
	{
		can = N;
		return;
	}

	s = DoDump(symtab, N);
	if (can->ntyp != tok)	/* only one element in list so far */
	{
		ptr = &can;
		goto insert;
	}

	/* there are at least 2 elements in list */
	prev = LTL2BA_ZN;
	for (m = can; m->ntyp == tok && m->rgt; prev = m, m = m->rgt)
	{
		t = DoDump(symtab, m->lft);
		cmp = strcmp(s->name, t->name);
		if (cmp == 0)	/* duplicate */
			return;
		if (cmp < 0)
		{
			if (!prev)
			{
				can = tl_nn(tok, N, can);
				return;
			} else
			{
				ptr = &(prev->rgt);
				goto insert;
	}	}	}

	/* new entry goes at the end of the list */
	ptr = &(prev->rgt);
insert:
	t = DoDump(symtab, *ptr);
	cmp = strcmp(s->name, t->name);
	if (cmp == 0)	/* duplicate */
		return;
	if (cmp < 0)
		*ptr = tl_nn(tok, N, *ptr);
	else
		*ptr = tl_nn(tok, *ptr, N);
}

static void
marknode(int tok, Node *m)
{
	if (m->ntyp != tok)
	{	releasenode(0, m->rgt);
		m->rgt = LTL2BA_ZN;
	}
	m->ntyp = -1;
}

Node * Canonical(tl_Symtab symtab, Node *n)
{
	Node *m, *p, *k1, *k2, *prev, *dflt = LTL2BA_ZN;
	int tok;

	if (!n) return NULL;

	tok = n->ntyp;
	if (tok != AND && tok != OR)
		return n;

	can = LTL2BA_ZN;
	addcan(symtab, tok, n);
#if 1
	LTL2BA_Debug("\nA0: "); LTL2BA_Dump(can);
	LTL2BA_Debug("\nA1: "); LTL2BA_Dump(n); LTL2BA_Debug("\n");
#endif
	releasenode(1, n);

	/* mark redundant nodes */
	if (tok == AND)
	{	for (m = can; m; m = (m->ntyp == AND) ? m->rgt : LTL2BA_ZN)
		{	k1 = (m->ntyp == AND) ? m->lft : m;
			if (k1->ntyp == TRUE)
			{	marknode(AND, m);
				dflt = LTL2BA_True;
				continue;
			}
			if (k1->ntyp == FALSE)
			{	releasenode(1, can);
				can = LTL2BA_False;
				goto out;
		}	}
		for (m = can; m; m = (m->ntyp == AND) ? m->rgt : LTL2BA_ZN)
		for (p = can; p; p = (p->ntyp == AND) ? p->rgt : LTL2BA_ZN)
		{	if (p == m
			||  p->ntyp == -1
			||  m->ntyp == -1)
				continue;
			k1 = (m->ntyp == AND) ? m->lft : m;
			k2 = (p->ntyp == AND) ? p->lft : p;

			if (isequal(k1, k2))
			{	marknode(AND, p);
				continue;
			}
			if (anywhere(OR, k1, k2))
			{	marknode(AND, p);
				continue;
			}
			if (k2->ntyp == U_OPER
			&&  anywhere(AND, k2->rgt, can))
			{	marknode(AND, p);
				continue;
			}	/* q && (p U q) = q */
	}	}
	if (tok == OR)
	{	for (m = can; m; m = (m->ntyp == OR) ? m->rgt : LTL2BA_ZN)
		{	k1 = (m->ntyp == OR) ? m->lft : m;
			if (k1->ntyp == FALSE)
			{	marknode(OR, m);
				dflt = LTL2BA_False;
				continue;
			}
			if (k1->ntyp == TRUE)
			{	releasenode(1, can);
				can = LTL2BA_True;
				goto out;
		}	}
		for (m = can; m; m = (m->ntyp == OR) ? m->rgt : LTL2BA_ZN)
		for (p = can; p; p = (p->ntyp == OR) ? p->rgt : LTL2BA_ZN)
		{	if (p == m
			||  p->ntyp == -1
			||  m->ntyp == -1)
				continue;
			k1 = (m->ntyp == OR) ? m->lft : m;
			k2 = (p->ntyp == OR) ? p->lft : p;

			if (isequal(k1, k2))
			{	marknode(OR, p);
				continue;
			}
			if (anywhere(AND, k1, k2))
			{	marknode(OR, p);
				continue;
			}
			if (k2->ntyp == V_OPER
			&&  k2->lft->ntyp == FALSE
			&&  anywhere(AND, k2->rgt, can))
			{	marknode(OR, p);
				continue;
			}	/* p || (F V p) = p */
	}	}
	for (m = can, prev = LTL2BA_ZN; m; )	/* remove marked nodes */
	{	if (m->ntyp == -1)
		{	k2 = m->rgt;
			releasenode(0, m);
			if (!prev)
			{	m = can = can->rgt;
			} else
			{	m = prev->rgt = k2;
				/* if deleted the last node in a chain */
				if (!prev->rgt && prev->lft
				&&  (prev->ntyp == AND || prev->ntyp == OR))
				{	k1 = prev->lft;
					prev->ntyp = prev->lft->ntyp;
					prev->sym = prev->lft->sym;
					prev->rgt = prev->lft->rgt;
					prev->lft = prev->lft->lft;
					releasenode(0, k1);
				}
			}
			continue;
		}
		prev = m;
		m = m->rgt;
	}
out:
#if 1
	LTL2BA_Debug("A2: "); LTL2BA_Dump(can); LTL2BA_Debug("\n");
#endif
	if (!can)
	{	if (!dflt)
			fatal("cannot happen, Canonical");
		return dflt;
	}

	return can;
}
