// SPDX-License-Identifier: GPL-2.0+
/***** ltl2ba : lex.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ltl2ba.h"

static Symbol	*symtab[Nhash+1];
static int	tl_lex(void);

extern YYSTYPE	tl_yylval;
char	yytext[2048];

int cexpr_idx = 0;
char *cexpr_expr_table[256];

#define Token(y)        tl_yylval = tl_nn(y,ZN,ZN); return y

int
isalnum_(int c)
{       return (isalnum(c) || c == '_');
}

int
hash(char *s)
{       int h=0;

        while (*s)
        {       h += *s++;
                h <<= 1;
                if (h&(Nhash+1))
                        h |= 1;
        }
        return h&Nhash;
}

static void
getword(int first, int (*tst)(int))
{	int i=0; char c;

	yytext[i++]= (char ) first;
	while (tst(c = tl_Getchar()))
		yytext[i++] = c;
	yytext[i] = '\0';
	tl_UnGetchar();
}

static int
follow(int tok, int ifyes, int ifno)
{	int c;
	char buf[32];
	extern int tl_yychar;

	if ((c = tl_Getchar()) == tok)
		return ifyes;
	tl_UnGetchar();
	tl_yychar = c;
	sprintf(buf, "expected '%c'", tok);
	tl_yyerror(buf);	/* no return from here */
	return ifno;
}

int
tl_yylex(void)
{	int c = tl_lex();
#if 0
	printf("c = %d\n", c);
#endif
	return c;
}

static int
tl_lex(void)
{	int c;

	do {
		c = tl_Getchar();
		yytext[0] = (char ) c;
		yytext[1] = '\0';

		if (c <= 0)
		{	Token(';');
		}

	} while (c == ' ');	/* '\t' is removed in tl_main.c */

	if (c == '{') {
		char buffer[256];
		int idx = 0;

		do {
			c = tl_Getchar();
			if (c == '}')
				break;

			if (c <= 0) {
				fprintf(stderr, "Unexpected end of file during C expression\n");
				exit(1);
			}

			yytext[idx++] = c;
			if (idx == 2048) {
				fprintf(stderr, "Your C expression is too long\n");
				exit(1);
			}
		} while (1);

		yytext[idx++] = '\0';
		cexpr_expr_table[cexpr_idx] = (char *)strdup(yytext);

		for (idx = 0; idx < cexpr_idx; idx++) {
			if (!strcmp(cexpr_expr_table[cexpr_idx], cexpr_expr_table[idx])) {
				sprintf(buffer, "_ltl2ba_cexpr_%d_status",idx);
				break;
			}
		}

		if (idx == cexpr_idx)
			sprintf(buffer, "_ltl2ba_cexpr_%d_status", cexpr_idx++);

		if (cexpr_idx == 256) {
			fprintf(stderr, "You have too many C expressions\n");
			exit(1);
		}

		tl_yylval = tl_nn(PREDICATE,ZN,ZN);
		tl_yylval->sym = tl_lookup(buffer);
		return PREDICATE;
	}


	if (islower(c))
	{	getword(c, isalnum_);
		if (strcmp("true", yytext) == 0)
		{	Token(TRUE);
		}
		if (strcmp("false", yytext) == 0)
		{	Token(FALSE);
		}
		tl_yylval = tl_nn(PREDICATE,ZN,ZN);
		tl_yylval->sym = tl_lookup(yytext);
		return PREDICATE;
	}
	if (c == '<')
	{	c = tl_Getchar();
		if (c == '>')
		{	Token(EVENTUALLY);
		}
		if (c != '-')
		{	tl_UnGetchar();
			tl_yyerror("expected '<>' or '<->'");
		}
		c = tl_Getchar();
		if (c == '>')
		{	Token(EQUIV);
		}
		tl_UnGetchar();
		tl_yyerror("expected '<->'");
	}
	if (c == 'N')
	{	c = tl_Getchar();
		if (c != 'O')
		{	tl_UnGetchar();
			tl_yyerror("expected 'NOT'");
		}
		c = tl_Getchar();
		if (c == 'T')
		{	Token(NOT);
		}
		tl_UnGetchar();
		tl_yyerror("expected 'NOT'");
	}

	switch (c) {
	case '/' : c = follow('\\', AND, '/'); break;
	case '\\': c = follow('/', OR, '\\'); break;
	case '&' : c = follow('&', AND, '&'); break;
	case '|' : c = follow('|', OR, '|'); break;
	case '[' : c = follow(']', ALWAYS, '['); break;
	case '-' : c = follow('>', IMPLIES, '-'); break;
	case '!' : c = NOT; break;
	case 'U' : c = U_OPER; break;
	case 'V' : c = V_OPER; break;
	case 'F' : c = EVENTUALLY; break;
	case 'G' : c = ALWAYS; break;
#ifdef NXT
	case 'X' : c = NEXT; break;
#endif
	default  : break;
	}
	Token(c);
}

Symbol *
tl_lookup(char *s)
{	Symbol *sp;
	int h = hash(s);

	for (sp = symtab[h]; sp; sp = sp->next)
		if (strcmp(sp->name, s) == 0)
			return sp;

	sp = (Symbol *) tl_emalloc(sizeof(Symbol));
	sp->name = (char *) tl_emalloc(strlen(s) + 1);
	strcpy(sp->name, s);
	sp->next = symtab[h];
	symtab[h] = sp;

	return sp;
}

Symbol *
getsym(Symbol *s)
{	Symbol *n = (Symbol *) tl_emalloc(sizeof(Symbol));

	n->name = s->name;
	return n;
}
