// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : set.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */

#include "ltl2ba.h"

extern FILE *tl_out;

static const int mod = 8 * sizeof(int);

int *new_set(int size) /* creates a new set */
{
  return (int *)tl_emalloc(size * sizeof(int));
}

int *clear_set(int *l, int size) /* clears the set */
{
  int i;
  for(i = 0; i < size; i++) {
    l[i] = 0;
  }
  return l;
}

int *make_set(int n, int size) /* creates the set {n}, or the empty set if n = -1 */
{
  int *l = clear_set(new_set(size), size);
  if(n == -1) return l;
  l[n/mod] = 1 << (n%mod);
  return l;
}

void copy_set(int *from, int *to, int size) /* copies a set */
{
  int i;
  for(i = 0; i < size; i++)
    to[i] = from[i];
}

int *dup_set(int *l, int size) /* duplicates a set */
{
  int i, *m = new_set(size);
  for(i = 0; i < size; i++)
    m[i] = l[i];
  return m;
}

void merge_sets(int *l1, int *l2, int size) /* puts the union of the two sets in l1 */
{
  int i;
  for(i = 0; i < size; i++)
    l1[i] = l1[i] | l2[i];
}

void do_merge_sets(int *l, int *l1, int *l2, int size) /* makes the union of two sets */
{
  int i;
  for(i = 0; i < size; i++)
    l[i] = l1[i] | l2[i];
}

int *intersect_sets(int *l1, int *l2, int size) /* makes the intersection of two sets */
{
  int i, *l = new_set(size);
  for(i = 0; i < size; i++)
    l[i] = l1[i] & l2[i];
  return l;
}

int empty_intersect_sets(int *l1, int *l2, int size) /* tests intersection of two sets */
{
  int i, test = 0;
  for(i = 0; i < size; i++)
    test |= l1[i] & l2[i];
  return !test;
}

int same_set(int *l1, int *l2, int size) /* tests equality of two sets */
{
  int i, test = 0;
  for(i = 0; i < size; i++)
    test |= l1[i] != l2[i];
  return !test;
}


void add_set(int *l, int n) /* adds an element to a set */
{
  l[n/mod] |= 1 << (n%mod);
}

void rem_set(int *l, int n) /* removes an element from a set */
{
  l[n/mod] &= (-1 - (1 << (n%mod)));
}

/* prints the content of a set for spin */
void spin_print_set(const char *const *sym_table, int *pos, int *neg, int sym_size)
{
  int i, j, start = 1;
  for(i = 0; i < sym_size; i++)
    for(j = 0; j < mod; j++) {
      if(pos && pos[i] & (1 << j)) {
	if(!start)
	  fprintf(tl_out, " && ");
	fprintf(tl_out, "%s", sym_table[mod * i + j]);
	start = 0;
      }
      if(neg && neg[i] & (1 << j)) {
	if(!start)
	  fprintf(tl_out, " && ");
	fprintf(tl_out, "!%s", sym_table[mod * i + j]);
	start = 0;
      }
    }
  if(start)
    fprintf(tl_out, "1");
}

/* prints the content of a set for dot */
void dot_print_set(const char *const *sym_table, const tl_Cexprtab *cexpr,
                   int *pos, int *neg, int sym_size, int need_parens)
{
  int i, j, start = 1;
  int count = 0, cex;
  for(i = 0; i < sym_size; i++)
    for(j = 0; j < mod; j++) {
	  if(pos[i] & (1 << j)) count++;
	  if(neg[i] & (1 << j)) count++;
  }
  if (count>1 && need_parens) fprintf(tl_out,"(");
  for(i = 0; i < sym_size; i++)
    for(j = 0; j < mod; j++) {
      if(pos[i] & (1 << j)) {
	if(!start)
	  fprintf(tl_out, "&&");
	if (sscanf(sym_table[mod * i + j],"_ltl2ba_cexpr_%d_status",&cex)==1)
	/* Yes, scanning for a match here is horrid DAN */
	  fprintf(tl_out, "{%s}", cexpr->cexpr_expr_table[cex]);
	else
	  fprintf(tl_out, "%s", sym_table[mod * i + j]);
	start = 0;
      }
      if(neg[i] & (1 << j)) {
	if(!start)
	  fprintf(tl_out, "&&");
	if (sscanf(sym_table[mod * i + j],"_ltl2ba_cexpr_%d_status",&cex)==1)
	/* And it's horrid here too DAN */
	fprintf(tl_out, "!{%s}", cexpr->cexpr_expr_table[cex]);
	else
      fprintf(tl_out, "!%s", sym_table[mod * i + j]);
	start = 0;
      }
    }
  if(start)
    fprintf(tl_out, "true");
  if (count>1 && need_parens) fprintf(tl_out,")");
}

void print_set(int *l, int size) /* prints the content of a set */
{
  int i, j, start = 1;;
  fprintf(tl_out, "{");
  for(i = 0; i < size; i++)
    for(j = 0; j < mod; j++)
      if(l[i] & (1 << j)) {
        if(!start) fprintf(tl_out, ",");
        fprintf(tl_out, "%i", mod * i + j);
        start = 0;
      }
  fprintf(tl_out, "}");
}

/* prints the content of a symbol set */
void print_sym_set(const char *const *sym_table, const tl_Cexprtab *cexpr,
                   int *l, int size)
{
  int i, j, cex, start = 1;;
  for(i = 0; i < size; i++)
    for(j = 0; j < mod; j++)
      if(l[i] & (1 << j)) {
        if(!start) fprintf(tl_out, " & ");
        if (sscanf(sym_table[mod * i + j],"_ltl2ba_cexpr_%d_status",&cex)==1)
        /* Yes, scanning for a match here is horrid DAN */
          fprintf(tl_out, "{%s}", cexpr->cexpr_expr_table[cex]);
        else
          fprintf(tl_out, "%s", sym_table[mod * i + j]);
        start = 0;
      }
}


int empty_set(int *l, int size) /* tests if a set is the empty set */
{
  int i, test = 0;
  for(i = 0; i < size; i++)
    test |= l[i];
  return !test;
}

int same_sets(int *l1, int *l2, int size) /* tests if two sets are identical */
{
  int i, test = 1;
  for(i = 0; i < size; i++)
    test &= (l1[i] == l2[i]);
  return test;
}

int included_set(int *l1, int *l2, int size)
{                    /* tests if the first set is included in the second one */
  int i, test = 0;
  for(i = 0; i < size; i++)
    test |= (l1[i] & ~l2[i]);
  return !test;
}

int in_set(int *l, int n) /* tests if an element is in a set */
{
  return(l[n/mod] & (1 << (n%mod)));
}

int *list_set(int *l, int size) /* transforms a set into a list */
{
  int i, j, list_size = 1, *list;
  for(i = 0; i < size; i++)
    for(j = 0; j < mod; j++)
      if(l[i] & (1 << j))
	list_size++;
  list = (int *)tl_emalloc(list_size * sizeof(int));
  list[0] = list_size;
  list_size = 1;
  for(i = 0; i < size; i++)
    for(j = 0; j < mod; j++)
      if(l[i] & (1 << j))
	list[list_size++] = mod * i + j;
  return list;
}

