/***** ltl2ba : buchi.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* This program is free software; you can redistribute it and/or modify   */
/* it under the terms of the GNU General Public License as published by   */
/* the Free Software Foundation; either version 2 of the License, or      */
/* (at your option) any later version.                                    */
/*                                                                        */
/* This program is distributed in the hope that it will be useful,        */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/* GNU General Public License for more details.                           */
/*                                                                        */
/* You should have received a copy of the GNU General Public License      */
/* along with this program; if not, write to the Free Software            */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA*/
/*                                                                        */
/* Based on the translation algorithm by Gastin and Oddoux,               */
/* presented at the 13th International Conference on Computer Aided       */
/* Verification, CAV 2001, Paris, France.                                 */
/* Proceedings - LNCS 2102, pp. 53-65                                     */
/*                                                                        */
/* Send bug-reports and/or questions to Paul Gastin                       */
/* http://www.lsv.ens-cachan.fr/~gastin                                   */

#include "ltl2ba.h"

/********************************************************************\
|*              Structures and shared variables                     *|
\********************************************************************/

extern GState **init, *gstates;
extern struct rusage tr_debut, tr_fin;
extern struct timeval t_diff;
extern int tl_verbose, tl_stats, tl_simp_diff, tl_simp_fly, tl_simp_scc,
  init_size, *final;
extern int sym_size, scc_size;
extern void put_uform(void);

extern FILE *tl_out;	
BState *bstack, *bstates, *bremoved;
BScc *scc_stack;
int accept, bstate_count = 0, btrans_count = 0, rank;

/* Record of what states stutter-accept, according to each input symbol. */
int **stutter_accept_table = NULL;
int *optimistic_accept_state_set = NULL;
int *pessimistic_accept_state_set = NULL;
static int g_num_states = 0;

extern char **sym_table;
extern const char *c_sym_name_prefix;
extern int sym_id;
extern enum outmodes outmode;

/********************************************************************\
|*        Simplification of the generalized Buchi automaton         *|
\********************************************************************/

void free_bstate(BState *s) /* frees a state and its transitions */
{
  free_btrans(s->trans->nxt, s->trans, 1);
  tfree(s);
}

BState *remove_bstate(BState *s, BState *s1) /* removes a state */
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

void copy_btrans(BTrans *from, BTrans *to) {
  to->to    = from->to;
  copy_set(from->pos, to->pos, sym_size);
  copy_set(from->neg, to->neg, sym_size);
}

int simplify_btrans() /* simplifies the transitions */
{
  BState *s;
  BTrans *t, *t1;
  int changed = 0;

  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);

  for (s = bstates->nxt; s != bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans;) {
      t1 = s->trans->nxt;
      copy_btrans(t, s->trans);
      while((t == t1) || (t->to != t1->to) ||
            !included_set(t1->pos, t->pos, sym_size) ||
            !included_set(t1->neg, t->neg, sym_size))
        t1 = t1->nxt;
      if(t1 != s->trans) {
        BTrans *free = t->nxt;
        t->to    = free->to;
        copy_set(free->pos, t->pos, sym_size);
        copy_set(free->neg, t->neg, sym_size);
        t->nxt   = free->nxt;
        if(free == s->trans) s->trans = t;
        free_btrans(free, 0, 0);
        changed++;
      }
      else
        t = t->nxt;
    }
      
  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nSimplification of the Buchi automaton - transitions: %i.%06is",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i transitions removed\n", changed);

  }
  return changed;
}

int same_btrans(BTrans *s, BTrans *t) /* returns 1 if the transitions are identical */
{
  return((s->to == t->to) &&
	 same_sets(s->pos, t->pos, sym_size) &&
	 same_sets(s->neg, t->neg, sym_size));
}

void remove_btrans(BState *to) 
{             /* redirects transitions before removing a state from the automaton */
  BState *s;
  BTrans *t;
  int i;
  for (s = bstates->nxt; s != bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans; t = t->nxt)
      if (t->to == to) { /* transition to a state with no transitions */
	BTrans *free = t->nxt;
	t->to = free->to;
	copy_set(free->pos, t->pos, sym_size);
	copy_set(free->neg, t->neg, sym_size);
	t->nxt   = free->nxt;
	if(free == s->trans) s->trans = t;
	free_btrans(free, 0, 0);
      }
}

void retarget_all_btrans()
{             /* redirects transitions before removing a state from the automaton */
  BState *s;
  BTrans *t;
  for (s = bstates->nxt; s != bstates; s = s->nxt)
    for (t = s->trans->nxt; t != s->trans; t = t->nxt)
      if (!t->to->trans) { /* t->to has been removed */
	t->to = t->to->prv;
	if(!t->to) { /* t->to has no transitions */
	  BTrans *free = t->nxt;
	  t->to = free->to;
	  copy_set(free->pos, t->pos, sym_size);
	  copy_set(free->neg, t->neg, sym_size);
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

int all_btrans_match(BState *a, BState *b) /* decides if the states are equivalent */
{	
  BTrans *s, *t;
  if (((a->final == accept) || (b->final == accept)) &&
      (a->final + b->final != 2 * accept) && 
      a->incoming >=0 && b->incoming >=0)
    return 0; /* the states have to be both final or both non final */

  for (s = a->trans->nxt; s != a->trans; s = s->nxt) { 
                                /* all transitions from a appear in b */
    copy_btrans(s, b->trans);
    t = b->trans->nxt;
    while(!same_btrans(s, t))
      t = t->nxt;
    if(t == b->trans) return 0;
  }
  for (s = b->trans->nxt; s != b->trans; s = s->nxt) { 
                                /* all transitions from b appear in a */
    copy_btrans(s, a->trans);
    t = a->trans->nxt;
    while(!same_btrans(s, t))
      t = t->nxt;
    if(t == a->trans) return 0;
  }
  return 1;
}

int simplify_bstates() /* eliminates redundant states */
{
  BState *s, *s1;
  int changed = 0;

  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);

  for (s = bstates->nxt; s != bstates; s = s->nxt) {
    if(s->trans == s->trans->nxt) { /* s has no transitions */
      s = remove_bstate(s, (BState *)0);
      changed++;
      continue;
    }
    bstates->trans = s->trans;
    bstates->final = s->final;
    s1 = s->nxt;
    while(!all_btrans_match(s, s1))
      s1 = s1->nxt;
    if(s1 != bstates) { /* s and s1 are equivalent */
      if(s1->incoming == -1)
        s1->final = s->final; /* get the good final condition */
      s = remove_bstate(s, s1);
      changed++;
    }
  }
  retarget_all_btrans();

  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nSimplification of the Buchi automaton - states: %i.%06is",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states removed\n", changed);
  }

  return changed;
}

int bdfs(BState *s) {
  BTrans *t;
  BScc *c;
  BScc *scc = (BScc *)tl_emalloc(sizeof(BScc));
  scc->bstate = s;
  scc->rank = rank;
  scc->theta = rank++;
  scc->nxt = scc_stack;
  scc_stack = scc;

  s->incoming = 1;

  for (t = s->trans->nxt; t != s->trans; t = t->nxt) {
    if (t->to->incoming == 0) {
      int result = bdfs(t->to);
      scc->theta = min(scc->theta, result);
    }
    else {
      for(c = scc_stack->nxt; c != 0; c = c->nxt)
	if(c->bstate == t->to) {
	  scc->theta = min(scc->theta, c->rank);
	  break;
	}
    }
  }
  if(scc->rank == scc->theta) {
    if(scc_stack == scc) { /* s is alone in a scc */
      s->incoming = -1;
      for (t = s->trans->nxt; t != s->trans; t = t->nxt)
	if (t->to == s)
	  s->incoming = 1;
    }
    scc_stack = scc->nxt;
  }
  return scc->theta;
}

void simplify_bscc() {
  BState *s;
  rank = 1;
  scc_stack = 0;

  if(bstates == bstates->nxt) return;

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    s->incoming = 0; /* state color = white */

  bdfs(bstates->prv);

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    if(s->incoming == 0)
      remove_bstate(s, 0);
}




/********************************************************************\
|*              Generation of the Buchi automaton                   *|
\********************************************************************/

BState *find_bstate(GState **state, int final, BState *s)
{                       /* finds the corresponding state, or creates it */
  if((s->gstate == *state) && (s->final == final)) return s; /* same state */

  s = bstack->nxt; /* in the stack */
  bstack->gstate = *state;
  bstack->final = final;
  while(!(s->gstate == *state) || !(s->final == final))
    s = s->nxt;
  if(s != bstack) return s;

  s = bstates->nxt; /* in the solved states */
  bstates->gstate = *state;
  bstates->final = final;
  while(!(s->gstate == *state) || !(s->final == final))
    s = s->nxt;
  if(s != bstates) return s;

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
  s->trans = emalloc_btrans(); /* sentinel */
  s->trans->nxt = s->trans;
  s->nxt = bstack->nxt;
  bstack->nxt = s;
  return s;
}

int next_final(int *set, int fin) /* computes the 'final' value */
{
  if((fin != accept) && in_set(set, final[fin + 1]))
    return next_final(set, fin + 1);
  return fin;
}

void make_btrans(BState *s) /* creates all the transitions from a state */
{
  int state_trans = 0;
  GTrans *t;
  BTrans *t1;
  BState *s1;
  if(s->gstate->trans)
    for(t = s->gstate->trans->nxt; t != s->gstate->trans; t = t->nxt) {
      int fin = next_final(t->final, (s->final == accept) ? 0 : s->final);
      BState *to = find_bstate(&t->to, fin, s);
      
      for(t1 = s->trans->nxt; t1 != s->trans;) {
	if(tl_simp_fly && 
	   (to == t1->to) &&
	   included_set(t->pos, t1->pos, sym_size) &&
	   included_set(t->neg, t1->neg, sym_size)) { /* t1 is redondant */
	  BTrans *free = t1->nxt;
	  t1->to->incoming--;
	  t1->to = free->to;
	  copy_set(free->pos, t1->pos, sym_size);
	  copy_set(free->neg, t1->neg, sym_size);
	  t1->nxt   = free->nxt;
	  if(free == s->trans) s->trans = t1;
	  free_btrans(free, 0, 0);
	  state_trans--;
	}
	else if(tl_simp_fly &&
		(t1->to == to ) &&
		included_set(t1->pos, t->pos, sym_size) &&
		included_set(t1->neg, t->neg, sym_size)) /* t is redondant */
	  break;
	else
	  t1 = t1->nxt;
      }
      if(t1 == s->trans) {
	BTrans *trans = emalloc_btrans();
	trans->to = to;
	trans->to->incoming++;
	copy_set(t->pos, trans->pos, sym_size);
	copy_set(t->neg, trans->neg, sym_size);
	trans->nxt = s->trans->nxt;
	s->trans->nxt = trans;
	state_trans++;
      }
    }
  
  if(tl_simp_fly) {
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
    bstates->trans = s->trans;
    bstates->final = s->final;
    s1 = bstates->nxt;
    while(!all_btrans_match(s, s1))
      s1 = s1->nxt;
    if(s1 != bstates) { /* s and s1 are equivalent */
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
  s->nxt = bstates->nxt; /* adds the current state to 'bstates' */
  s->prv = bstates;
  s->nxt->prv = s;
  bstates->nxt = s;
  btrans_count += state_trans;
  bstate_count++;
}

/********************************************************************\
|*                  Display of the Buchi automaton                  *|
\********************************************************************/

void print_buchi(BState *s) /* dumps the Buchi automaton */
{
  BTrans *t;
  if(s == bstates) return;

  print_buchi(s->nxt); /* begins with the last state */

  fprintf(tl_out, "state ");
  if(s->id == -1)
    fprintf(tl_out, "init");
  else {
    if(s->final == accept)
      fprintf(tl_out, "accept");
    else
      fprintf(tl_out, "T%i", s->final);
    fprintf(tl_out, "_%i", s->id);
  }
  fprintf(tl_out, "\n");
  for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
    if (empty_set(t->pos, sym_size) && empty_set(t->neg, sym_size))
      fprintf(tl_out, "1");
    print_set(t->pos, sym_size);
    if (!empty_set(t->pos, sym_size) && !empty_set(t->neg, sym_size)) fprintf(tl_out, " & ");
    print_set(t->neg, scc_size);
    fprintf(tl_out, " -> ");
    if(t->to->id == -1) 
      fprintf(tl_out, "init\n");
    else {
      if(t->to->final == accept)
	fprintf(tl_out, "accept");
      else
	fprintf(tl_out, "T%i", t->to->final);
      fprintf(tl_out, "_%i\n", t->to->id);
    }
  }
}

void print_c_buchi();
void print_spin_buchi() {
  BTrans *t;
  BState *s;
  int accept_all = 0, init_count = 0;
  if(bstates->nxt == bstates) { /* empty automaton */
    fprintf(tl_out, "never {    /* ");
    put_uform();
    fprintf(tl_out, " */\n");
    fprintf(tl_out, "T0_init:\n");
    fprintf(tl_out, "\tfalse;\n");
    fprintf(tl_out, "}\n");
    return;
  }
  if(bstates->nxt->nxt == bstates && bstates->nxt->id == 0) { /* true */
    fprintf(tl_out, "never {    /* ");
    put_uform();
    fprintf(tl_out, " */\n");
    fprintf(tl_out, "accept_init:\n");
    fprintf(tl_out, "\tif\n");
    fprintf(tl_out, "\t:: (1) -> goto accept_init\n");
    fprintf(tl_out, "\tfi;\n");
    fprintf(tl_out, "}\n");
    return;
  }

  fprintf(tl_out, "never { /* ");
  put_uform();
  fprintf(tl_out, " */\n");
  for(s = bstates->prv; s != bstates; s = s->prv) {
    if(s->id == 0) { /* accept_all at the end */
      accept_all = 1;
      continue;
    }
    if(s->final == accept)
      fprintf(tl_out, "accept_");
    else fprintf(tl_out, "T%i_", s->final);
    if(s->id == -1)
      fprintf(tl_out, "init:\n");
    else fprintf(tl_out, "S%i:\n", s->id);
    if(s->trans->nxt == s->trans) {
      fprintf(tl_out, "\tfalse;\n");
      continue;
    }
    fprintf(tl_out, "\tif\n");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
      BTrans *t1;
      fprintf(tl_out, "\t:: (");
      spin_print_set(t->pos, t->neg);
      for(t1 = t; t1->nxt != s->trans; )
	if (t1->nxt->to->id == t->to->id &&
	    t1->nxt->to->final == t->to->final) {
	  fprintf(tl_out, ") || (");
	  spin_print_set(t1->nxt->pos, t1->nxt->neg);
	  t1->nxt = t1->nxt->nxt; 
	}
	else  t1 = t1->nxt;
      fprintf(tl_out, ") -> goto ");
      if(t->to->final == accept)
	fprintf(tl_out, "accept_");
      else fprintf(tl_out, "T%i_", t->to->final);
      if(t->to->id == 0)
	fprintf(tl_out, "all\n");
      else if(t->to->id == -1)
	fprintf(tl_out, "init\n");
      else fprintf(tl_out, "S%i\n", t->to->id);
    }
    fprintf(tl_out, "\tfi;\n");
  }
  if(accept_all) {
    fprintf(tl_out, "accept_all:\n");
    fprintf(tl_out, "\tskip\n");
  }
  fprintf(tl_out, "}\n");
}

void print_dot_state_name(BState *s) {
  if (s->id == -1) fprintf(tl_out, "init");
  else if (s->id == 0) fprintf(tl_out,"all");
  else {
    if (s->final != accept) fprintf(tl_out,"T%i_",s->final);
	fprintf(tl_out,"%i",s->id);
  }
}

void print_dot_buchi() {
  BTrans *t;
  BState *s;
  int accept_all = 0, init_count = 0;
  if(bstates->nxt == bstates) { /* empty automaton */
    fprintf(tl_out, "digraph G {\n");
    fprintf(tl_out, "init [shape=circle]\n");
    fprintf(tl_out, "}\n");
    return;
  }
  if(bstates->nxt->nxt == bstates && bstates->nxt->id == 0) { /* true */
    fprintf(tl_out, "digraph G {\n");
    fprintf(tl_out, "init -> init [label=\"{true}\",font=\"courier\"]\n");
    fprintf(tl_out, "init [shape=doublecircle]\n");
    fprintf(tl_out, "}\n");
    return;
  }

  fprintf(tl_out, "digraph G {\n");
  for(s = bstates->prv; s != bstates; s = s->prv) {
    if(s->id == 0) { /* accept_all at the end */
      fprintf(tl_out, "all [shape=doublecircle]\n");
	  fprintf(tl_out,"all -> all [label=\"true\", fontname=\"Courier\", fontcolor=blue]");
      continue;
    }
	print_dot_state_name(s);
    if(s->final == accept)
      fprintf(tl_out, " [shape=doublecircle]\n");
    else fprintf(tl_out, " [shape=circle]\n");
    if(s->trans->nxt == s->trans) {
      continue;
    }
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
	  int need_parens=0;
      BTrans *t1;
      print_dot_state_name(s);
      fprintf(tl_out, " -> ");
	  if (t->nxt->to->id == t->to->id &&
	        t->nxt->to->final == t->to->final)
		need_parens=1;
      print_dot_state_name(t->to);
	  fprintf(tl_out, " [label=\""),
      dot_print_set(t->pos, t->neg,need_parens);
      for(t1 = t; t1->nxt != s->trans; )
	    if (t1->nxt->to->id == t->to->id &&
	        t1->nxt->to->final == t->to->final) {
	      fprintf(tl_out, "||");
	      dot_print_set(t1->nxt->pos, t1->nxt->neg,sym_size);
	      t1->nxt = t1->nxt->nxt; 
	    }
	    else  
		  t1 = t1->nxt;
        fprintf(tl_out, "\", fontname=\"Courier\", fontcolor=blue]\n");

    }
  }
  fprintf(tl_out, "}\n");
}

/********************************************************************\
|*                       Main method                                *|
\********************************************************************/

void mk_buchi() 
{/* generates a Buchi automaton from the generalized Buchi automaton */
  int i;
  BState *s = (BState *)tl_emalloc(sizeof(BState));
  GTrans *t;
  BTrans *t1;
  accept = final[0] - 1;
  
  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);

  bstack        = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bstack->nxt   = bstack;
  bremoved      = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bremoved->nxt = bremoved;
  bstates       = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bstates->nxt  = s;
  bstates->prv  = s;

  s->nxt        = bstates; /* creates (unique) inital state */
  s->prv        = bstates;
  s->id = -1;
  s->incoming = 1;
  s->final = 0;
  s->gstate = 0;
  s->trans = emalloc_btrans(); /* sentinel */
  s->trans->nxt = s->trans;
  for(i = 0; i < init_size; i++) 
    if(init[i])
      for(t = init[i]->trans->nxt; t != init[i]->trans; t = t->nxt) {
	int fin = next_final(t->final, 0);
	BState *to = find_bstate(&t->to, fin, s);
	for(t1 = s->trans->nxt; t1 != s->trans;) {
	  if(tl_simp_fly && 
	     (to == t1->to) &&
	     included_set(t->pos, t1->pos, sym_size) &&
	     included_set(t->neg, t1->neg, sym_size)) { /* t1 is redondant */
	    BTrans *free = t1->nxt;
	    t1->to->incoming--;
	    t1->to = free->to;
	    copy_set(free->pos, t1->pos, sym_size);
	    copy_set(free->neg, t1->neg, sym_size);
	    t1->nxt   = free->nxt;
	    if(free == s->trans) s->trans = t1;
	    free_btrans(free, 0, 0);
	  }
	else if(tl_simp_fly &&
		(t1->to == to ) &&
		included_set(t1->pos, t->pos, sym_size) &&
		included_set(t1->neg, t->neg, sym_size)) /* t is redondant */
	  break;
	  else
	    t1 = t1->nxt;
	}
	if(t1 == s->trans) {
	  BTrans *trans = emalloc_btrans();
	  trans->to = to;
	  trans->to->incoming++;
	  copy_set(t->pos, trans->pos, sym_size);
	  copy_set(t->neg, trans->neg, sym_size);
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
    make_btrans(s);
  }

  retarget_all_btrans();

  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nBuilding the Buchi automaton : %i.%06is",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states, %i transitions\n", bstate_count, btrans_count);
  }

  if(tl_verbose) {
    fprintf(tl_out, "\nBuchi automaton before simplification\n");
    print_buchi(bstates->nxt);
    if(bstates == bstates->nxt) 
      fprintf(tl_out, "empty automaton, refuses all words\n");  
  }

  if(tl_simp_diff) {
    simplify_btrans();
    if(tl_simp_scc) simplify_bscc();
    while(simplify_bstates()) { /* simplifies as much as possible */
      simplify_btrans();
      if(tl_simp_scc) simplify_bscc();
    }
    
    if(tl_verbose) {
      fprintf(tl_out, "\nBuchi automaton after simplification\n");
      print_buchi(bstates->nxt);
      if(bstates == bstates->nxt) 
	fprintf(tl_out, "empty automaton, refuses all words\n");
      fprintf(tl_out, "\n");
    }
  }

switch (outmode) {
	case none:	break;
	case c: 	print_c_buchi(); break;
	case dot: 	print_dot_buchi(); break;
	case spin:
	default:	print_spin_buchi(); break;
	}
}

extern int cexpr_idx;
extern char *cexpr_expr_table[];

void
print_c_headers(void)
{
  int i;

  /* Need some headers... */
  fprintf(tl_out, "#include <pthread.h>\n");
  fprintf(tl_out, "#include <stdbool.h>\n");
  fprintf(tl_out, "#include <stdint.h>\n\n");

  /* Declare ESBMC routines we'll be using too */
  fprintf(tl_out, "void __ESBMC_switch_to_monitor(void);\n");
  fprintf(tl_out, "void __ESBMC_switch_from_monitor(void);\n");
  fprintf(tl_out, "void __ESBMC_register_monitor(pthread_t t);\n");
  fprintf(tl_out, "void __ESBMC_really_atomic_begin();\n");
  fprintf(tl_out, "void __ESBMC_really_atomic_end();\n");
  fprintf(tl_out, "void __ESBMC_atomic_begin();\n");
  fprintf(tl_out, "void __ESBMC_atomic_end();\n");
  fprintf(tl_out, "void __ESBMC_assume(bool prop);\n");
  fprintf(tl_out, "void __ESBMC_kill_monitor();\n");
  fprintf(tl_out, "int nondet_uint();\n\n");

  /* Pump out the C expressions we'll be using */
  for (i = 0; i < cexpr_idx; i++) {
    fprintf(tl_out, "char __ESBMC_property__ltl2ba_cexpr_%d[] = \"%s\";\n", i, cexpr_expr_table[i]);
    fprintf(tl_out, "int %s_cexpr_%d_status;\n", c_sym_name_prefix, i);
  }

  return;
}

int
print_enum_decl(void)
{
  BState *s;
  int num_states = 0;

  /* Generate enumeration of states */

  fprintf(tl_out, "\ntypedef enum {\n");
  for (s = bstates->prv; s != bstates; s = s->prv) {
    num_states++;
    fprintf(tl_out, "\t%s_state_%d,\n", c_sym_name_prefix, s->label);
  }

  fprintf(tl_out, "} %s_state;\n\n", c_sym_name_prefix);

  return num_states;
}

void
print_buchi_statevars(const char *prefix, int num_states)
{
  BState *s;

  fprintf(tl_out, "%s_state %s_statevar =", prefix, prefix);

  s = bstates->prv;
  fprintf(tl_out, "%s_state_0;\n\n", prefix);

  fprintf(tl_out, "unsigned int %s_visited_states[%d];\n\n",
		  prefix, num_states);

  fprintf(tl_out, "#include <stdbool.h>\n", prefix);

  return;
}

void
print_fsm_func_opener(void)
{

  fprintf(tl_out, "void\nltl2ba_fsm(bool state_stats, unsigned int num_iters)\n{\n");
  fprintf(tl_out, "\tunsigned int choice;\n");
  fprintf(tl_out, "\tunsigned int iters;\n");
  fprintf(tl_out, "\t_Bool state_is_viable;\n\n");

  fprintf(tl_out, "\t/* Original formula:\n\t * ");
  put_uform();
  fprintf(tl_out, "\n\t */\n\n");

  fprintf(tl_out, "\tfor (iters = 0; iters < num_iters; iters++) {\n");

  return;
}

void
print_transition_guard(BTrans *t, BState *state)
{
  BTrans *t1;
  spin_print_set(t->pos, t->neg);
  for(t1 = t->nxt; t1 != state->trans; t1=t1->nxt) {
    if (t1->to->id == t->to->id && t1->to->final == t->to->final){
      fprintf(tl_out, ") || (");
      spin_print_set(t1->pos, t1->neg);
    }
  }
}

void
print_state_name(BState *s, const char *prefix)
{
  fprintf(tl_out, "%s_state_%d", prefix, s->label);
  return;
}

void
print_c_buchi_body(const char *prefix)
{
  BTrans *t, *t1;
  BState *s;
  int choice_count;

  /* Calculate number of states, globally */
  for (s = bstates->prv; s != bstates; s = s->prv)
    g_num_states++;

  /* Start in first state? From loops, ltl2ba reverses order...*/
  s = bstates->prv;

  fprintf(tl_out, "\t\tchoice = nondet_uint();\n\n");
  fprintf(tl_out, "\t\t__ESBMC_atomic_begin();\n\n");
  fprintf(tl_out, "\t\tswitch(%s_statevar) {\n", prefix);

  for (s = bstates->prv; s != bstates; s = s->prv) {
    choice_count = 0;

    /* In each state... */
    fprintf(tl_out, "\t\tcase ");
    print_state_name(s, prefix);
    fprintf(tl_out, ":\n");

    fprintf(tl_out, "\t\t\tstate_is_viable = (((");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
      print_transition_guard(t, s);
      fprintf(tl_out, ")) || ((");
    }
    fprintf(tl_out, "false)));\n");

    fprintf(tl_out, "\t\t\t");
    for(t = s->trans->nxt; t != s->trans; t = t->nxt) {
#if 0
      if (empty_set(t->pos, sym_size) && empty_set(t->neg, sym_size)) {
        continue;
      }
#endif

      fprintf(tl_out, "if (choice == %d) {\n", choice_count++);

      fprintf(tl_out, "\t\t\t\t__ESBMC_assume(((");
      print_transition_guard(t, s);
      fprintf(tl_out, ")));\n");

      fprintf(tl_out, "\t\t\t\t%s_statevar = ", prefix);

      print_state_name(t->to, prefix);
      fprintf(tl_out, ";\n", prefix);

      fprintf(tl_out, "\t\t\t} else ");
    }

    /* And finally, a clause for if none of those transitions are viable */
    fprintf(tl_out, "{\n");
    fprintf(tl_out, "\t\t\t\t__ESBMC_assume(0);\n");
    fprintf(tl_out, "\t\t\t}\n");

    fprintf(tl_out, "\t\t\tbreak;\n");
  }

  fprintf(tl_out, "\t\t}\n");
  fprintf(tl_out, "\t\tif (state_stats)\n");
  fprintf(tl_out, "\t\t\t%s_visited_states[%s_statevar]++;\n\n", prefix, prefix);
  fprintf(tl_out, "\t\t__ESBMC_really_atomic_end();\n");

  return;
}

void
print_c_buchi_body_tail(void)
{

  fprintf(tl_out, "\t\t__ESBMC_switch_from_monitor();\n");
  fprintf(tl_out, "\t}\n\n");
  fprintf(tl_out, "\t__ESBMC_assert(num_iters == iters, \"Unwind bound on ltl2ba_fsm insufficient\");\n\n");
  fprintf(tl_out, "\treturn;\n}\n\n");
  return;
}

void
print_c_buchi_util_funcs(const char *prefix)
{
  BState *s;

  fprintf(tl_out, "#ifndef LTL_PREFIX_BOUND\n");
  fprintf(tl_out, "#define LTL_PREFIX_BOUND 2147483648\n");
  fprintf(tl_out, "#endif\n\n");
  fprintf(tl_out, "#define max(x,y) ((x) < (y) ? (y) : (x))\n\n");
  fprintf(tl_out, "int\nltl2ba_thread(int *dummy)\n{\n\n");
  fprintf(tl_out, "\tltl2ba_fsm(false, LTL_PREFIX_BOUND);\n\treturn 0;\n}\n\n");

  fprintf(tl_out, "pthread_t\nltl2ba_start_monitor(void)\n{\n");
  fprintf(tl_out, "\tpthread_t t;\n\n");
  fprintf(tl_out, "\t__ESBMC_really_atomic_begin();\n");
  fprintf(tl_out, "\tpthread_create(&t, NULL, ltl2ba_thread, NULL);\n");
  fprintf(tl_out, "\t__ESBMC_register_monitor(t);\n");
  fprintf(tl_out, "\t__ESBMC_atomic_end();\n");
  fprintf(tl_out, "\t__ESBMC_switch_to_monitor();\n");
  fprintf(tl_out, "\treturn t;\n}\n\n");
  return;
}

int state_count=0;

int increment_symbol_set(int *s) 
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
    
  

typedef struct Slist {
  int * set;
  struct Slist * nxt; } Slist;

int * reachability(int * m, int rows)
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
  
int state_size;
int *full_state_set;

int *pess_recurse1(Slist **tr, Slist* sl, int depth);

int* pess_recurse3(Slist **tr, int i, int depth) {
/* Okay, we've now pessimistically picked a set and optimistically picked
 * an element within it. So we just have to iterate the depth */
  depth--;
  if (depth == 0) 
    return make_set(i, state_size);
  return pess_recurse1(tr, tr[i], depth);
}


int* pess_recurse2(Slist **tr, int * s, int depth) {
/* Optimistically pick an element out of the set */
  int i;
  int *t;
  int *reach=make_set(EMPTY_SET,state_size);
  for(i=0; i< state_count; i++)
    if (in_set(s, i)) {
      merge_sets(reach,t=pess_recurse3(tr, i, depth),state_size);
      tfree(t); }
  return reach;
}

  

int *pess_recurse1(Slist **tr, Slist* sl, int depth) {
/* Pessimistically pick a set out of p->slist */
  int * reach = dup_set(full_state_set,state_size);
  int *t, *t1;
  while (sl) {
    reach = intersect_sets(t1=reach, t=pess_recurse2(tr, sl->set, depth), state_size);
    tfree(t);
    tfree(t1); 
    sl = sl->nxt; }
  return reach;
}


int * pess_reach(Slist **tr, int st, int depth) {
/* We are looking for states which are reachable down _all_ the imposed Slist elements 
 * So, the one-step reachable states are simply the intersection of all the Slist elements.
 * The two-step reachable states are those for which we can pick an element of each slist element and replace it 
 * with with the target slist, such that the state is in the intersection of all of the new slists. */
  int i;
  full_state_set = make_set(EMPTY_SET,state_size);
  for(i=0; i< state_count; i++)
    add_set(full_state_set, i);
  return pess_recurse1(tr, tr[st], depth);
}

void
print_behaviours()
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

    /* Allocate a set of sets, each representing the accepting states for each
     * input symbol combination */
    stutter_accept_table = tl_emalloc(sizeof(int *) * (2<<sym_id) * (2<<sym_id));
    stut_accept_idx = 0;

/*    if (bstates->nxt == bstates) {
      fprintf(tl_out,"\nEmpty automaton---accepts nothing\n");
      return; 
    } */
    /* Horribly, if there is a state with id == 0, it can has a TRUE transition to itself, 
     * which may not be explicit . So we jam this in. It is also (magically) an
     * accepting state                                                           */
    {
      int going = !0;
      for (s = bstates -> prv; s != bstates; s = s -> prv) {
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
    fprintf(tl_out,"States:\nlabel\tid\tfinal\n");
    for (s = bstates -> prv; s != bstates; s = s -> prv) {   /* Loop over states */
      s -> label = state_count;
      state_count++;
      fprintf(tl_out,"%d\t",s->label);
      print_dot_state_name(s);
      /* Horribly, the correct test for an accepting state is
       *     s->final == accept || s -> id == 0
       *     Here, "final" is a VARIABLE and the state with id=0 is magic       */
      fprintf(tl_out,"\t%d\n",s->final == accept || s -> id == 0); } /* END Loop over states */
    fprintf(tl_out,"\nSymbol table:\nid\tsymbol\t\t\tcexpr\n");
    state_size = SET_SIZE(state_count);
    full_state_set = make_set(EMPTY_SET,state_size);
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
    working_set = make_set(EMPTY_SET,state_size);

/*    for (i=0; i<cexpr_idx; i++)
      printf("%d\t%s\n",i,cexpr_expr_table[i]);
    fprintf(tl_out,"\n");
*/
    for(i=0; i<sym_id; i++) {
      printf("%d\t%s",i,sym_table[i]?sym_table[i]:"");
      if (sym_table[i] && sscanf(sym_table[i],"_ltl2ba_cexpr_%d_status",&cex)==1)
        /* Yes, scanning for a match here is horrid DAN */
        fprintf(tl_out, "\t{ %s }\n", cexpr_expr_table[cex]);
        else
          fprintf(tl_out, "\n"); }

    fprintf(tl_out,"\nStuttering:\n\n");
    a=make_set(EMPTY_SET,sym_size);
    do {                                     /* Loop over alphabet */
      fprintf(tl_out,"\n");
      for (i=0;i<state_count*state_count;i++)   /* Loop over states, clearing transition matrix for this character */
        transition_matrix[i]=0;                 /* END loop over states */
/*    print_sym_set(a, sym_size); fprintf(tl_out,"\n"); */
      { 					/* Display the current symbol */
        int first = !0;
	int i, cex;
        for(i=0; i<sym_id; i++) {
	  if (!first)
	    fprintf(tl_out,"&");
	  first = 0;
	  if(!in_set(a,i))
	    fprintf(tl_out,"!");
	  if (sscanf(sym_table[i],"_ltl2ba_cexpr_%d_status",&cex)==1)
	    fprintf(tl_out, "{%s}", cexpr_expr_table[cex]);
	  else
            fprintf(tl_out, "%s",sym_table[i]); }
	fprintf(tl_out,"\n");
      }

      for (s = bstates -> prv; s != bstates; s = s -> prv) {   /* Loop over states */
        (void)clear_set(working_set,sym_size);                    /* clear transition targets for this state and character */
        for(t = s->trans->nxt; t != s -> trans; t = t->nxt) {       /* Loop over transitions */
#if 0 
	  fprintf(tl_out,"%d--[+",s->label);
	  if(t->pos) print_sym_set(t->pos, sym_size);
	   fprintf(tl_out," -");
	  if(t->neg) print_sym_set(t->neg, sym_size);
	  fprintf(tl_out,"]--> %d\t",t->to->label);
	  fprintf(tl_out, "%s\n", (!t->neg || empty_intersect_sets(t->neg,a,sym_size))?
	                             ((!t->pos || included_set(t->pos,a,sym_size))?"active":""):"suppressed");
#endif

	  if ((!t->pos || included_set(t->pos,a,sym_size)) && (!t->neg || empty_intersect_sets(t->neg,a,sym_size))) {  /* Tests TRUE if this transition is enabled on this character of the alphabet */
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

            {									    /* Eliminate duplicate sets in set list */
	      Slist * set_list2 = pessimistic_transition[s->label];
	      while(set_list2) {
	        set_list = set_list2;
	        while(set_list->nxt) {
		  if(same_set(set_list->nxt->set,set_list2->set,state_size)) {
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

      fprintf(tl_out,"Transitions:\n");
      for(i=0; i<state_count; i++) {
        for(j=0;j<state_count; j++)
	  fprintf(tl_out,"%d\t",transition_matrix[i*state_count + j]);
	fprintf(tl_out,"\n"); }
      fprintf(tl_out,"\n"); 

      int * reach = reachability(transition_matrix, state_count);

      fprintf(tl_out,"Reachability:\n");
      for(i=0; i<state_count; i++) {
        for(j=0;j<state_count; j++)
          fprintf(tl_out,"%d\t",reach[i*state_count + j]);
        fprintf(tl_out,"\n"); }
      fprintf(tl_out,"\n");
      { 
        BState *s2;
        int r, c;
        int * accepting_cycles=make_set(EMPTY_SET,state_size);
        for (s2 = bstates -> prv; s2 != bstates; s2 = s2 -> prv) 
          if((s2->final == accept || s2 -> id == 0) && reach[(s2->label)*(state_count+1)]) 
            add_set(accepting_cycles,s2->label);
        fprintf(tl_out,"Accepting cycles: ");
        print_set(accepting_cycles,state_size);
        int * accepting_states=make_set(EMPTY_SET,state_size);
        for (r=0;r<state_count;r++) 
          for (c=0; c<state_count;c++) {
	    /* fprintf(tl_out,"\n*** r:%d c:%d reach:%d in_set:%d\n",r,c,reach[r*state_count+c],in_set(accepting_cycles,c)); */
            if(reach[r*state_count+c] && in_set(accepting_cycles,c))
              add_set(accepting_states,r); }
        fprintf(tl_out,"\nAccepting states: ");
        print_set(accepting_states,state_size);
	fprintf(tl_out,"\n");

        stutter_accept_table[stut_accept_idx++] = accepting_states;

        tfree(accepting_cycles);
      }
      tfree (reach); 
       } while (increment_symbol_set(a));       /* END Loop over alphabet */

    fprintf(tl_out,"\n\nOptimistic transitions:\n");
    for(i=0; i<state_count; i++) {
      for(j=0;j<state_count; j++)
        fprintf(tl_out,"%d\t",optimistic_transition[i*state_count + j]); 
      fprintf(tl_out,"\n"); }

    int * optimistic_reach = reachability(optimistic_transition, state_count);
    fprintf(tl_out,"Optimistic reachability:\n");
    for(i=0; i<state_count; i++) {
      for(j=0;j<state_count; j++)
        fprintf(tl_out,"%d\t",optimistic_reach[i*state_count + j]);
      fprintf(tl_out,"\n"); }
      {
        BState *s2;
        int r, c;
        int * accepting_cycles=make_set(EMPTY_SET,state_size);
        for (s2 = bstates -> prv; s2 != bstates; s2 = s2 -> prv)
          if((s2->final == accept || s2 -> id == 0) && optimistic_reach[(s2->label)*(state_count+1)])
            add_set(accepting_cycles,s2->label);
        fprintf(tl_out,"\nAccepting optimistic cycles: ");
        print_set(accepting_cycles,state_size);
        int * accepting_states=make_set(EMPTY_SET,state_size);
        for (r=0;r<state_count;r++)
          for (c=0; c<state_count;c++) {
            /* fprintf(tl_out,"\n*** r:%d c:%d reach:%d in_set:%d\n",r,c,reach[r*state_count+c],in_set(accepting_cycles,c)); */
            if(optimistic_reach[r*state_count+c] && in_set(accepting_cycles,c))
              add_set(accepting_states,r); }
        fprintf(tl_out,"\nAccepting optimistic states: ");
        print_set(accepting_states,state_size);
        fprintf(tl_out,"\n");
        tfree(accepting_cycles);

        optimistic_accept_state_set = accepting_states;
      }
      tfree(optimistic_reach);

    fprintf(tl_out,"\n\nPessimistic transitions:\n");
    for(i=0; i<state_count; i++) {
      fprintf(tl_out,"%2d: ",i);
      set_list = pessimistic_transition[i];
      while (set_list != (Slist*)0) {
        print_set(set_list->set,state_size);
	set_list = set_list->nxt; }
      fprintf(tl_out,"\n"); }
    int* pessimistic_reachable[state_count];
    fprintf(tl_out,"\n\nPessimistic reachable:\n");
    for(i=0; i<state_count; i++) { 
      fprintf(tl_out,"%2d: ",i);
      pessimistic_reachable[i] = pess_reach(pessimistic_transition, i, state_count);
      print_set(pessimistic_reachable[i],state_size);
      fprintf(tl_out,"\n"); }
    int *accepting_pessimistic_cycles=make_set(EMPTY_SET,state_size);
    BState* s2;
    for (s2 = bstates -> prv; s2 != bstates; s2 = s2 -> prv)
      if((s2->final == accept || s2 -> id == 0) && in_set(pessimistic_reachable[s2->label],s2->label))
        add_set(accepting_pessimistic_cycles,s2->label);
    fprintf(tl_out,"\nAccepting pessimistic cycles: ");
    print_set(accepting_pessimistic_cycles,state_size);
    int *accepting_pessimistic_states=make_set(EMPTY_SET,state_size);
    for (s2 = bstates -> prv; s2 != bstates; s2 = s2 -> prv)
      if(!empty_intersect_sets(pessimistic_reachable[s2->label],accepting_pessimistic_cycles,state_size))
        add_set(accepting_pessimistic_states,s2->label);
    fprintf(tl_out,"\nAccepting pessimistic states: ");
    print_set(accepting_pessimistic_states,state_size);
    pessimistic_accept_state_set = accepting_pessimistic_states;
    fprintf(tl_out,"\n");
}

void
print_c_buchi()
{

  BTrans *t, *t1;
  BState *s;
  int i, num_states;

  if (bstates->nxt == bstates) {
    fprintf(tl_out, "#error Empty Buchi automaton\n");
    return;
  } else if (bstates->nxt->nxt == bstates && bstates->nxt->id == 0) {
    fprintf(tl_out, "#error Always-true Buchi automaton\n");
    return;
  }

  fprintf(tl_out, "#if 0\n/* Precomputed transition data */\n");
  print_behaviours();
  fprintf(tl_out, "#endif\n");

  print_c_headers();

  num_states = print_enum_decl();

  print_buchi_statevars(c_sym_name_prefix, num_states);

  /* And now produce state machine */

  print_fsm_func_opener();

  print_c_buchi_body(c_sym_name_prefix);

  print_c_buchi_body_tail();

  /* Some things vaguely in the shape of a modelling api */
  print_c_buchi_util_funcs(c_sym_name_prefix);

  print_c_accept_tables();

  print_c_epilog();
  return;
}

void
print_c_accept_tables(void)
{
  int sym_comb, state, i;

  int num_sym_combs = 1 << sym_id;

  /* Print static table of whether states accept, given an input symbol. */
  fprintf(tl_out, "_Bool %s_stutter_accept_table[%d][%d] = {\n",
		  c_sym_name_prefix, num_sym_combs, g_num_states);
  for (sym_comb = 0; sym_comb < num_sym_combs; sym_comb++) {
    fprintf(tl_out, "{\n  ");
    for (state = 0; state < g_num_states; state++) {
      if (in_set(stutter_accept_table[sym_comb], state))
        fprintf(tl_out, "true, ");
      else
        fprintf(tl_out, "false, ");
    }
    fprintf(tl_out, "\n},\n");
  }
  fprintf(tl_out, "};\n\n");

  fprintf(tl_out, "_Bool %s_good_prefix_excluded_states[%d] = {\n",
		  c_sym_name_prefix, g_num_states);
  for (state = 0; state < g_num_states; state++) {
    if (in_set(optimistic_accept_state_set, state))
      fprintf(tl_out, "true, ");
    else
      fprintf(tl_out, "false, ");
  }
  fprintf(tl_out, "\n};\n\n");

  fprintf(tl_out, "_Bool %s_bad_prefix_states[%d] = {\n",
		  c_sym_name_prefix, g_num_states);
  for (state = 0; state < g_num_states; state++) {
    if (in_set(pessimistic_accept_state_set, state))
      fprintf(tl_out, "true, ");
    else
      fprintf(tl_out, "false, ");
  }
  fprintf(tl_out, "\n};\n\n");

  fprintf(tl_out, "unsigned int\n%s_sym_to_idx(void)\n{\n", c_sym_name_prefix);
  fprintf(tl_out, "\tunsigned int idx = 0;\n");
  for (i = 0; i < sym_id; i++) {
    fprintf(tl_out, "\tidx |= (%s) ? %d : 0;\n", sym_table[i], 1 << i);
  }
  fprintf(tl_out, "\treturn idx;\n}\n\n");

  return;
}

void
print_c_epilog(void)
{

  fprintf(tl_out, "void\nltl2ba_finish_monitor(pthread_t t)\n{\n");
  fprintf(tl_out, "\n\t__ESBMC_kill_monitor();\n\n");

  /* Assert we're not in a bad trap. */
  fprintf(tl_out, "\t__ESBMC_assert(!%s_bad_prefix_states[%s_statevar],"
		  "\"LTL_BAD\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  /* Assert whether we're in a failing state */
  fprintf(tl_out, "\t__ESBMC_assert(!%s_stutter_accept_table[%s_sym_to_idx()][%s_statevar],"
		  "\"LTL_FAILING\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  /* Assert whether we're in a succeeding state */
  fprintf(tl_out, "\t__ESBMC_assert(!_ltl2ba_good_prefix_excluded_states[_ltl2ba_statevar],"
		  "\"LTL_SUCCEEDING\");\n\n", c_sym_name_prefix, c_sym_name_prefix,
		  c_sym_name_prefix);

  fprintf(tl_out, "\treturn;\n}\n");

  return;
}
