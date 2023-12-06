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

#define Token(y)        lex->tl_yylval = tl_nn(y,ZN,ZN); return y

static int
isalnum_(int c)
{       return (isalnum(c) || c == '_');
}

static int
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
getword(tl_Lexer *lex, int first, int (*tst)(int))
{	int i=0; char c;

	lex->yytext[i++]= (char ) first;
	while (tst(c = tl_Getchar()))
		lex->yytext[i++] = c;
	lex->yytext[i] = '\0';
	tl_UnGetchar();
}

static int
follow(tl_Lexer *lex, int tok, int ifyes, int ifno)
{	int c;
	char buf[32];

	if ((c = tl_Getchar()) == tok)
		return ifyes;
	tl_UnGetchar();
	lex->tl_yychar = c;
	sprintf(buf, "expected '%c'", tok);
	tl_yyerror(lex, buf);	/* no return from here */
	return ifno;
}

static int
tl_lex(tl_Symtab symtab, tl_Cexprtab *cexpr, tl_Lexer *lex)
{	int c;

	do {
		c = tl_Getchar();
		lex->yytext[0] = (char ) c;
		lex->yytext[1] = '\0';

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

			if (c <= 0)
				tl_yyerror(lex, "Unexpected end of file during C expression");

			lex->yytext[idx++] = c;
			if (idx == 2048)
				tl_yyerror(lex, "Your C expression is too long");
		} while (1);

		lex->yytext[idx++] = '\0';
		cexpr->cexpr_expr_table[cexpr->cexpr_idx] = (char *)strdup(lex->yytext);

		for (idx = 0; idx < cexpr->cexpr_idx; idx++) {
			if (!strcmp(cexpr->cexpr_expr_table[cexpr->cexpr_idx],
			            cexpr->cexpr_expr_table[idx])) {
				sprintf(buffer, "_ltl2ba_cexpr_%d_status",idx);
				break;
			}
		}

		if (idx == cexpr->cexpr_idx)
			sprintf(buffer, "_ltl2ba_cexpr_%d_status", cexpr->cexpr_idx++);

		if (cexpr->cexpr_idx == 256)
			tl_yyerror(lex, "You have too many C expressions");

		lex->tl_yylval = tl_nn(PREDICATE,ZN,ZN);
		lex->tl_yylval->sym = tl_lookup(symtab, buffer);
		return PREDICATE;
	}


	if (islower(c))
	{	getword(lex, c, isalnum_);
		if (strcmp("true", lex->yytext) == 0)
		{	Token(TRUE);
		}
		if (strcmp("false", lex->yytext) == 0)
		{	Token(FALSE);
		}
		lex->tl_yylval = tl_nn(PREDICATE,ZN,ZN);
		lex->tl_yylval->sym = tl_lookup(symtab, lex->yytext);
		return PREDICATE;
	}
	if (c == '<')
	{	c = tl_Getchar();
		if (c == '>')
		{	Token(EVENTUALLY);
		}
		if (c != '-')
		{	tl_UnGetchar();
			tl_yyerror(lex, "expected '<>' or '<->'");
		}
		c = tl_Getchar();
		if (c == '>')
		{	Token(EQUIV);
		}
		tl_UnGetchar();
		tl_yyerror(lex, "expected '<->'");
	}
	if (c == 'N')
	{	c = tl_Getchar();
		if (c != 'O')
		{	tl_UnGetchar();
			tl_yyerror(lex, "expected 'NOT'");
		}
		c = tl_Getchar();
		if (c == 'T')
		{	Token(NOT);
		}
		tl_UnGetchar();
		tl_yyerror(lex, "expected 'NOT'");
	}

	switch (c) {
	case '/' : c = follow(lex, '\\', AND, '/'); break;
	case '\\': c = follow(lex, '/', OR, '\\'); break;
	case '&' : c = follow(lex, '&', AND, '&'); break;
	case '|' : c = follow(lex, '|', OR, '|'); break;
	case '[' : c = follow(lex, ']', ALWAYS, '['); break;
	case '-' : c = follow(lex, '>', IMPLIES, '-'); break;
	case '!' : c = NOT; break;
	case 'U' : c = U_OPER; break;
	case 'V' : c = V_OPER; break;
	case 'F' : c = EVENTUALLY; break;
	case 'G' : c = ALWAYS; break;
	case 'X' : c = NEXT; break;
	default  : break;
	}
	Token(c);
}

int
tl_yylex(tl_Symtab symtab, tl_Cexprtab *cexpr, tl_Lexer *lex)
{	int c = tl_lex(symtab, cexpr, lex);
#if 0
	printf("c = %d\n", c);
#endif
	return c;
}

Symbol *
tl_lookup(tl_Symtab symtab, char *s)
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
