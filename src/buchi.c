// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : buchi.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */

#include "internal.h"

#undef min
#define min(x,y)        ((x<y)?x:y)

/********************************************************************\
|*              Structures and shared variables                     *|
\********************************************************************/

typedef struct BScc {
  BState *bstate;
  int rank;
  int theta;
  struct BScc *nxt;
} BScc;

struct bdfs_state {
  int rank;
  BScc *scc_stack;
};

struct bcounts {
  int bstate_count, btrans_count;
};

/* Record of what states stutter-accept, according to each input symbol. */
struct accept_sets {
  int **stutter_accept_table;
  int *optimistic_accept_state_set;
  int *pessimistic_accept_state_set;
};

typedef struct Slist {
  int * set;
  struct Slist * nxt; } Slist;

struct pess_data {
  int state_count;
  int state_size;
  int *full_state_set;
  Slist **tr;
};

/********************************************************************\
|*        Simplification of the generalized Buchi automaton         *|
\********************************************************************/

static void free_bstate(BState *s) /* frees a state and its transitions */
{
  free_btrans(s->trans->nxt, s->trans, 1);
  tfree(s);
}

/* removes a state */
static BState *remove_bstate(BState *s, BState *s1, BState *const bremoved)
{
  BState *prv = s->prv;
  s->prv->nxt = s->nxt;
  s->nxt->prv = s->prv;
  free_btrans(s->trans->nxt, s->trans, 0);
  s->trans = (BTrans *)0;
  s->nxt = bremoved->nxt;
  bremoved->nxt = s;
  s->prv = s1;
  for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
    if(s1->prv == s)
      s1->prv = s->prv;
  return prv;
}

static void copy_btrans(const set_sizes *sz, BTrans *from, BTrans *to) {
  to->to    = from->to;
  copy_set(from->pos, to->pos, sz->sym_size);
  copy_set(from->neg, to->neg, sz->sym_size);
}

/* simplifies the transitions */
static int simplify_btrans(Buchi *b, FILE *f, Flags flags)
{
  BState *s;
  BTrans *t, *t1;
  int changed = 0;
  struct rusage tr_debut, tr_fin;
  struct timeval t_diff;

  if(flags & LTL2BA_STATS) getrusage(RUSAGE_SELF, &tr_debut);

  for (s = b->bstates->nxt; s != b->bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans;) {
      t1 = s->trans->nxt;
      copy_btrans(&b->sz, t, s->trans);
      while((t == t1) || (t->to != t1->to) ||
            !included_set(t1->pos, t->pos, b->sz.sym_size) ||
            !included_set(t1->neg, t->neg, b->sz.sym_size))
        t1 = t1->nxt;
      if(t1 != s->trans) {
        BTrans *free = t->nxt;
        t->to    = free->to;
        copy_set(free->pos, t->pos, b->sz.sym_size);
        copy_set(free->neg, t->neg, b->sz.sym_size);
        t->nxt   = free->nxt;
        if(free == s->trans) s->trans = t;
        free_btrans(free, 0, 0);
        changed++;
      }
      else
        t = t->nxt;
    }

  if(flags & LTL2BA_STATS) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(f, "\nSimplification of the Buchi automaton - transitions: %ld.%06lis",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(f, "\n%i transitions removed\n", changed);

  }
  return changed;
}

/* returns 1 if the transitions are identical */
static int same_btrans(const set_sizes *sz, BTrans *s, BTrans *t)
{
  return((s->to == t->to) &&
	 same_sets(s->pos, t->pos, sz->sym_size) &&
	 same_sets(s->neg, t->neg, sz->sym_size));
}

/* redirects transitions before removing a state from the automaton */
static void remove_btrans(Buchi *b, BState *to)
{
  BState *s;
  BTrans *t;
  for (s = b->bstates->nxt; s != b->bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans; t = t->nxt)
      if (t->to == to) { /* transition to a state with no transitions */
	BTrans *free = t->nxt;
	t->to = free->to;
	copy_set(free->pos, t->pos, b->sz.sym_size);
	copy_set(free->neg, t->neg, b->sz.sym_size);
	t->nxt   = free->nxt;
	if(free == s->trans) s->trans = t;
	free_btrans(free, 0, 0);
      }
}

/* redirects transitions before removing a state from the automaton */
static void retarget_all_btrans(Buchi *b, BState *const bremoved)
{
  BState *s;
  BTrans *t;
  for (s = b->bstates->nxt; s != b->bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans; t = t->nxt)
      if (!t->to->trans) { /* t->to has been removed */
	t->to = t->to->prv;
	if(!t->to) { /* t->to has no transitions */
	  BTrans *free = t->nxt;
	  t->to = free->to;
	  copy_set(free->pos, t->pos, b->sz.sym_size);
	  copy_set(free->neg, t->neg, b->sz.sym_size);
	  t->nxt   = free->nxt;
	  if(free == s->trans) s->trans = t;
	  free_btrans(free, 0, 0);
	}
      }
  while(bremoved->nxt != bremoved) { /* clean the 'removed' list */
    s = bremoved->nxt;
    bremoved->nxt = bremoved->nxt->nxt;
    tfree(s);
  }
}

/* decides if the states are equivalent */
static int all_btrans_match(Buchi *buchi, BState *a, BState *b)
{
  BTrans *s, *t;

  /* the states have to be both final or both non final,
   * or at least one of them has to be in a trivial SCC
   * (incoming == -1), as the acceptance condition of
   * such a state can be modified without changing the
   * language of the automaton
   */
  if (((a->final == buchi->accept) || (b->final == buchi->accept)) &&
      (a->final + b->final != 2 * buchi->accept)  /* final condition of a and b differs */
      && a->incoming >=0   /* a is not in a trivial SCC */
      && b->incoming >=0)  /* b is not in a trivial SCC */
    return 0;  /* states can not be matched */

  for (s = a->trans->nxt; s != a->trans; s = s->nxt) {
                                /* all transitions from a appear in b */
    copy_btrans(&buchi->sz, s, b->trans);
    t = b->trans->nxt;
    while(!same_btrans(&buchi->sz, s, t))
      t = t->nxt;
    if(t == b->trans) return 0;
  }
  for (s = b->trans->nxt; s != b->trans; s = s->nxt) {
                                /* all transitions from b appear in a */
    copy_btrans(&buchi->sz, s, a->trans);
    t = a->trans->nxt;
    while(!same_btrans(&buchi->sz, s, t))
      t = t->nxt;
    if(t == a->trans) return 0;
  }
  return 1;
}

/* eliminates redundant states */
static int simplify_bstates(Buchi *b, FILE *f, Flags flags, int *gstate_id,
                            BState *const bremoved)
{
  BState *s, *s1, *s2;
  int changed = 0;
  struct rusage tr_debut, tr_fin;
  struct timeval t_diff;

  if(flags & LTL2BA_STATS) getrusage(RUSAGE_SELF, &tr_debut);

  for (s = b->bstates->nxt; s != b->bstates; s = s->nxt) {
    if(s->trans == s->trans->nxt) { /* s has no transitions */
      s = remove_bstate(s, (BState *)0, bremoved);
      changed++;
      continue;
    }
    b->bstates->trans = s->trans;
    b->bstates->final = s->final;
    s1 = s->nxt;
    while(!all_btrans_match(b, s, s1))
      s1 = s1->nxt;
    if(s1 != b->bstates) { /* s and s1 are equivalent */
      /* we now want to remove s and replace it by s1 */
      if(s1->incoming == -1) {  /* s1 is in a trivial SCC */
        s1->final = s->final; /* change the final condition of s1 to that of s */

        /* We may have to update the SCC status of s1
         * stored in s1->incoming, because we will retarget the incoming
         * transitions of s to s1.
         *
         * If both s1 and s are in trivial SCC, then retargeting
         * the incoming transitions does not change the status of s1,
         * it remains in a trivial SCC.
         *
         * If s1 was in a trivial SCC, but s was not, then
         * s1 has to have a transition to s that corresponds to a
         * self-loop of s (as both states have the same outgoing transitions).
         * But then, s1 will not remain a trivial SCC after retargeting.
         * In particular, afterwards the final condition of s1 may not be
         * changed anymore.
         *
         * If both s1 and s are in non-trivial SCC, merging does not
         * change the SCC status of s1.
         *
         * If we are here, s1->incoming==1 and thus s1 forms a trivial SCC.
         * We therefore can set the status of s1 to that of s,
         * which correctly handles the first two cases above.
         */
        s1->incoming = s->incoming;
      }
      s = remove_bstate(s, s1, bremoved);
      changed++;
    }
  }
  retarget_all_btrans(b, bremoved);

  /*
   * As merging equivalent states can change the 'final' attribute of
   * the remaining state, it is possible that now there are two
   * different states with the same id and final values.
   * This would lead to multiply-defined labels in the generated neverclaim.
   * We iterate over all states and assign new ids (previously unassigned)
   * to these states to disambiguate.
   * Fix from ltl3ba.
   */
  for (s = b->bstates->nxt; s != b->bstates; s = s->nxt) { /* For all states s*/
    for (s2 = s->nxt; s2 != b->bstates; s2 = s2->nxt) {    /* and states s2 to the right of s */
      if(s->final == s2->final && s->id == s2->id) {       /* if final and id match */
        s->id = ++*gstate_id;                              /* disambiguate by assigning unused id */
      }
    }
  }

  if(flags & LTL2BA_STATS) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(f, "\nSimplification of the Buchi automaton - states: %ld.%06lis",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(f, "\n%i states removed\n", changed);
  }

  return changed;
}

static int bdfs(BState *s, struct bdfs_state *st) {
  BTrans *t;
  BScc *c;
  BScc *scc = (BScc *)tl_emalloc(sizeof(BScc));
  scc->bstate = s;
  scc->rank = st->rank;
  scc->theta = st->rank++;
  scc->nxt = st->scc_stack;
  st->scc_stack = scc;

  s->incoming = 1;

  for (t = s->trans->nxt; t != s->trans; t = t->nxt) {
    if (t->to->incoming == 0) {
      int result = bdfs(t->to, st);
      scc->theta = min(scc->theta, result);
    }
    else {
      for(c = st->scc_stack->nxt; c != 0; c = c->nxt)
	if(c->bstate == t->to) {
	  scc->theta = min(scc->theta, c->rank);
	  break;
	}
    }
  }
  if(scc->rank == scc->theta) {
    if(st->scc_stack == scc) { /* s is alone in a scc */
      s->incoming = -1;
      for (t = s->trans->nxt; t != s->trans; t = t->nxt)
	if (t->to == s)
	  s->incoming = 1;
    }
    st->scc_stack = scc->nxt;
  }
  return scc->theta;
}


static void simplify_bscc(Buchi *b, BState *const bremoved) {
  BState *s;
  struct bdfs_state st;
  st.rank = 1;
  st.scc_stack = NULL;

  if(b->bstates == b->bstates->nxt) return;

  for(s = b->bstates->nxt; s != b->bstates; s = s->nxt)
    s->incoming = 0; /* state color = white */

  bdfs(b->bstates->prv, &st);

  for(s = b->bstates->nxt; s != b->bstates; s = s->nxt)
    if(s->incoming == 0)
      remove_bstate(s, 0, bremoved);
}




/********************************************************************\
|*              Generation of the Buchi automaton                   *|
\********************************************************************/

/* finds the corresponding state, or creates it */
static BState *find_bstate(Buchi *b, GState **state, int final, BState *s,
                           BState *const bstack, BState *const bremoved)
{
  if((s->gstate == *state) && (s->final == final)) return s; /* same state */

  s = bstack->nxt; /* in the stack */
  bstack->gstate = *state;
  bstack->final = final;
  while(!(s->gstate == *state) || !(s->final == final))
    s = s->nxt;
  if(s != bstack) return s;

  s = b->bstates->nxt; /* in the solved states */
  b->bstates->gstate = *state;
  b->bstates->final = final;
  while(!(s->gstate == *state) || !(s->final == final))
    s = s->nxt;
  if(s != b->bstates) return s;

  s = bremoved->nxt; /* in the removed states */
  bremoved->gstate = *state;
  bremoved->final = final;
  while(!(s->gstate == *state) || !(s->final == final))
    s = s->nxt;
  if(s != bremoved) return s;

  s = (BState *)tl_emalloc(sizeof(BState)); /* creates a new state */
  s->gstate = *state;
  s->id = (*state)->id;
  s->incoming = 0;
  s->final = final;
  s->trans = emalloc_btrans(b->sz.sym_size); /* sentinel */
  s->trans->nxt = s->trans;
  s->nxt = bstack->nxt;
  bstack->nxt = s;
  return s;
}

static int next_final(Buchi *b, int *set, int fin, const int *final) /* computes the 'final' value */
{
  if((fin != b->accept) && in_set(set, final[fin + 1]))
    return next_final(b, set, fin + 1, final);
  return fin;
}

/* creates all the transitions from a state */
static void make_btrans(Buchi *b, BState *s, const int *final, Flags flags,
                        struct bcounts *c, BState *const bstack,
                        BState *const bremoved)
{
  int state_trans = 0;
  GTrans *t;
  BTrans *t1;
  BState *s1;
  if(s->gstate->trans)
    for(t = s->gstate->trans->nxt; t != s->gstate->trans; t = t->nxt) {
      int fin = next_final(b, t->final, (s->final == b->accept) ? 0 : s->final, final);
      BState *to = find_bstate(b, &t->to, fin, s, bstack, bremoved);

      for(t1 = s->trans->nxt; t1 != s->trans;) {
	if((flags & LTL2BA_SIMP_FLY) &&
	   (to == t1->to) &&
	   included_set(t->pos, t1->pos, b->sz.sym_size) &&
	   included_set(t->neg, t1->neg, b->sz.sym_size)) { /* t1 is redondant */
	  BTrans *free = t1->nxt;
	  t1->to->incoming--;
	  t1->to = free->to;
	  copy_set(free->pos, t1->pos, b->sz.sym_size);
	  copy_set(free->neg, t1->neg, b->sz.sym_size);
	  t1->nxt   = free->nxt;
	  if(free == s->trans) s->trans = t1;
	  free_btrans(free, 0, 0);
	  state_trans--;
	}
	else if((flags & LTL2BA_SIMP_FLY) &&
		(t1->to == to ) &&
		included_set(t1->pos, t->pos, b->sz.sym_size) &&
		included_set(t1->neg, t->neg, b->sz.sym_size)) /* t is redondant */
	  break;
	else
	  t1 = t1->nxt;
      }
      if(t1 == s->trans) {
	BTrans *trans = emalloc_btrans(b->sz.sym_size);
	trans->to = to;
	trans->to->incoming++;
	copy_set(t->pos, trans->pos, b->sz.sym_size);
	copy_set(t->neg, trans->neg, b->sz.sym_size);
	trans->nxt = s->trans->nxt;
	s->trans->nxt = trans;
	state_trans++;
      }
    }

  if(flags & LTL2BA_SIMP_FLY) {
    if(s->trans == s->trans->nxt) { /* s has no transitions */
      free_btrans(s->trans->nxt, s->trans, 1);
      s->trans = (BTrans *)0;
      s->prv = (BState *)0;
      s->nxt = bremoved->nxt;
      bremoved->nxt = s;
      for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
	if(s1->prv == s)
	  s1->prv = (BState *)0;
      return;
    }
    b->bstates->trans = s->trans;
    b->bstates->final = s->final;
    s1 = b->bstates->nxt;
    while(!all_btrans_match(b, s, s1))
      s1 = s1->nxt;
    if(s1 != b->bstates) { /* s and s1 are equivalent */
      free_btrans(s->trans->nxt, s->trans, 1);
      s->trans = (BTrans *)0;
      s->prv = s1;
      s->nxt = bremoved->nxt;
      bremoved->nxt = s;
      for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
	if(s1->prv == s)
	  s1->prv = s->prv;
      return;
    }
  }
  s->nxt = b->bstates->nxt; /* adds the current state to 'bstates' */
  s->prv = b->bstates;
  s->nxt->prv = s;
  b->bstates->nxt = s;
  c->btrans_count += state_trans;
  c->bstate_count++;
}

/********************************************************************\
|*                  Display of the Buchi automaton                  *|
\********************************************************************/

/* dumps the Buchi automaton */
static void print_buchi(FILE *f, const char *const *sym_table,
                        const Cexprtab *cexpr, const Buchi *b, BState *s,
                        int scc_size)
{
  BTrans *t;
  if(s == b->bstates) return;

  print_buchi(f, sym_table, cexpr, b, s->nxt, scc_size); /* begins with the last state */

  fprintf(f, "state ");
  if(s->id == -1)
    fprintf(f, "init");
  else {
    if(s->final == b->accept)
      fprintf(f, "accept");
    else
      fprintf(f, "T%i", s->final);
    fprintf(f, "_%i", s->id);
  }
  fprintf(f, "\n");
  for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
    if (empty_set(t->pos, b->sz.sym_size) && empty_set(t->neg, b->sz.sym_size))
      fprintf(f, "1");
    print_sym_set(f, sym_table, cexpr, t->pos, b->sz.sym_size);
    if (!empty_set(t->pos, b->sz.sym_size) && !empty_set(t->neg, b->sz.sym_size)) fprintf(f, " & ");
    print_sym_set(f, sym_table, cexpr, t->neg, b->sz.sym_size);
    fprintf(f, " -> ");
    if(t->to->id == -1)
      fprintf(f, "init\n");
    else {
      if(t->to->final == b->accept)
	fprintf(f, "accept");
      else
	fprintf(f, "T%i", t->to->final);
      fprintf(f, "_%i\n", t->to->id);
    }
  }
}

void print_spin_buchi(FILE *f, const Buchi *b, const char **sym_table) {
  BTrans *t;
  BState *s;
  int accept_all = 0;
  if(b->bstates->nxt == b->bstates) { /* empty automaton */
    fprintf(f, "never {    /* ");
    put_uform(f);
    fprintf(f, " */\n");
    fprintf(f, "T0_init:\n");
    fprintf(f, "\tfalse;\n");
    fprintf(f, "}\n");
    return;
  }
  if(b->bstates->nxt->nxt == b->bstates && b->bstates->nxt->id == 0) { /* true */
    fprintf(f, "never {    /* ");
    put_uform(f);
    fprintf(f, " */\n");
    fprintf(f, "accept_init:\n");
    fprintf(f, "\tif\n");
    fprintf(f, "\t:: (1) -> goto accept_init\n");
    fprintf(f, "\tfi;\n");
    fprintf(f, "}\n");
    return;
  }

  fprintf(f, "never { /* ");
  put_uform(f);
  fprintf(f, " */\n");
  for(s = b->bstates->prv; s != b->bstates; s = s->prv) {
    if(s->id == 0) { /* accept_all at the end */
      accept_all = 1;
      continue;
    }
    if(s->final == b->accept)
      fprintf(f, "accept_");
    else fprintf(f, "T%i_", s->final);
    if(s->id == -1)
      fprintf(f, "init:\n");
    else fprintf(f, "S%i:\n", s->id);
    if(s->trans->nxt == s->trans) {
      fprintf(f, "\tfalse;\n");
      continue;
    }
    fprintf(f, "\tif\n");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
      BTrans *t1;
      fprintf(f, "\t:: (");
      spin_print_set(f, sym_table, t->pos, t->neg, b->sz.sym_size);
      for(t1 = t; t1->nxt != s->trans; )
	if (t1->nxt->to->id == t->to->id &&
	    t1->nxt->to->final == t->to->final) {
	  fprintf(f, ") || (");
	  spin_print_set(f, sym_table, t1->nxt->pos, t1->nxt->neg, b->sz.sym_size);
	  t1->nxt = t1->nxt->nxt;
	}
	else  t1 = t1->nxt;
      fprintf(f, ") -> goto ");
      if(t->to->final == b->accept)
	fprintf(f, "accept_");
      else fprintf(f, "T%i_", t->to->final);
      if(t->to->id == 0)
	fprintf(f, "all\n");
      else if(t->to->id == -1)
	fprintf(f, "init\n");
      else fprintf(f, "S%i\n", t->to->id);
    }
    fprintf(f, "\tfi;\n");
  }
  if(accept_all) {
    fprintf(f, "accept_all:\n");
    fprintf(f, "\tskip\n");
  }
  fprintf(f, "}\n");
}

static void print_dot_state_name(FILE *f, const Buchi *b, BState *s) {
  if (s->id == -1) fprintf(f, "init");
  else if (s->id == 0) fprintf(f,"all");
  else {
    if (s->final != b->accept) fprintf(f,"T%i_",s->final);
	fprintf(f,"%i",s->id);
  }
}

void print_dot_buchi(FILE *f, const Buchi *b, const char *const *sym_table, const Cexprtab *cexpr) {
  BTrans *t;
  BState *s;
  int accept_all = 0, init_count = 0;
  if(b->bstates->nxt == b->bstates) { /* empty automaton */
    fprintf(f, "digraph G {\n");
    fprintf(f, "init [shape=circle]\n");
    fprintf(f, "}\n");
    return;
  }
  if(b->bstates->nxt->nxt == b->bstates && b->bstates->nxt->id == 0) { /* true */
    fprintf(f, "digraph G {\n");
    fprintf(f, "init -> init [label=\"{true}\",font=\"courier\"]\n");
    fprintf(f, "init [shape=doublecircle]\n");
    fprintf(f, "}\n");
    return;
  }

  fprintf(f, "digraph G {\n");
  for(s = b->bstates->prv; s != b->bstates; s = s->prv) {
    if(s->id == 0) { /* accept_all at the end */
      fprintf(f, "all [shape=doublecircle]\n");
	  fprintf(f,"all -> all [label=\"true\", fontname=\"Courier\", fontcolor=blue]");
      continue;
    }
	print_dot_state_name(f, b, s);
    if(s->final == b->accept)
      fprintf(f, " [shape=doublecircle]\n");
    else fprintf(f, " [shape=circle]\n");
    if(s->trans->nxt == s->trans) {
      continue;
    }
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
	  int need_parens=0;
      BTrans *t1;
      print_dot_state_name(f, b, s);
      fprintf(f, " -> ");
	  if (t->nxt->to->id == t->to->id &&
	        t->nxt->to->final == t->to->final)
		need_parens=1;
      print_dot_state_name(f, b, t->to);
	  fprintf(f, " [label=\""),
      dot_print_set(f, sym_table, cexpr, t->pos, t->neg, b->sz.sym_size, need_parens);
      for(t1 = t; t1->nxt != s->trans; )
	    if (t1->nxt->to->id == t->to->id &&
	        t1->nxt->to->final == t->to->final) {
	      fprintf(f, "||");
	      dot_print_set(f, sym_table, cexpr, t1->nxt->pos, t1->nxt->neg, b->sz.sym_size, b->sz.sym_size); /* TODO: need_parens == (sym_size != 0)? */
	      t1->nxt = t1->nxt->nxt;
	    }
	    else
		  t1 = t1->nxt;
        fprintf(f, "\", fontname=\"Courier\", fontcolor=blue]\n");

    }
  }
  fprintf(f, "}\n");
}

/********************************************************************\
|*                       Main method                                *|
\********************************************************************/

/* generates a Buchi automaton from the generalized Buchi automaton */
Buchi mk_buchi(Generalized *g, FILE *f, Flags flags, const char *const *sym_table, const Cexprtab *cexpr)
{
  int i;
  BState *s = (BState *)tl_emalloc(sizeof(BState));
  GTrans *t;
  BTrans *t1;
  Buchi b = { .accept = g->final[0] - 1, .sz = g->sz, };
  struct rusage tr_debut, tr_fin;
  struct timeval t_diff;
  struct bcounts cnts;
  memset(&cnts, 0, sizeof(cnts));

  BState *bstack, *bremoved;

  if(flags & LTL2BA_STATS) getrusage(RUSAGE_SELF, &tr_debut);

  bstack         = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bstack->nxt    = bstack;
  bremoved       = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bremoved->nxt  = bremoved;
  b.bstates      = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  b.bstates->nxt = s;
  b.bstates->prv = s;

  s->nxt        = b.bstates; /* creates (unique) inital state */
  s->prv        = b.bstates;
  s->id = -1;
  s->incoming = 1;
  s->final = 0;
  s->gstate = 0;
  s->trans = emalloc_btrans(b.sz.sym_size); /* sentinel */
  s->trans->nxt = s->trans;
  for(i = 0; i < g->init_size; i++)
    if(g->init[i])
      for(t = g->init[i]->trans->nxt; t != g->init[i]->trans; t = t->nxt) {
	int fin = next_final(&b, t->final, 0, g->final);
	BState *to = find_bstate(&b, &t->to, fin, s, bstack, bremoved);
	for(t1 = s->trans->nxt; t1 != s->trans;) {
	  if((flags & LTL2BA_SIMP_FLY) &&
	     (to == t1->to) &&
	     included_set(t->pos, t1->pos, b.sz.sym_size) &&
	     included_set(t->neg, t1->neg, b.sz.sym_size)) { /* t1 is redondant */
	    BTrans *free = t1->nxt;
	    t1->to->incoming--;
	    t1->to = free->to;
	    copy_set(free->pos, t1->pos, b.sz.sym_size);
	    copy_set(free->neg, t1->neg, b.sz.sym_size);
	    t1->nxt   = free->nxt;
	    if(free == s->trans) s->trans = t1;
	    free_btrans(free, 0, 0);
	  }
	else if((flags & LTL2BA_SIMP_FLY) &&
		(t1->to == to ) &&
		included_set(t1->pos, t->pos, b.sz.sym_size) &&
		included_set(t1->neg, t->neg, b.sz.sym_size)) /* t is redondant */
	  break;
	  else
	    t1 = t1->nxt;
	}
	if(t1 == s->trans) {
	  BTrans *trans = emalloc_btrans(b.sz.sym_size);
	  trans->to = to;
	  trans->to->incoming++;
	  copy_set(t->pos, trans->pos, b.sz.sym_size);
	  copy_set(t->neg, trans->neg, b.sz.sym_size);
	  trans->nxt = s->trans->nxt;
	  s->trans->nxt = trans;
	}
      }

  while(bstack->nxt != bstack) { /* solves all states in the stack until it is empty */
    s = bstack->nxt;
    bstack->nxt = bstack->nxt->nxt;
    if(!s->incoming) {
      free_bstate(s);
      continue;
    }
    make_btrans(&b, s, g->final, flags, &cnts, bstack, bremoved);
  }

  retarget_all_btrans(&b, bremoved);

  if(flags & LTL2BA_STATS) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(f, "\nBuilding the Buchi automaton : %ld.%06lis",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(f, "\n%i states, %i transitions\n", cnts.bstate_count, cnts.btrans_count);
  }

  if(flags & LTL2BA_VERBOSE) {
    fprintf(f, "\nBuchi automaton before simplification\n");
    print_buchi(f, sym_table, cexpr, &b, b.bstates->nxt, g->scc_size);
    if(b.bstates == b.bstates->nxt)
      fprintf(f, "empty automaton, refuses all words\n");
  }

  if(flags & LTL2BA_SIMP_DIFF) {
    simplify_btrans(&b, f, flags);
    if(flags & LTL2BA_SIMP_SCC) simplify_bscc(&b, bremoved);
    while(simplify_bstates(&b, f, flags, &g->gstate_id, bremoved)) { /* simplifies as much as possible */
      simplify_btrans(&b, f, flags);
      if(flags & LTL2BA_SIMP_SCC) simplify_bscc(&b, bremoved);
    }

    if(flags & LTL2BA_VERBOSE) {
      fprintf(f, "\nBuchi automaton after simplification\n");
      print_buchi(f, sym_table, cexpr, &b, b.bstates->nxt, g->scc_size);
      if(b.bstates == b.bstates->nxt)
	fprintf(f, "empty automaton, refuses all words\n");
      fprintf(f, "\n");
    }
  }

  return b;
}


static void print_c_headers(FILE *f, const Cexprtab *cexpr,
                            const char *c_sym_name_prefix,
                            const char *extern_header)
{
  int i;

  /* Need some headers... */
  fprintf(f, "#include <pthread.h>\n");
  fprintf(f, "#include <stdbool.h>\n");
  fprintf(f, "#include <stdint.h>\n\n");

  /* Declare ESBMC routines we'll be using too */
  fprintf(f, "void __ESBMC_switch_to_monitor(void);\n");
  fprintf(f, "void __ESBMC_switch_from_monitor(void);\n");
  fprintf(f, "void __ESBMC_register_monitor(pthread_t t);\n");
  fprintf(f, "void __ESBMC_really_atomic_begin();\n");
  fprintf(f, "void __ESBMC_really_atomic_end();\n");
  fprintf(f, "void __ESBMC_atomic_begin();\n");
  fprintf(f, "void __ESBMC_atomic_end();\n");
  fprintf(f, "void __ESBMC_assume(_Bool prop);\n");
  fprintf(f, "void __ESBMC_kill_monitor();\n");
  fprintf(f, "unsigned int nondet_uint();\n\n");

  if (extern_header)
    fprintf(f, "#include %s\n", extern_header);

  /* Pump out the C expressions we'll be using */
  for (i = 0; i < cexpr->cexpr_idx; i++) {
    fprintf(f, "char __ESBMC_property__ltl2ba_cexpr_%d[] = \"%s\";\n",
            i, cexpr->cexpr_expr_table[i]);
    fprintf(f, "int %s_cexpr_%d_status(void) { return %s; }\n",
            c_sym_name_prefix, i, cexpr->cexpr_expr_table[i]);
  }
}

static int print_enum_decl(FILE *f, const Buchi *b,
                           const char *c_sym_name_prefix)
{
  BState *s;
  int num_states = 0;

  /* Generate enumeration of states */

  fprintf(f, "\ntypedef enum {\n");
  for (s = b->bstates->prv; s != b->bstates; s = s->prv) {
    num_states++;
    fprintf(f, "\t%s_state_%d,\n", c_sym_name_prefix, s->label);
  }

  fprintf(f, "} %s_state;\n\n", c_sym_name_prefix);

  return num_states;
}

static void print_buchi_statevars(FILE *f, const Buchi *b, const char *prefix,
                                  int num_states)
{
  BState *s;

  fprintf(f, "%s_state %s_statevar =", prefix, prefix);

  s = b->bstates->prv;
  fprintf(f, "%s_state_0;\n\n", prefix);

  fprintf(f, "unsigned int %s_visited_states[%d];\n\n", prefix, num_states);
}

static void print_fsm_func_opener(FILE *f)
{
  fprintf(f, "void\nltl2ba_fsm(bool state_stats, unsigned int num_iters)\n{\n");
  fprintf(f, "\tunsigned int choice;\n");
  fprintf(f, "\tunsigned int iters;\n");
  fprintf(f, "\t_Bool state_is_viable;\n\n");

  fprintf(f, "\t/* Original formula:\n\t * ");
  put_uform(f);
  fprintf(f, "\n\t */\n\n");

  fprintf(f, "\tfor (iters = 0; iters < num_iters; iters++) {\n");

  return;
}

static void print_transition_guard(FILE *f, const Buchi *b, BTrans *t,
                                   BState *state, const char *const *sym_table)
{
  BTrans *t1;
  c_print_set(f, sym_table, t->pos, t->neg, b->sz.sym_size);
  for(t1 = t->nxt; t1 != state->trans; t1=t1->nxt) {
    if (t1->to->id == t->to->id && t1->to->final == t->to->final){
      fprintf(f, ") || (");
      c_print_set(f, sym_table, t1->pos, t1->neg, b->sz.sym_size);
    }
  }
}

static void print_state_name(FILE *f, BState *s, const char *prefix)
{
  fprintf(f, "%s_state_%d", prefix, s->label);
  return;
}

static int print_c_buchi_body(FILE *f, const Buchi *b,
                              const char *const *sym_table, const char *prefix)
{
  BTrans *t, *t1;
  BState *s;
  int choice_count;
  int g_num_states = 0;

  /* Calculate number of states, globally */
  for (s = b->bstates->prv; s != b->bstates; s = s->prv)
    g_num_states++;

  /* Start in first state? From loops, ltl2ba reverses order...*/
  s = b->bstates->prv;

  fprintf(f, "\t\tchoice = nondet_uint();\n\n");
  fprintf(f, "\t\t__ESBMC_atomic_begin();\n\n");
  fprintf(f, "\t\tswitch(%s_statevar) {\n", prefix);

  for (s = b->bstates->prv; s != b->bstates; s = s->prv) {
    choice_count = 0;

    /* In each state... */
    fprintf(f, "\t\tcase ");
    print_state_name(f, s, prefix);
    fprintf(f, ":\n");

    fprintf(f, "\t\t\tstate_is_viable = (((");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
      print_transition_guard(f, b, t, s, sym_table);
      fprintf(f, ")) || ((");
    }
    fprintf(f, "false)));\n");

    fprintf(f, "\t\t\t");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
#if 0
      if (empty_set(t->pos, sym_size) && empty_set(t->neg, sym_size)) {
        continue;
      }
#endif

      fprintf(f, "if (choice == %d) {\n", choice_count++);

      fprintf(f, "\t\t\t\t__ESBMC_assume(((");
      print_transition_guard(f, b, t, s, sym_table);
      fprintf(f, ")));\n");

      fprintf(f, "\t\t\t\t%s_statevar = ", prefix);

      print_state_name(f, t->to, prefix);
      fprintf(f, ";\n", prefix);

      fprintf(f, "\t\t\t} else ");
    }

    /* And finally, a clause for if none of those transitions are viable */
    fprintf(f, "{\n");
    fprintf(f, "\t\t\t\t__ESBMC_assume(0);\n");
    fprintf(f, "\t\t\t}\n");

    fprintf(f, "\t\t\tbreak;\n");
  }

  fprintf(f, "\t\t}\n");
  fprintf(f, "\t\tif (state_stats)\n");
  fprintf(f, "\t\t\t%s_visited_states[%s_statevar]++;\n\n", prefix, prefix);
  fprintf(f, "\t\t/* __ESBMC_really_atomic_end(); */\n");
  fprintf(f, "\t\t__ESBMC_atomic_end();\n");

  return g_num_states;
}

static void print_c_buchi_body_tail(FILE *f)
{
  fprintf(f, "\
		/* __ESBMC_switch_from_monitor(); */\n\
	}\n\
\n\
	__ESBMC_assert(num_iters == iters, \"Unwind bound on ltl2ba_fsm insufficient\");\n\
\n\
	return;\n\
}\n\
\n\
");
}

static void print_c_buchi_util_funcs(FILE *f, const char *prefix)
{
  fprintf(f, "\
#ifndef LTL_PREFIX_BOUND\n\
#define LTL_PREFIX_BOUND 2147483648\n\
#endif\n\
\n\
#define max(x,y) ((x) < (y) ? (y) : (x))\n\
\n\
void * ltl2ba_thread(void *dummy)\n\
{\n\
	ltl2ba_fsm(false, LTL_PREFIX_BOUND);\n\
	return 0;\n\
	(void)dummy;\n\
}\n\
\n\
pthread_t ltl2ba_start_monitor(void)\n\
{\n\
	pthread_t t;\n\
\n\
	/* __ESBMC_really_atomic_begin(); */\n\
	__ESBMC_atomic_begin();\n\
	pthread_create(&t, NULL, ltl2ba_thread, NULL);\n\
	__ESBMC_register_monitor(t);\n\
	__ESBMC_atomic_end();\n\
	/* __ESBMC_switch_to_monitor(); */\n\
	return t;\n\
}\n\
\n\
");
}

static int increment_symbol_set(int *s, int sym_id)
{
  int i,j;
  for(i=0; i< sym_id && in_set(s, i); i++);
  if (i==sym_id)
    return 0;
  for(j=0;j<i;j++)
    rem_set(s,j);
  add_set(s,i);
  return !0;
}

static int * reachability(int * m, int rows)
{
/* This function takes a rows * cols integer array and repeatedly applies the transformation
 * M <- M*M + M to a fixed point, where the elementwise + operator is boolean OR and the * is
 * boolean AND. The idea is that the original matrix is a transition matrix, and the reachability
 * matrix gives the set of states reachable from a given initial (row) state.
 */
  int *t1 = (int*)tl_emalloc(rows*rows*sizeof(int));
  int *t2 = (int*)tl_emalloc(rows*rows*sizeof(int));
  int *m1 = m;
  int *m2 =t1;
  int i, r, c;
  int going = !0;

  while (going) {
    going = 0;
    for(r=0;r<rows;r++) {
      for(c=0;c<rows;c++) {
        m2[r*rows+c] = 0;
        for (i=0;i<rows;i++) {
          m2[r*rows+c] |= m1[r*rows+i]&m1[i*rows+c]; }
        m2[r*rows+c] |=  m1[r*rows+c];
        going |= (m2[r*rows+c] !=  m1[r*rows+c]); } }
    m1 = m2;
    m2 = (m1==t1)?t2:t1; }
  tfree(m2);
  return m1; }

static int *pess_recurse1(const struct pess_data *d, Slist* sl, int depth);

static int* pess_recurse3(const struct pess_data *d, int i, int depth) {
/* Okay, we've now pessimistically picked a set and optimistically picked
 * an element within it. So we just have to iterate the depth */
  depth--;
  if (depth == 0)
    return make_set(i, d->state_size);
  return pess_recurse1(d, d->tr[i], depth);
}

static int* pess_recurse2(const struct pess_data *d, int *s, int depth) {
/* Optimistically pick an element out of the set */
  int i;
  int *t;
  int *reach=make_set(LTL2BA_EMPTY_SET, d->state_size);
  for(i = 0; i < d->state_count; i++)
    if (in_set(s, i)) {
      merge_sets(reach,
                 t=pess_recurse3(d, i, depth),
                 d->state_size);
      tfree(t); }
  return reach;
}

static int *pess_recurse1(const struct pess_data *d, Slist* sl, int depth) {
/* Pessimistically pick a set out of p->slist */
  int *reach = dup_set(d->full_state_set, d->state_size);
  int *t, *t1;
  while (sl) {
    reach = intersect_sets(t1=reach,
                           t=pess_recurse2(d, sl->set, depth),
                           d->state_size);
    tfree(t);
    tfree(t1);
    sl = sl->nxt; }
  return reach;
}

/* We are looking for states which are reachable down _all_ the imposed Slist
 * elements. So, the one-step reachable states are simply the intersection of
 * all the Slist elements. The two-step reachable states are those for which we
 * can pick an element of each slist element and replace it with with the target
 * slist, such that the state is in the intersection of all of the new slists.
 */
static int * pess_reach(Slist **tr, int st, int depth, int state_count, int state_size) {
  int i;
  struct pess_data d;
  d.state_count = state_count;
  d.state_size = state_size;
  d.full_state_set = make_set(LTL2BA_EMPTY_SET, d.state_size);
  d.tr = tr;
  for(i=0; i < d.state_count; i++)
    add_set(d.full_state_set, i);
  return pess_recurse1(&d, tr[st], depth);
}

static void print_behaviours(const Buchi *b, FILE *f,
                             const char *const *sym_table,
                             const Cexprtab *cexpr, int sym_id,
                             struct accept_sets *as)
{
  BState *s;
  BTrans *t;
  int cex;
  int *a;
  int *transition_matrix, *optimistic_transition;
  Slist **pessimistic_transition, *set_list;
  int *working_set, *full_state_set;
  int i, j, k;
  int stut_accept_idx;
  int state_count = 0;
  int state_size;

  /* Allocate a set of sets, each representing the accepting states for each
   * input symbol combination */
  as->stutter_accept_table = tl_emalloc(sizeof(int *) * (2<<sym_id) * (2<<sym_id));
  stut_accept_idx = 0;

  /*
  if (bstates->nxt == bstates) {
    fprintf(tl_out,"\nEmpty automaton---accepts nothing\n");
    return;
  } */

  /* Horribly, if there is a state with id == 0, it can has a TRUE transition to itself,
   * which may not be explicit . So we jam this in. It is also (magically) an
   * accepting state                                                           */
  {
    int going = !0;
    for (s = b->bstates->prv; s != b->bstates; s = s->prv) {
      if (s -> id == 0) {
        for(t = s->trans->nxt; t != s -> trans; t = t->nxt) {
          if ( !t->pos && ! t->neg)
            going = 0;
        }
        if (going) {
        /* It's missing so jam it in */
          BTrans *t2 = (BTrans*)tl_emalloc(sizeof(BTrans));
          t2->nxt = s->trans->nxt;
          s->trans->nxt = t2;
          t2->pos=(int*)0;
          t2->neg=(int*)0;
          t2->to = s;
        }
      }
    }
  }

  fprintf(f,"States:\nlabel\tid\tfinal\n");
  for (s = b->bstates->prv; s != b->bstates; s = s->prv) {   /* Loop over states */
    s -> label = state_count;
    state_count++;
    fprintf(f,"%d\t",s->label);
    print_dot_state_name(f, b, s);
    /* Horribly, the correct test for an accepting state is
     *     s->final == accept || s -> id == 0
     *     Here, "final" is a VARIABLE and the state with id=0 is magic       */
    fprintf(f,"\t%d\n",s->final == b->accept || s -> id == 0); } /* END Loop over states */
  fprintf(f,"\nSymbol table:\nid\tsymbol\t\t\tcexpr\n");
  state_size = LTL2BA_SET_SIZE(state_count);
  full_state_set = make_set(LTL2BA_EMPTY_SET,state_size);
  for(i=0;i<state_count; i++)
    add_set(full_state_set,i);

  /* transition_matrix is a per symbol matrix of permitted transitions */
  transition_matrix = (int*) tl_emalloc(state_count*state_count*sizeof(int));

  /* optimistic_transition is the union of all transition_matricies,
  // i.e. transitions permitted under some symbol */
  optimistic_transition = (int*) tl_emalloc(state_count*state_count*sizeof(int));
  for (i=0;i<state_count*state_count;i++)
      optimistic_transition[i]=0;

  /* pessimistic_transition is a list of sets of transitions permitted under any symbol
  // i.e. after an externally chosen symbol, the machine is forced into one of the sets
  // but is free to chose a state within the set */
  pessimistic_transition = (Slist**) tl_emalloc(state_count*sizeof(Slist*));
  for (i=0;i<state_count;i++) {
    pessimistic_transition[i] = (Slist*)tl_emalloc(sizeof(Slist));
    pessimistic_transition[i]->set = dup_set(full_state_set, state_size);
    pessimistic_transition[i]->nxt = (Slist*)0; }
  working_set = make_set(LTL2BA_EMPTY_SET,state_size);

  /*
  for (i=0; i<cexpr->cexpr_idx; i++)
    printf("%d\t%s\n",i,cexpr->cexpr_expr_table[i]);
  fprintf(tl_out,"\n"); */

  for(i=0; i<sym_id; i++) {
    fprintf(f, "%d\t%s",i,sym_table[i]?sym_table[i]:"");
    if (sym_table[i] && sscanf(sym_table[i],"_ltl2ba_cexpr_%d_status",&cex)==1)
      /* Yes, scanning for a match here is horrid DAN */
      fprintf(f, "\t{ %s }\n", cexpr->cexpr_expr_table[cex]);
    else
      fprintf(f, "\n"); }

  fprintf(f,"\nStuttering:\n\n");
  a=make_set(LTL2BA_EMPTY_SET,b->sz.sym_size);
  do {                                     /* Loop over alphabet */
    fprintf(f,"\n");
    for (i=0;i<state_count*state_count;i++)   /* Loop over states, clearing transition matrix for this character */
      transition_matrix[i]=0;                 /* END loop over states */
    /* print_sym_set(a, sym_size); fprintf(tl_out,"\n"); */
    {                                         /* Display the current symbol */
      int first = !0;
      int i, cex;
      for(i=0; i<sym_id; i++) {
        if (!first)
          fprintf(f,"&");
        first = 0;
        if(!in_set(a,i))
          fprintf(f,"!");
        if (sscanf(sym_table[i],"_ltl2ba_cexpr_%d_status",&cex)==1)
          fprintf(f, "{%s}", cexpr->cexpr_expr_table[cex]);
        else
          fprintf(f, "%s",sym_table[i]); }
      fprintf(f,"\n");
    }

    for (s = b->bstates->prv; s != b->bstates; s = s->prv) {   /* Loop over states */
      (void)clear_set(working_set,b->sz.sym_size);                    /* clear transition targets for this state and character */
      for(t = s->trans->nxt; t != s -> trans; t = t->nxt) {       /* Loop over transitions */
#if 0
        fprintf(f,"%d--[+",s->label);
        if(t->pos) print_sym_set(f, t->pos, sym_size);
         fprintf(f," -");
        if(t->neg) print_sym_set(f, t->neg, sym_size);
        fprintf(f,"]--> %d\t",t->to->label);
        fprintf(f, "%s\n", (!t->neg || empty_intersect_sets(t->neg,a,sym_size))?
                                   ((!t->pos || included_set(t->pos,a,sym_size))?"active":""):"suppressed");
#endif

        if ((!t->pos || included_set(t->pos,a,b->sz.sym_size)) && (!t->neg || empty_intersect_sets(t->neg,a,b->sz.sym_size))) {  /* Tests TRUE if this transition is enabled on this character of the alphabet */
          add_set(working_set,t->to->label);                                  /* update working set of transition targets enabled for this character on this state */
          transition_matrix[(s->label)*state_count + (t->to->label)] = 1;     /* update per-character transition matrix */
          optimistic_transition[(s->label)*state_count + (t->to->label)] = 1; /* update optimistic (any character) transition matrix */
          }
        }                                                     /* END Lop over transitions */
        {                                                                     /* update pessimistic transition list for this state */
          set_list=pessimistic_transition[s->label];
          int add = 1;
          Slist *prev_set;
          while (set_list) {                                /* loop over list of pessimistic transitions */
            if(included_set(working_set,set_list->set, state_size)) {
              tfree(set_list->set);                                                 /* our new set is smaller than this set already in the list -> replace */
              set_list->set = dup_set(working_set, state_size); add = 0;
            } else if (included_set(set_list->set,working_set,state_size)) {
              add = 0;                                                              /* our new set is bigger than this set already in the list -> ignore */
              }
            prev_set = set_list;
            set_list = set_list->nxt; }
          if (add) {                                                              /* Our new set overlaps all existing sets, so add it */
            prev_set->nxt = (Slist*)tl_emalloc(sizeof(Slist));
            prev_set->nxt->set = dup_set(working_set, state_size);
            prev_set->nxt->nxt = (Slist*)0; }

          {                                                                            /* Eliminate duplicate sets in set list */
            Slist * set_list2 = pessimistic_transition[s->label];
            while(set_list2) {
              set_list = set_list2;
              while(set_list->nxt) {
                if(same_sets(set_list->nxt->set,set_list2->set,state_size)) {
                  Slist * drop = set_list->nxt;
                  set_list->nxt = drop -> nxt;
                  tfree(drop -> set);
                  tfree(drop);
                } else
                  set_list = set_list->nxt;
              }
              set_list2 = set_list2->nxt;
            }
          }                                                                      /* END Eliminate duplicate sets in set list */
            }                                                                 /* END update pessimistic transition list for this state */
        }                                                   /* END Loop over states */

    fprintf(f,"Transitions:\n");
    for(i=0; i<state_count; i++) {
      for(j=0;j<state_count; j++)
        fprintf(f,"%d\t",transition_matrix[i*state_count + j]);
      fprintf(f,"\n"); }
    fprintf(f,"\n");

    int * reach = reachability(transition_matrix, state_count);

    fprintf(f,"Reachability:\n");
    for(i=0; i<state_count; i++) {
      for(j=0;j<state_count; j++)
        fprintf(f,"%d\t",reach[i*state_count + j]);
      fprintf(f,"\n"); }
    fprintf(f,"\n");
    {
      BState *s2;
      int r, c;
      int * accepting_cycles=make_set(LTL2BA_EMPTY_SET,state_size);
      for (s2 = b->bstates->prv; s2 != b->bstates; s2 = s2->prv)
        if((s2->final == b->accept || s2 -> id == 0) && reach[(s2->label)*(state_count+1)])
          add_set(accepting_cycles,s2->label);
      fprintf(f,"Accepting cycles: ");
      print_set(f, accepting_cycles,state_size);
      int * accepting_states=make_set(LTL2BA_EMPTY_SET,state_size);
      for (r=0;r<state_count;r++)
        for (c=0; c<state_count;c++) {
          /* fprintf(tl_out,"\n*** r:%d c:%d reach:%d in_set:%d\n",r,c,reach[r*state_count+c],in_set(accepting_cycles,c)); */
          if(reach[r*state_count+c] && in_set(accepting_cycles,c))
            add_set(accepting_states,r); }
      fprintf(f,"\nAccepting states: ");
      print_set(f, accepting_states,state_size);
      fprintf(f,"\n");

      as->stutter_accept_table[stut_accept_idx++] = accepting_states;

      tfree(accepting_cycles);
    }
    tfree (reach);
     } while (increment_symbol_set(a, sym_id));       /* END Loop over alphabet */

  fprintf(f,"\n\nOptimistic transitions:\n");
  for(i=0; i<state_count; i++) {
    for(j=0;j<state_count; j++)
      fprintf(f,"%d\t",optimistic_transition[i*state_count + j]);
    fprintf(f,"\n"); }

  int *optimistic_reach = reachability(optimistic_transition, state_count);
  fprintf(f,"Optimistic reachability:\n");
  for(i=0; i<state_count; i++) {
    for(j=0;j<state_count; j++)
      fprintf(f,"%d\t",optimistic_reach[i*state_count + j]);
    fprintf(f,"\n"); }

  {
    BState *s2;
    int r, c;
    int * accepting_cycles=make_set(LTL2BA_EMPTY_SET,state_size);
    for (s2 = b->bstates->prv; s2 != b->bstates; s2 = s2->prv)
      if((s2->final == b->accept || s2 -> id == 0) && optimistic_reach[(s2->label)*(state_count+1)])
        add_set(accepting_cycles,s2->label);
    fprintf(f,"\nAccepting optimistic cycles: ");
    print_set(f, accepting_cycles,state_size);

    int * accepting_states=make_set(LTL2BA_EMPTY_SET,state_size);
    for (r=0;r<state_count;r++)
      for (c=0; c<state_count;c++) {
        /* fprintf(tl_out,"\n*** r:%d c:%d reach:%d in_set:%d\n",r,c,reach[r*state_count+c],in_set(accepting_cycles,c)); */
        if(optimistic_reach[r*state_count+c] && in_set(accepting_cycles,c))
          add_set(accepting_states,r); }
    fprintf(f,"\nAccepting optimistic states: ");
    print_set(f, accepting_states,state_size);
    fprintf(f,"\n");
    tfree(accepting_cycles);

    as->optimistic_accept_state_set = accepting_states;
  }
  tfree(optimistic_reach);

  fprintf(f,"\n\nPessimistic transitions:\n");
  for(i=0; i<state_count; i++) {
    fprintf(f,"%2d: ",i);
    set_list = pessimistic_transition[i];
    while (set_list != (Slist*)0) {
      print_set(f, set_list->set,state_size);
      set_list = set_list->nxt; }
    fprintf(f,"\n"); }

  int* pessimistic_reachable[state_count];
  fprintf(f,"\n\nPessimistic reachable:\n");
  for(i=0; i<state_count; i++) {
    fprintf(f,"%2d: ",i);
    pessimistic_reachable[i] = pess_reach(pessimistic_transition, i, state_count, state_count, state_size);
    print_set(f, pessimistic_reachable[i],state_size);
    fprintf(f,"\n"); }

  int *accepting_pessimistic_cycles=make_set(LTL2BA_EMPTY_SET,state_size);
  BState* s2;
  for (s2 = b->bstates->prv; s2 != b->bstates; s2 = s2->prv)
    if((s2->final == b->accept || s2 -> id == 0) && in_set(pessimistic_reachable[s2->label],s2->label))
      add_set(accepting_pessimistic_cycles,s2->label);
  fprintf(f,"\nAccepting pessimistic cycles: ");
  print_set(f, accepting_pessimistic_cycles,state_size);

  int *accepting_pessimistic_states=make_set(LTL2BA_EMPTY_SET,state_size);
  for (s2 = b->bstates->prv; s2 != b->bstates; s2 = s2->prv)
    if(!empty_intersect_sets(pessimistic_reachable[s2->label],accepting_pessimistic_cycles,state_size))
      add_set(accepting_pessimistic_states,s2->label);
  fprintf(f,"\nAccepting pessimistic states: ");
  print_set(f, accepting_pessimistic_states,state_size);
  as->pessimistic_accept_state_set = accepting_pessimistic_states;
  fprintf(f,"\n");
}

static void print_c_accept_tables(FILE *f, const char *const *sym_table,
                                  int sym_id, int g_num_states,
                                  const struct accept_sets *as,
                                  const char *c_sym_name_prefix)
{
  int sym_comb, state, i;

  int num_sym_combs = 1 << sym_id;

  /* Print static table of whether states accept, given an input symbol. */
  fprintf(f, "_Bool %s_stutter_accept_table[%d][%d] = {\n",
		  c_sym_name_prefix, num_sym_combs, g_num_states);
  for (sym_comb = 0; sym_comb < num_sym_combs; sym_comb++) {
    fprintf(f, "{\n  ");
    for (state = 0; state < g_num_states; state++) {
      if (in_set(as->stutter_accept_table[sym_comb], state))
        fprintf(f, "true, ");
      else
        fprintf(f, "false, ");
    }
    fprintf(f, "\n},\n");
  }
  fprintf(f, "};\n\n");

  fprintf(f, "_Bool %s_good_prefix_excluded_states[%d] = {\n",
		  c_sym_name_prefix, g_num_states);
  for (state = 0; state < g_num_states; state++) {
    if (in_set(as->optimistic_accept_state_set, state))
      fprintf(f, "true, ");
    else
      fprintf(f, "false, ");
  }
  fprintf(f, "\n};\n\n");

  fprintf(f, "_Bool %s_bad_prefix_states[%d] = {\n",
		  c_sym_name_prefix, g_num_states);
  for (state = 0; state < g_num_states; state++) {
    if (in_set(as->pessimistic_accept_state_set, state))
      fprintf(f, "true, ");
    else
      fprintf(f, "false, ");
  }
  fprintf(f, "\n};\n\n");

  fprintf(f, "unsigned int\n%s_sym_to_idx(void)\n{\n", c_sym_name_prefix);
  fprintf(f, "\tunsigned int idx = 0;\n");
  for (i = 0; i < sym_id; i++) {
    fprintf(f, "\tidx |= (%s()) ? 1 << %d : 0;\n", sym_table[i], i);
  }
  fprintf(f, "\treturn idx;\n}\n\n");
}

static void print_c_epilog(FILE *f, const char *c_sym_name_prefix)
{
  fprintf(f, "void\nltl2ba_finish_monitor(pthread_t t)\n{\n");
  fprintf(f, "\n\t__ESBMC_kill_monitor();\n\n");

  /* Assert we're not in a bad trap. */
  fprintf(f, "\t__ESBMC_assert(!%s_bad_prefix_states[%s_statevar],"
		  "\"LTL_BAD\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  /* Assert whether we're in a failing state */
  fprintf(f, "\t__ESBMC_assert(!%s_stutter_accept_table[%s_sym_to_idx()][%s_statevar],"
		  "\"LTL_FAILING\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  /* Assert whether we're in a succeeding state */
  fprintf(f, "\t__ESBMC_assert(!%s_good_prefix_excluded_states[%s_statevar],"
		  "\"LTL_SUCCEEDING\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  fprintf(f, "\treturn;\n}\n");
}

void print_c_buchi(FILE *f, const Buchi *b, const char *const *sym_table,
                   const Cexprtab *cexpr, int sym_id,
                   const char *c_sym_name_prefix, const char *extern_header,
                   const char *cmdline)
{
  BTrans *t, *t1;
  BState *s;
  struct accept_sets as;
  int i, num_states;

  if (b->bstates->nxt == b->bstates) {
    fprintf(f, "#error Empty Buchi automaton\n");
    return;
  } else if (b->bstates->nxt->nxt == b->bstates && b->bstates->nxt->id == 0) {
    fprintf(f, "#error Always-true Buchi automaton\n");
    return;
  }

  fprintf(f, "#if 0\n");
  if (cmdline)
    fprintf(f, "generated by libltl2ba with command: %s\n", cmdline);
  fprintf(f, "/* Precomputed transition data */\n");
  print_behaviours(b, f, sym_table, cexpr, sym_id, &as);
  fprintf(f, "#endif\n");

  print_c_headers(f, cexpr, c_sym_name_prefix, extern_header);

  num_states = print_enum_decl(f, b, c_sym_name_prefix);

  print_buchi_statevars(f, b, c_sym_name_prefix, num_states);

  /* And now produce state machine */

  print_fsm_func_opener(f);

  int g_num_states = print_c_buchi_body(f, b, sym_table, c_sym_name_prefix);

  print_c_buchi_body_tail(f);

  /* Some things vaguely in the shape of a modelling api */
  print_c_buchi_util_funcs(f, c_sym_name_prefix);

  print_c_accept_tables(f, sym_table, sym_id, g_num_states, &as, c_sym_name_prefix);

  print_c_epilog(f, c_sym_name_prefix);
}
