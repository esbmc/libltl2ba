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

static int	tl_errs      = 0;

static char	uform[4096];
static int	hasuform=0, cnt=0;

enum out {
	OUT_SPIN,
	OUT_C,
	OUT_DOT
};

static const char *progname;

static void	tl_endstats(void);

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

void put_uform(FILE *f)
{
	fprintf(f, "%s", uform);
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
 -C            dump cache info at the end\n\
", progname, (int)strlen(progname), "");
	alldone(code);
}

static void tl_main(char *formula, enum out outmode, tl_Flags flags,
                    const char *c_sym_name_prefix)
{
	for (int i = 0; formula[i]; i++)
		if (formula[i] == '\t'
		||  formula[i] == '\"'
		||  formula[i] == '\n')
			formula[i] = ' ';

	strcpy(uform, formula);
	hasuform = strlen(uform);

	tl_Symtab symtab;
	memset(&symtab, 0, sizeof(symtab));
	tl_Cexprtab cexpr;
	memset(&cexpr, 0, sizeof(cexpr));

	Node *p = tl_parse(symtab, &cexpr, flags);
	if (flags & LTL2BA_VERBOSE)
	{
		fprintf(stderr, "formula: ");
		put_uform(stderr);
		fprintf(stderr, "\n");
	}

	if (!p || tl_errs)
		return;

	if (flags & LTL2BA_VERBOSE) {
		fprintf(stderr, "\t/* Normlzd: ");
		dump(stderr, p);
		fprintf(stderr, " */\n");
	}

	Alternating alt = mk_alternating(p, stderr, &cexpr, flags);
	releasenode(1, p);

	Generalized gen = mk_generalized(&alt, stderr, flags, &cexpr);
	// free the data from the alternating automaton
	/* for(i = 0; i < alt->node_id; i++)
		free_atrans(transition[i], 1); */
	free_all_atrans();
	tfree(alt.transition);

	Buchi b = mk_buchi(&gen, stderr, flags, alt.sym_table, &cexpr);

	switch (outmode) {
	case OUT_SPIN:
		print_spin_buchi(stdout, &b, alt.sym_table);
		break;
	case OUT_C:
		print_c_buchi(stdout, &b, alt.sym_table, &cexpr, alt.sym_id, c_sym_name_prefix);
		break;
	case OUT_DOT:
		print_dot_buchi(stdout, &b, alt.sym_table, &cexpr);
		break;
	}

	if (flags & LTL2BA_STATS)
		tl_endstats();
}

int main(int argc, char *argv[])
{
	int i;
	int invert_formula = 0;
	char *ltl_file = NULL;
	char *add_ltl  = NULL;
	char *formula  = NULL, *inv_formula = NULL;
	tl_Flags flags = LTL2BA_SIMP_LOG
	               | LTL2BA_SIMP_DIFF
	               | LTL2BA_SIMP_FLY
	               | LTL2BA_SIMP_SCC
	               | LTL2BA_FJTOFJ;
	enum out outmode = OUT_SPIN;
	const char *c_sym_name_prefix = "_ltl2ba";
	int display_cache = 0;

	progname = argv[0] ? basename(argv[0]) : "";
	if (!strcmp(progname, "ltl2c"))
		outmode = OUT_C;

	for (int opt; (opt = getopt(argc, argv, ":hF:f:acopldsO:PiC")) != -1;)
		switch (opt) {
		case 'h': usage(0); break;
		case 'F': ltl_file = optarg; break;
		case 'f': add_ltl = optarg; break;
		case 'a': flags &= ~LTL2BA_FJTOFJ; break;
		case 'c': flags &= ~LTL2BA_SIMP_SCC; break;
		case 'o': flags &= ~LTL2BA_SIMP_FLY; break;
		case 'p': flags &= ~LTL2BA_SIMP_DIFF; break;
		case 'l': flags &= ~LTL2BA_SIMP_LOG; break;
		case 'd': flags |= LTL2BA_VERBOSE; break;
		case 's': flags |= LTL2BA_STATS; break;
		case 'O':
			if (strcmp("spin", optarg) == 0)
				outmode = OUT_SPIN;
			else if (strcmp("c", optarg) == 0)
				outmode = OUT_C;
			else if (strcmp("dot", optarg) == 0)
				outmode = OUT_DOT;
			else
				usage(1);
			break;
		case 'P': c_sym_name_prefix = optarg; break;
		case 'i': invert_formula = 1; break;
		case 'C': display_cache = 1; break;
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
		char buf[4096];
		formula = calloc(4096, 1);
		for (size_t rd, off = 0, sz = 4096; (rd = fread(buf, 1, sizeof(buf), f)) > 0;)
		{
			if (off + rd >= sz)
				formula = realloc(formula, sz *= 2);
			if (!formula)
				fatal("not enough memory to read the formula from file");
			memcpy(formula + off, buf, rd);
			off += rd;
			formula[off] = '\0';
		}
		fclose(f);
		add_ltl = formula;
	}

	if (invert_formula) {
		inv_formula = emalloc(strlen(add_ltl) + 4);
		if (!inv_formula)
			fatal("not enough memory to invert formula");
		sprintf(inv_formula, "!(%s)", add_ltl);
		add_ltl = inv_formula;
	}

	tl_main(add_ltl, outmode, flags, c_sym_name_prefix);

	free(formula);
	free(inv_formula);

	if (display_cache)
		cache_dump();

	return tl_errs != 0;
}

/* Subtract the `struct timeval' values X and Y, storing the result X-Y in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

void
timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	if (x->tv_usec < y->tv_usec) {
		x->tv_usec += 1000000;
		x->tv_sec--;
	}

	/* Compute the time remaining to wait. tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;
}

static void
tl_endstats(void)
{
	/*cache_stats();*/
	a_stats();
}

#define Binop(a)		\
	fprintf(f, "(");	\
	dump(f, n->lft);	\
	fprintf(f, a);		\
	dump(f, n->rgt);	\
	fprintf(f, ")")

void dump(FILE *f, const Node *n)
{
	if (!n)
		return;

	switch(n->ntyp) {
	case OR:	Binop(" || "); break;
	case AND:	Binop(" && "); break;
	case U_OPER:	Binop(" U ");  break;
	case V_OPER:	Binop(" V ");  break;
	case NEXT:
		fprintf(f, "X");
		fprintf(f, " (");
		dump(f, n->lft);
		fprintf(f, ")");
		break;
	case NOT:
		fprintf(f, "!");
		fprintf(f, " (");
		dump(f, n->lft);
		fprintf(f, ")");
		break;
	case FALSE:
		fprintf(f, "false");
		break;
	case TRUE:
		fprintf(f, "true");
		break;
	case PREDICATE:
		fprintf(f, "(%s)", n->sym->name);
		break;
	case -1:
		fprintf(f, " D ");
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
non_fatal(int tl_yychar, const char *s1)
{
	int i;

	fprintf(stderr, "%s: ", progname);
	fputs(s1, stderr);
	if (tl_yychar != -1 && tl_yychar != 0)
	{	fprintf(stderr,", saw '");
		tl_explain(tl_yychar);
		fprintf(stderr,"'");
	}
	fprintf(stderr,"\n%s: %s\n", progname, uform);
	int n = cnt + strlen(progname) + 2 - 1;
	for (i = 0; i < n; i++)
		fprintf(stderr,"-");
	fprintf(stderr,"^\n");
	fflush(stderr);
	tl_errs++;
}

void
tl_yyerror(tl_Lexer *lex, char *s1)
{
	non_fatal(lex->tl_yychar, s1);
	alldone(1);
}

void
fatal(const char *s1)
{
	non_fatal(0, s1);
	alldone(1);
}
