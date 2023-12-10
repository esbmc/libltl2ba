
#include "ltl2ba.h"

#define LTL2BA_True       tl_nn(TRUE, NULL, NULL)
#define LTL2BA_False      tl_nn(FALSE, NULL, NULL)
#define LTL2BA_Not(a)     push_negation(symtab, tl_nn(NOT, a, NULL))
#define LTL2BA_rewrite(n) canonical(symtab, right_linked(n))

#define LTL2BA_Debug(x)    { if (0) fprintf(stderr, x); }
#define LTL2BA_Debug2(x,y) { if (tl_verbose) fprintf(stderr, x,y); }
#define LTL2BA_Dump(x)     { if (0) dump(stderr, x); }
#define LTL2BA_Explain(x)  { if (tl_verbose) tl_explain(x); }

#define LTL2BA_Assert(x, y)                                                    \
	{                                                                      \
		if (!(x)) {                                                    \
			tl_explain(y);                                         \
			fatal(": assertion failed\n");                         \
		}                                                              \
	}

#undef LTL2BA_EMALLOC_VERBOSE

typedef ltl2ba_Symbol      Symbol;
typedef ltl2ba_Node        Node;
typedef ltl2ba_ATrans      ATrans;
typedef ltl2ba_GTrans      GTrans;
typedef ltl2ba_BTrans      BTrans;
typedef ltl2ba_AProd       AProd;
typedef ltl2ba_GState      GState;
typedef ltl2ba_BState      BState;
typedef ltl2ba_Alternating Alternating;
typedef ltl2ba_Generalized Generalized;
typedef ltl2ba_Buchi       Buchi;
typedef ltl2ba_Symtab      tl_Symtab;
typedef ltl2ba_Cexprtab    tl_Cexprtab;
typedef ltl2ba_Lexer       tl_Lexer;
typedef ltl2ba_Flags       tl_Flags;
typedef ltl2ba_set_sizes   set_sizes;
