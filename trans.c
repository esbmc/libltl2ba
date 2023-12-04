// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : trans.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include "ltl2ba.h"

extern int tl_verbose, tl_terse, tl_errs;
extern FILE	*tl_out;

#ifdef NXT
int
only_nxt(Node *n)
{
        switch (n->ntyp) {
        case NEXT:
                return 1;
        case OR:
        case AND:
                return only_nxt(n->rgt) && only_nxt(n->lft);
        default:
                return 0;
        }
}
#endif

int
dump_cond(Node *pp, Node *r, int first)
{       Node *q;
        int frst = first;

        if (!pp) return frst;

        q = dupnode(pp);
        q = rewrite(q);

        if (q->ntyp == PREDICATE
        ||  q->ntyp == NOT
#ifndef NXT
        ||  q->ntyp == OR
#endif
        ||  q->ntyp == FALSE)
        {       if (!frst) fprintf(tl_out, " && ");
                dump(q);
                frst = 0;
#ifdef NXT
        } else if (q->ntyp == OR)
        {       if (!frst) fprintf(tl_out, " && ");
                fprintf(tl_out, "((");
                frst = dump_cond(q->lft, r, 1);

                if (!frst)
                        fprintf(tl_out, ") || (");
                else
                {       if (only_nxt(q->lft))
                        {       fprintf(tl_out, "1))");
                                return 0;
                        }
                }

                frst = dump_cond(q->rgt, r, 1);

                if (frst)
                {       if (only_nxt(q->rgt))
                                fprintf(tl_out, "1");
                        else
                                fprintf(tl_out, "0");
                        frst = 0;
                }

                fprintf(tl_out, "))");
#endif
        } else  if (q->ntyp == V_OPER
                && !anywhere(AND, q->rgt, r))
        {       frst = dump_cond(q->rgt, r, frst);
        } else  if (q->ntyp == AND)
        {
                frst = dump_cond(q->lft, r, frst);
                frst = dump_cond(q->rgt, r, frst);
        }

        return frst;
}

void trans(Node *p)
{
  if (!p || tl_errs) return;

  if (tl_verbose || tl_terse) {
    FILE *f = tl_out;
    tl_out = stderr;
    fprintf(tl_out, "\t/* Normlzd: ");
    dump(p);
    fprintf(tl_out, " */\n");
    tl_out = f;
  }
  if (tl_terse)
    return;

  mk_alternating(p);
  mk_generalized();
  mk_buchi();
}

