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

