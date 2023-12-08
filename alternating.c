// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : alternating.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */

#include "ltl2ba.h"

/********************************************************************\
|*              Structures and shared variables                     *|
\********************************************************************/

struct counts {
  int astate_count, atrans_count;
};

static ATrans *build_alternating(const Node *p, const Node **label,
                                 Alternating *alt);

/********************************************************************\
|*              Generation of the alternating automaton             *|
\********************************************************************/

/* returns the number of temporal nodes */
static int calculate_node_size(const Node *p)
{
  switch(p->ntyp) {
  case AND:
  case OR:
  case U_OPER:
  case V_OPER:
    return(calculate_node_size(p->lft) + calculate_node_size(p->rgt) + 1);
  case NEXT:
    return(calculate_node_size(p->lft) + 1);
  default:
    return 1;
    break;
  }
}

/* returns the number of predicates */
static int calculate_sym_size(const Node *p)
{
  switch(p->ntyp) {
  case AND:
  case OR:
  case U_OPER:
  case V_OPER:
    return(calculate_sym_size(p->lft) + calculate_sym_size(p->rgt));
  case NEXT:
    return(calculate_sym_size(p->lft));
  case NOT:
  case PREDICATE:
    return 1;
  default:
    return 0;
  }
}

/* returns the copy of a transition */
ATrans *dup_trans(const struct set_sizes *sz, const ATrans *trans)
{
  ATrans *result;
  if(!trans) return NULL;
  result = emalloc_atrans(sz->sym_size, sz->node_size);
  copy_set(trans->to,  result->to,  sz->node_size);
  copy_set(trans->pos, result->pos, sz->sym_size);
  copy_set(trans->neg, result->neg, sz->sym_size);
  return result;
}

void do_merge_trans(const struct set_sizes *sz, ATrans **result,
                    const ATrans *trans1, const ATrans *trans2)
{ /* merges two transitions */
  if(!trans1 || !trans2) {
    free_atrans(*result, 0);
    *result = (ATrans *)0;
    return;
  }
  if(!*result)
    *result = emalloc_atrans(sz->sym_size, sz->node_size);
  do_merge_sets((*result)->to, trans1->to,  trans2->to,  sz->node_size);
  do_merge_sets((*result)->pos, trans1->pos, trans2->pos, sz->sym_size);
  do_merge_sets((*result)->neg, trans1->neg, trans2->neg, sz->sym_size);
  if(!empty_intersect_sets((*result)->pos, (*result)->neg, sz->sym_size)) {
    free_atrans(*result, 0);
    *result = (ATrans *)0;
  }
}

/* merges two transitions */
ATrans *merge_trans(const struct set_sizes *sz, const ATrans *trans1,
                    const ATrans *trans2)
{
  ATrans *result = emalloc_atrans(sz->sym_size, sz->node_size);
  do_merge_trans(sz, &result, trans1, trans2);
  return result;
}

/* finds the id of the node, if already explored */
static int already_done(const Node *p, const Node *const *label, int node_id)
{
  int i;
  for(i = 1; i<node_id; i++)
    if (isequal(p, label[i]))
      return i;
  return -1;
}

/* finds the id of a predicate, or attributes one */
static int get_sym_id(const char *s, Alternating *alt)
{
  int i;
  for(i=0; i<alt->sym_id; i++)
    if (!strcmp(s, alt->sym_table[i]))
      return i;
  alt->sym_table[alt->sym_id] = s;
  return alt->sym_id++;
}

/* computes the transitions to boolean nodes -> next & init */
static ATrans *boolean(const Node *p, const Node **label, Alternating *alt)
{
  ATrans *t1, *t2, *lft, *rgt, *result = (ATrans *)0;
  switch(p->ntyp) {
  case TRUE:
    result = emalloc_atrans(alt->sz.sym_size, alt->sz.node_size);
    clear_set(result->to,  alt->sz.node_size);
    clear_set(result->pos, alt->sz.sym_size);
    clear_set(result->neg, alt->sz.sym_size);
  case FALSE:
    break;
  case AND:
    lft = boolean(p->lft, label, alt);
    rgt = boolean(p->rgt, label, alt);
    for(t1 = lft; t1; t1 = t1->nxt) {
      for(t2 = rgt; t2; t2 = t2->nxt) {
	ATrans *tmp = merge_trans(&alt->sz, t1, t2);
	if(tmp) {
	  tmp->nxt = result;
	  result = tmp;
	}
      }
    }
    free_atrans(lft, 1);
    free_atrans(rgt, 1);
    break;
  case OR:
    lft = boolean(p->lft, label, alt);
    for(t1 = lft; t1; t1 = t1->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t1);
      tmp->nxt = result;
      result = tmp;
    }
    free_atrans(lft, 1);
    rgt = boolean(p->rgt, label, alt);
    for(t1 = rgt; t1; t1 = t1->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t1);
      tmp->nxt = result;
      result = tmp;
    }
    free_atrans(rgt, 1);
    break;
  default:
    build_alternating(p, label, alt);
    result = emalloc_atrans(alt->sz.sym_size, alt->sz.node_size);
    clear_set(result->to,  alt->sz.node_size);
    clear_set(result->pos, alt->sz.sym_size);
    clear_set(result->neg, alt->sz.sym_size);
    add_set(result->to, already_done(p, label, alt->node_id));
  }
  return result;
}

/* builds an alternating automaton for p */
static ATrans *build_alternating(const Node *p, const Node **label,
                                 Alternating *alt)
{
  ATrans *t1, *t2, *t = (ATrans *)0;
  int node = already_done(p, label, alt->node_id);
  if(node >= 0) return alt->transition[node];

  switch (p->ntyp) {

  case TRUE:
    t = emalloc_atrans(alt->sz.sym_size, alt->sz.node_size);
    clear_set(t->to,  alt->sz.node_size);
    clear_set(t->pos, alt->sz.sym_size);
    clear_set(t->neg, alt->sz.sym_size);
  case FALSE:
    break;

  case PREDICATE:
    t = emalloc_atrans(alt->sz.sym_size, alt->sz.node_size);
    clear_set(t->to,  alt->sz.node_size);
    clear_set(t->pos, alt->sz.sym_size);
    clear_set(t->neg, alt->sz.sym_size);
    add_set(t->pos, get_sym_id(p->sym->name, alt));
    break;

  case NOT:
    t = emalloc_atrans(alt->sz.sym_size, alt->sz.node_size);
    clear_set(t->to,  alt->sz.node_size);
    clear_set(t->pos, alt->sz.sym_size);
    clear_set(t->neg, alt->sz.sym_size);
    add_set(t->neg, get_sym_id(p->lft->sym->name, alt));
    break;

  case NEXT:
    t = boolean(p->lft, label, alt);
    break;

  case U_OPER:    /* p U q <-> q || (p && X (p U q)) */
    for(t2 = build_alternating(p->rgt, label, alt); t2; t2 = t2->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t2);  /* q */
      tmp->nxt = t;
      t = tmp;
    }
    for(t1 = build_alternating(p->lft, label, alt); t1; t1 = t1->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t1);  /* p */
      add_set(tmp->to, alt->node_id);  /* X (p U q) */
      tmp->nxt = t;
      t = tmp;
    }
    add_set(alt->final_set, alt->node_id);
    break;

  case V_OPER:    /* p V q <-> (p && q) || (p && X (p V q)) */
    for(t1 = build_alternating(p->rgt, label, alt); t1; t1 = t1->nxt) {
      ATrans *tmp;

      for(t2 = build_alternating(p->lft, label, alt); t2; t2 = t2->nxt) {
	tmp = merge_trans(&alt->sz, t1, t2);  /* p && q */
	if(tmp) {
	  tmp->nxt = t;
	  t = tmp;
	}
      }

      tmp = dup_trans(&alt->sz, t1);  /* p */
      add_set(tmp->to, alt->node_id);  /* X (p V q) */
      tmp->nxt = t;
      t = tmp;
    }
    break;

  case AND:
    t = (ATrans *)0;
    for(t1 = build_alternating(p->lft, label, alt); t1; t1 = t1->nxt) {
      for(t2 = build_alternating(p->rgt, label, alt); t2; t2 = t2->nxt) {
	ATrans *tmp = merge_trans(&alt->sz, t1, t2);
	if(tmp) {
	  tmp->nxt = t;
	  t = tmp;
	}
      }
    }
    break;

  case OR:
    t = (ATrans *)0;
    for(t1 = build_alternating(p->lft, label, alt); t1; t1 = t1->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t1);
      tmp->nxt = t;
      t = tmp;
    }
    for(t1 = build_alternating(p->rgt, label, alt); t1; t1 = t1->nxt) {
      ATrans *tmp = dup_trans(&alt->sz, t1);
      tmp->nxt = t;
      t = tmp;
    }
    break;

  default:
    break;
  }

  alt->transition[alt->node_id] = t;
  label[alt->node_id++] = p;
  return(t);
}

/********************************************************************\
|*        Simplification of the alternating automaton               *|
\********************************************************************/

/* simplifies the transitions */
static void simplify_atrans(const Alternating *alt, ATrans **trans,
                            struct counts *c)
{
  ATrans *t, *father = (ATrans *)0;
  for(t = *trans; t;) {
    ATrans *t1;
    for(t1 = *trans; t1; t1 = t1->nxt) {
      if((t1 != t) &&
	 included_set(t1->to,  t->to,  alt->sz.node_size) &&
	 included_set(t1->pos, t->pos, alt->sz.sym_size) &&
	 included_set(t1->neg, t->neg, alt->sz.sym_size))
	break;
    }
    if(t1) {
      if (father)
	father->nxt = t->nxt;
      else
	*trans = t->nxt;
      free_atrans(t, 0);
      if (father)
	t = father->nxt;
      else
	t = *trans;
      continue;
    }
    c->atrans_count++;
    father = t;
    t = t->nxt;
  }
}

/* simplifies the alternating automaton */
static void simplify_astates(const Node **label, Alternating *alt,
                             struct counts *c)
{
  ATrans *t;
  int i, *acc = make_set(-1, alt->sz.node_size); /* no state is accessible initially */

  for(t = alt->transition[0]; t; t = t->nxt, i = 0)
    merge_sets(acc, t->to, alt->sz.node_size); /* all initial states are accessible */

  for(i = alt->node_id - 1; i > 0; i--) {
    if (!in_set(acc, i)) { /* frees unaccessible states */
      label[i] = LTL2BA_ZN;
      free_atrans(alt->transition[i], 1);
      alt->transition[i] = (ATrans *)0;
      continue;
    }
    c->astate_count++;
    simplify_atrans(alt, &alt->transition[i], c);
    for(t = alt->transition[i]; t; t = t->nxt)
      merge_sets(acc, t->to, alt->sz.node_size);
  }

  tfree(acc);
}

/********************************************************************\
|*            Display of the alternating automaton                  *|
\********************************************************************/

/* dumps the alternating automaton */
static void print_alternating(FILE *f, const Node **label,
                              const tl_Cexprtab *cexpr, const Alternating *alt)
{
  int i;
  ATrans *t;

  fprintf(f, "init :\n");
  for(t = alt->transition[0]; t; t = t->nxt) {
    print_set(f, t->to, alt->sz.node_size);
    fprintf(f, "\n");
  }

  for(i = alt->node_id - 1; i > 0; i--) {
    if(!label[i])
      continue;
    fprintf(f, "state %i : ", i);
    dump(f, label[i]);
    fprintf(f, "\n");
    for(t = alt->transition[i]; t; t = t->nxt) {
      if (empty_set(t->pos, alt->sz.sym_size) && empty_set(t->neg, alt->sz.sym_size))
	fprintf(f, "1");
      print_sym_set(f, alt->sym_table, cexpr, t->pos, alt->sz.sym_size);
      if (!empty_set(t->pos,alt->sz.sym_size) && !empty_set(t->neg,alt->sz.sym_size))
        fprintf(f, " & ");
      print_sym_set(f, alt->sym_table, cexpr, t->neg, alt->sz.sym_size);
      fprintf(f, " -> ");
      print_set(f, t->to, alt->sz.node_size);
      fprintf(f, "\n");
    }
  }
}

/********************************************************************\
|*                       Main method                                *|
\********************************************************************/

/* generates an alternating automaton for p */
Alternating mk_alternating(const Node *p, FILE *tl_out, const tl_Cexprtab *cexpr,
                           tl_Flags flags)
{
  struct counts cnts;
  memset(&cnts, 0, sizeof(cnts));
  struct rusage tr_debut, tr_fin;
  struct timeval t_diff;
  Alternating alt;
  memset(&alt, 0, sizeof(alt));
  alt.node_id = 1;
  alt.sym_id = 0;

  if(flags & TL_STATS) getrusage(RUSAGE_SELF, &tr_debut);

  int the_node_size = calculate_node_size(p) + 1; /* number of states in the automaton */
  const Node **label = tl_emalloc(the_node_size * sizeof(Node *));
  alt.transition = (ATrans **) tl_emalloc(the_node_size * sizeof(ATrans *));
  alt.sz.node_size = LTL2BA_SET_SIZE(the_node_size);

  int the_sym_size = calculate_sym_size(p); /* number of predicates */
  if(the_sym_size) alt.sym_table = tl_emalloc(the_sym_size * sizeof(char *));
  alt.sz.sym_size = LTL2BA_SET_SIZE(the_sym_size);

  alt.final_set = make_set(-1, alt.sz.node_size);
  alt.transition[0] = boolean(p, label, &alt); /* generates the alternating automaton */

  if(flags & TL_VERBOSE) {
    fprintf(tl_out, "\nAlternating automaton before simplification\n");
    print_alternating(tl_out, label, cexpr, &alt);
  }

  if(flags & TL_SIMP_DIFF) {
    simplify_astates(label, &alt, &cnts); /* keeps only accessible states */
    if(flags & TL_VERBOSE) {
      fprintf(tl_out, "\nAlternating automaton after simplification\n");
      print_alternating(tl_out, label, cexpr, &alt);
    }
  }

  if(flags & TL_STATS) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nBuilding and simplification of the alternating automaton: %ld.%06lis",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states, %i transitions\n", cnts.astate_count, cnts.atrans_count);
  }

  tfree(label);

  return alt;
}
