// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : main.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include <unistd.h>
#include <libgen.h>	/* basename() */
#include "ltl2ba.h"

FILE	*tl_out;

int	tl_stats     = 0; /* time and size stats */
int tl_simp_log  = 1; /* logical simplification */
int tl_simp_diff = 1; /* automata simplification */
int tl_simp_fly  = 1; /* on the fly simplification */
int tl_simp_scc  = 1; /* use scc simplification */
int tl_fjtofj    = 1; /* 2eme fj */
int	tl_errs      = 0;
int	tl_verbose   = 0;
unsigned long	All_Mem = 0;
const char *c_sym_name_prefix = "_ltl2ba";

static char	uform[4096];
static int	hasuform=0, cnt=0;

static enum {spin, c, dot, none} outmode = spin;

static const char *progname;

static void	tl_endstats(void);
static void	non_fatal(const char *);

static void
alldone(int estatus)
{
	exit(estatus);
}

char *
emalloc(int n)
{
	char *tmp;

	if (!(tmp = (char *) malloc(n)))
		fatal("not enough memory");
	memset(tmp, 0, n);
	return tmp;
}

int
tl_Getchar(void)
{
	if (cnt < hasuform)
		return uform[cnt++];
	cnt++;
	return -1;
}

void
put_uform(void)
{
	fprintf(tl_out, "%s", uform);
}

void
tl_UnGetchar(void)
{
	if (cnt > 0) cnt--;
}

static void
usage(int code)
{
	FILE *f = code ? stderr : stdout;
	fprintf(f, "\
usage: %s [-flag] -f 'formula'\n\
       %*s      or -F file\n\
 -f 'formula'  translate LTL formula into never claim\n\
 -F file       like -f, but with the LTL formula stored in a 1-line file\n\
 -P            Specify ltl2c symbol prefixes\n\
 -i            Invert formula once read\n\
 -d            display automata (D)escription at each step\n\
 -s            computing time and automata sizes (S)tatistics\n\
 -l            disable (L)ogic formula simplification\n\
 -p            disable a-(P)osteriori simplification\n\
 -o            disable (O)n-the-fly simplification\n\
 -c            disable strongly (C)onnected components simplification\n\
 -a            disable trick in (A)ccepting conditions\n\
 -O mode       output mode; one of spin, c or dot\n\
", progname, (int)strlen(progname), "");
	alldone(code);
}

static void
tl_main(char  *formula)
{
	for (int i = 0; formula[i]; i++)
		if (formula[i] == '\t'
		||  formula[i] == '\"'
		||  formula[i] == '\n')
			formula[i] = ' ';

	strcpy(uform, formula);
	hasuform = strlen(uform);

	Node *p = tl_formula();
	if (tl_verbose)
	{
		fprintf(stderr, "formula: ");
		FILE *f = tl_out;
		tl_out = stderr;
		put_uform();
		tl_out = f;
		fprintf(stderr, "\n");
	}

	if (!p || tl_errs)
		return;

	if (tl_verbose) {
		FILE *f = tl_out;
		tl_out = stderr;
		fprintf(tl_out, "\t/* Normlzd: ");
		dump(p);
		fprintf(tl_out, " */\n");
		tl_out = f;
	}

	Alternating alt = mk_alternating(p);
	mk_generalized(&alt);
	mk_buchi();

	switch (outmode) {
	case none:	break;
	case c: 	print_c_buchi(alt.sym_table, alt.sym_id); break;
	case dot: 	print_dot_buchi(alt.sym_table); break;
	case spin:
	default:	print_spin_buchi(alt.sym_table); break;
	}

	if (tl_stats)
		tl_endstats();
}

int
main(int argc, char *argv[])
{
	int i;
	int invert_formula = 0;
	char *ltl_file = (char *)0;
	char *add_ltl  = (char *)0;
	char formula[4096], inv_formula[4100];
	tl_out = stdout;

	progname = argv[0] ? basename(argv[0]) : "";
	if (!strcmp(progname, "ltl2c"))
		outmode = c;

	for (int opt; (opt = getopt(argc, argv, ":hF:f:acopldsO:Pi")) != -1;)
		switch (opt) {
		case 'h': usage(0); break;
		case 'F': ltl_file = optarg; break;
		case 'f': add_ltl = optarg; break;
		case 'a': tl_fjtofj = 0; break;
		case 'c': tl_simp_scc = 0; break;
		case 'o': tl_simp_fly = 0; break;
		case 'p': tl_simp_diff = 0; break;
		case 'l': tl_simp_log = 0; break;
		case 'd': tl_verbose = 1; break;
		case 's': tl_stats = 1; break;
		case 'O':
			if (strcmp("spin", optarg) == 0)
				outmode=spin;
			else if (strcmp("c", optarg) == 0)
				outmode=c;
			else if (strcmp("dot", optarg) == 0)
				outmode=dot;
			else
				usage(1);
			break;
		case 'P': c_sym_name_prefix = optarg; break;
		case 'i': invert_formula = 1; break;
		case ':':
		case '?': usage(1); break;
		}

	if(!ltl_file == !add_ltl || argc != optind)
		usage(1);

	if (ltl_file)
	{
		FILE *f = fopen(ltl_file, "r");
		if (!f)
		{
			fprintf(stderr, "%s: cannot open %s\n", progname, ltl_file);
			alldone(1);
		}
		fgets(formula, 4096, f);
		fclose(f);
		add_ltl = formula;
	}

	if (invert_formula) {
		sprintf(inv_formula, "!(%s)", add_ltl);
		add_ltl = inv_formula;
	}

	tl_main(add_ltl);

	return tl_errs != 0;
}

/* Subtract the `struct timeval' values X and Y, storing the result X-Y in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

int
timeval_subtract (result, x, y)
struct timeval *result, *x, *y;
{
	if (x->tv_usec < y->tv_usec) {
		x->tv_usec += 1000000;
		x->tv_sec--;
	}

	/* Compute the time remaining to wait. tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

static void
tl_endstats(void)
{
	/*extern int Stack_mx;*/
	fprintf(stderr, "\ntotal memory used: %9ld\n", All_Mem);
	/*fprintf(stderr, "largest stack sze: %9d\n", Stack_mx);*/
	/*cache_stats();*/
	a_stats();
}

#define Binop(a)		\
	fprintf(tl_out, "(");	\
	dump(n->lft);		\
	fprintf(tl_out, a);	\
	dump(n->rgt);		\
	fprintf(tl_out, ")")

void
dump(Node *n)
{
	if (!n)
		return;

	switch(n->ntyp) {
	case OR:	Binop(" || "); break;
	case AND:	Binop(" && "); break;
	case U_OPER:	Binop(" U ");  break;
	case V_OPER:	Binop(" V ");  break;
	case NEXT:
		fprintf(tl_out, "X");
		fprintf(tl_out, " (");
		dump(n->lft);
		fprintf(tl_out, ")");
		break;
	case NOT:
		fprintf(tl_out, "!");
		fprintf(tl_out, " (");
		dump(n->lft);
		fprintf(tl_out, ")");
		break;
	case FALSE:
		fprintf(tl_out, "false");
		break;
	case TRUE:
		fprintf(tl_out, "true");
		break;
	case PREDICATE:
		fprintf(tl_out, "(%s)", n->sym->name);
		break;
	case -1:
		fprintf(tl_out, " D ");
		break;
	default:
		fprintf(stderr,"Unknown token: ");
		tl_explain(n->ntyp);
		break;
	}
}

void
tl_explain(int n)
{
	switch (n) {
	case ALWAYS:	fprintf(stderr,"[]"); break;
	case EVENTUALLY: fprintf(stderr,"<>"); break;
	case IMPLIES:	fprintf(stderr,"->"); break;
	case EQUIV:	fprintf(stderr,"<->"); break;
	case PREDICATE:	fprintf(stderr,"predicate"); break;
	case OR:	fprintf(stderr,"||"); break;
	case AND:	fprintf(stderr,"&&"); break;
	case NOT:	fprintf(stderr,"!"); break;
	case U_OPER:	fprintf(stderr,"U"); break;
	case V_OPER:	fprintf(stderr,"V"); break;
	case NEXT:	fprintf(stderr,"X"); break;
	case TRUE:	fprintf(stderr,"true"); break;
	case FALSE:	fprintf(stderr,"false"); break;
	case ';':	fprintf(stderr,"end of formula"); break;
	default:	fprintf(stderr,"%c", n); break;
	}
}

static void
non_fatal(const char *s1)
{
	extern int tl_yychar;
	int i;

	fprintf(stderr, "%s: ", progname);
	fputs(s1, stderr);
	if (tl_yychar != -1 && tl_yychar != 0)
	{	fprintf(stderr,", saw '");
		tl_explain(tl_yychar);
		fprintf(stderr,"'");
	}
	fprintf(stderr,"\n%s: %s\n---------", progname, uform);
	for (i = 0; i < cnt; i++)
		fprintf(stderr,"-");
	fprintf(stderr,"^\n");
	fflush(stderr);
	tl_errs++;
}

void
tl_yyerror(char *s1)
{
	fatal(s1);
}

void
fatal(const char *s1)
{
	non_fatal(s1);
	alldone(1);
}
