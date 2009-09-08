#ifndef R_COMPAT_H__
#define R_COMPAT_H__

#include "aleph.h"

#define NILSXP       0    /* nil = NULL */
#define SYMSXP       1    /* symbols */
#define LISTSXP      2    /* lists of dotted pairs */
#define CLOSXP       3    /* closures */
#define ENVSXP       4    /* environments */
#define PROMSXP      5    /* promises: [un]evaluated closure arguments */
#define LANGSXP      6    /* language constructs (special lists) */
#define SPECIALSXP   7    /* special forms */
#define BUILTINSXP   8    /* builtin non-special forms */
#define CHARSXP      9    /* "scalar" string type (internal only)*/
#define LGLSXP      10    /* logical vectors */
#define INTSXP      13    /* integer vectors */
#define REALSXP     14    /* real variables */
#define CPLXSXP     15    /* complex variables */
#define STRSXP      16    /* string vectors */
#define DOTSXP      17    /* dot-dot-dot object */
#define ANYSXP      18    /* make "any" args work. Used in specifying types for symbol registration to mean anything is okay  */
#define VECSXP      19    /* generic vectors */
#define EXPRSXP     20    /* expressions vectors */
#define BCODESXP    21    /* byte code */
#define EXTPTRSXP   22    /* external pointer */
#define WEAKREFSXP  23    /* weak reference */
#define RAWSXP      24    /* raw bytes */
#define S4SXP       25    /* S4, non-vector */
#define FUNSXP      99    /* Closure or Builtin or Special */

typedef AObject* SEXP;
typedef vlen_t R_len_t;

#define RAPI_CALL API_CALL
#ifdef MAIN__
#define RAPI_VAR
#else
#define RAPI_VAR API_VAR
#endif

#define TYPEOF(X) (CLASS(X)->flags & 127)
#define SET_TYPE(X,Y) CLASS(X)->flags = (CLASS(X)->flags & (~ ((vlen_t)127))) | (((vlen_t) Y) & ((vlen_t)127))

API_CALL SEXP allocVector(int type, vlen_t n) {
    switch (type) {
	    case VECSXP:
		return allocObjectVector(listClass, n);
	    case STRSXP:
		return allocObjectVector(stringClass, n);
	    case INTSXP:
		return allocIntVector(n);
	    case REALSXP:
		return allocRealVector(n);
	    default:
		A_error("Sorry, unsupported allocVector type: %d", type);
    }
    return nullObject;		
}

#define isString(X) isAssignableClass(CLASS(X), stringClass)

#define list1(X) CONS(X, nullObject)
#define lang1(X) LCONS(X, nullObject)
#define list2(A, B) CONS(A, list1(B))
#define lang2(A, B) LCONS(A, list1(B))
#define list3(A, B, C) CONS(A, list2(B, C))
#define lang3(A, B, C) LCONS(A, list2(B, C))
#define list4(A, B, C, D) CONS(A, list3(B, C, D))
#define lang4(A, B, C, D) LCONS(A, list3(B, C, D))

#define CADR(e)         CAR(CDR(e))
#define CDDR(e)         CDR(CDR(e))
#define CADDR(e)        CAR(CDR(CDR(e)))
#define CADDDR(e)       CAR(CDR(CDR(CDR(e))))
#define CAD4R(e)        CAR(CDR(CDR(CDR(CDR(e)))))


#define translateChar(X) CHAR(X)

#ifndef _
#define _(X) X
#endif

#define attribute_hidden static

#define R_fgetc fgetc
#define R_atof atof

/* dummy for now */
#define PROTECT(X) (X)
#define UNPROTECT(X)

#define PROTECT_INDEX int
#define REPROTECT(X, Y) (X)
#define UNPROTECT_PTR(X)
#define PROTECT_WITH_INDEX(X,I) (X)

#define SET_NAMED(X, Y)

#define inheritsC(O, CL) isAssignableClass(CLASS(O), CL)

API_CALL SEXP setAttrib(SEXP vec, SEXP name, SEXP val) {
    if (CLASS(name) != symbolClass)
	A_error("setAttrib supports symbols only (encountered %s)", className(name));
    setAttr(vec, ASymbol2sym_t(name), val);
    return val;
}

API_CALL SEXP getAttrib(SEXP vec, SEXP name) {
    if (CLASS(name) != symbolClass)
	A_error("getAttrib supports symbols only (encountered %s)", className(name));
    return getAttr(vec, ASymbol2sym_t(name));
}

static vlen_t length(SEXP x) { return LENGTH(x); }

/* more internal stuff (used by the parser) */
#define R_EOF -1
#ifdef MAIN__
#define extern0
#define INI_as(v) = v
#else
#define extern0 extern
#define INI_as(v)
#endif

#define TRUE ((Rboolean) 1)
#define FALSE ((Rboolean) 0)

typedef int Rboolean;

/* Objects Used In Parsing  */
extern0 SEXP    R_CommentSxp;       /* Comments accumulate here */
extern0 int     R_ParseError    INI_as(0); /* Line where parse error occurred */
extern0 int     R_ParseErrorCol;    /* Column of start of token where parse error occurred */
extern0 SEXP    R_ParseErrorFile;   /* Source file where parse error was seen */
#define PARSE_ERROR_SIZE 256        /* Parse error messages saved here */
extern0 char    R_ParseErrorMsg[PARSE_ERROR_SIZE] INI_as("");
#define PARSE_CONTEXT_SIZE 256      /* Recent parse context kept in a circular buffer */
extern0 char    R_ParseContext[PARSE_CONTEXT_SIZE] INI_as("");
extern0 int     R_ParseContextLast INI_as(0); /* last character in context buffer */
extern0 int     R_ParseContextLine; /* Line in file of the above */

typedef enum {
    PARSE_NULL,
    PARSE_OK,
    PARSE_INCOMPLETE,
    PARSE_ERROR,
    PARSE_EOF
} ParseStatus;

#define MAXELTSIZE 8192 /* Used as a default for string buffer sizes */

extern0 Rboolean R_WarnEscapes  INI_as(TRUE);   /* Warn on unrecognized escapes */
extern0 Rboolean known_to_be_latin1 INI_as(0);
extern0 Rboolean known_to_be_utf8 INI_as(1);

RAPI_VAR AObject *R_MissingArg, *R_CurrentExpr, *R_NaString;
RAPI_VAR AObject *R_SrcfileSymbol, *R_SrcrefSymbol, *R_ClassSymbol;

typedef enum {
    CE_NATIVE = 0,
    CE_UTF8   = 1,
    CE_LATIN1 = 2,
    CE_SYMBOL = 5,
    CE_ANY    =99
} cetype_t;

#define error A_error
#define warning A_warning

API_CALL void warningcall(SEXP call, const char *fmt, ...) {
    va_list (ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/*-- from arithmetic.c --*/

#define NA_LOGICAL      R_NaInt
#define NA_INTEGER	R_NaInt
#define NA_REAL		R_NaReal
#define NA_STRING	R_NaString

#define ISNA(x)        R_IsNA(x)

#define R_FINITE(x)    isfinite(x)
#ifdef __cplusplus
API_CALL int R_isnancpp(double); /* in arithmetic.c */
#  define ISNAN(x)     R_isnancpp(x)
#else
#  define ISNAN(x)     (isnan(x)!=0)
#endif

#include <math.h>

#define LibExtern static

/* implementation of these : ../../main/arithmetic.c */
LibExtern double R_NaN;         /* IEEE NaN */
LibExtern double R_PosInf;      /* IEEE Inf */
LibExtern double R_NegInf;      /* IEEE -Inf */
LibExtern double R_NaReal;      /* NA_REAL: IEEE */
LibExtern int    R_NaInt;       /* NA_INTEGER:= INT_MIN currently */

static const double R_Zero_Hack = 0.0; /* Silence the Sun compiler */

typedef union {
    double value;
    unsigned int word[2];
} ieee_double;

#if __BIG_ENDIAN__ || __ppc__ || __ppc64__
#define hw__ 0
#define lw__ 1
#else
#define hw__ 1
#define lw__ 0
#endif

static double R_ValueOfNA(void)
{
    /* The gcc shipping with RedHat 9 gets this wrong without
     * the volatile declaration. Thanks to Marc Schwartz. */
    volatile ieee_double x;
    x.word[hw__] = 0x7ff00000;
    x.word[lw__] = 1954;
    return x.value;
}

RAPI_CALL int R_IsNA(double x)
{
    if (isnan(x)) {
        ieee_double y;
        y.value = x;
        return (y.word[lw__] == 1954);
    }
    return 0;
}

RAPI_CALL int R_IsNaN(double x)
{
    if (isnan(x)) {
        ieee_double y;
        y.value = x;
        return (y.word[lw__] != 1954);
    }
    return 0;
}

RAPI_CALL int R_finite(double x)
{
#if 1
    return isfinite(x);
#else
    return (!isnan(x) & (x != R_PosInf) & (x != R_NegInf));
#endif
}

#ifdef MAIN__
static void init_Rcompat() {
    R_ClassSymbol = install("class");
    R_SrcfileSymbol = install("srcfile");
    R_SrcrefSymbol = install("srcref");
    R_MissingArg = allocObject(nullClass); /* it is a NULL object but not the same as nullObject */
    R_NaString = mkChar("NA");

    R_NaInt = INT_MIN;
    R_NaN = 0.0/R_Zero_Hack;
    R_NaReal = R_ValueOfNA();
    R_PosInf = 1.0/R_Zero_Hack;
    R_NegInf = -1.0/R_Zero_Hack;  
    
    /* set TYPEOF value to most basic classes */
    listClass->flags |= VECSXP;
    stringClass->flags |= STRSXP;
    charClass->flags |= CHARSXP;
    realClass->flags |= REALSXP;
    integerClass->flags |= INTSXP;
    pairlistClass->flags |= LISTSXP;
    langClass->flags |= LANGSXP;
    complexClass->flags |= CPLXSXP;
    logicalClass->flags |= LGLSXP;
    symbolClass->flags |= SYMSXP;
    objectClass->flags |= S4SXP;
}
#endif

#endif

