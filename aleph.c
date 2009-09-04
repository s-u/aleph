#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

#define ALEPH 1

typedef unsigned int  vlen_t;
typedef unsigned long vsize_t;
typedef unsigned int  symbol_t;
typedef signed int    smapi_t;

/*=========================================================================================================*/

#pragma mark --- objects and classes ---

typedef struct AClass_s AClass;
typedef struct AObject_s AObject;

/* the object layout is very simple:
 0  # of attrs
 1  size (in excess of sizeof(AObject))
 2  length (in elements - for vector objects)
 3  class (zero-attribute)
 *  any further attributes
 *  data 
 
 the smallest object is 16 (32-bit) or 24 bytes (64-bit) long
 */

struct AObject_s {
    vlen_t attrs, len; /* number of attributes, length */
    vsize_t size;      /* size of the data portion (64-bit safe) */
    AObject *attr[1];  /* array of attributes - the first one is not counted in attrs and is the class object */
};

#define CLASS(O) ((AClass*)((O)->attr[0]))

/* number of superclasses that are stored directly int the class object. Since most classes have only one or two superclasses we store them directly for speed and use overflow array for special objects that have more superclasses. Note that superclass code needs to be also modified if this is touched. */
#define BUILTIN_SUPERCLASSES 3

struct AClass_s {
    AObject class_obj; /* dummy object such that classes can be used in object calls */
    const char *name;
    vlen_t attrs; /* attributes in instances */
    
    /* superclasses */
    vlen_t supers;
    AClass *super[BUILTIN_SUPERCLASSES]; /* first superclasses (so we don't have to do dual look-up for most objects) */
    AClass **more_super;
    
    /* mapping of symbols to attribute slots in instances */
    vlen_t attr_map_len;
    smapi_t *attr_map;
    AClass **attr_classes;

    /* class-level attributes (denoted by negative index in the map) */
    AObject **attr;

    vlen_t flags; /* currently used to simulate R types for now */
    
    /* low-level class functions - those are special functions that need to be really fast */
    AObject *(*copy)(AObject *);
    void *(*dataPtr)(AObject *);
    vlen_t (*length)(AObject *);
    AObject *(*eval)(AObject *, AObject *);
    AObject *(*call)(AObject *, AObject *, AObject *); /* fun, args, where (can be used to eval args if desired) */
};

/* most basic classes (we could cut down to just "class" and "object" since others could be defined in Aleph */
/* also note that we'll populate the class functions on init */
AClass classClass[1]  = { { { 0, sizeof(AClass) - sizeof(AObject), 0, { (AObject*) classClass } }, "class" } };
AClass objectClass[1] = { { { 0, 0, 0, { (AObject*) classClass } } , "object" } };
AClass nullClass[1] = { { { 0, 0, 0, { (AObject*) classClass } } , "null", 0, 1, { objectClass, NULL ,NULL } } };
AClass symbolClass[1] = { { { 0, 0, 0, { (AObject*) classClass } } , "symbol", 1, 1, { objectClass, NULL, NULL } } };

/* very crude error handling for now */
jmp_buf error_jmpbuf;

AObject *A_error(const char *fmt, ...) {
    va_list (ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    longjmp(error_jmpbuf, 1);
    return NULL;
}

AObject *A_warning(const char *fmt, ...) {
    va_list (ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return NULL;
}

#define NEW_CONTEXT if (setjmp(error_jmpbuf) == 0)
#define ON_ERROR if (setjmp(error_jmpbuf))

const char *className(AObject *o) {
    return CLASS(o)->name;
}

int isClass(AObject *o) { /* this is really obsolete now - it is from times when classes didn't have a class object */
    return CLASS(o) == classClass;
}

AClass *getClass(AObject *o) {
    return CLASS(o);
}

/** allocate variable-length objects (with data) */
AObject *allocVarObject(AClass *cl, vlen_t size, vlen_t len) {
    vlen_t a = cl->attrs;
    AObject *o = calloc(1, sizeof(AObject) + sizeof(AObject*) * a + size);
    o->attr[0] = (AObject*) cl;
    o->attrs = a;
    o->size = size;
    o->len = len;
    printf(" + alloc <%s %p> [%u/%lu/%u]\n", className(o), o, a, size, len);
    return o;
}

/** allocate fixed-length objets (no data) */
AObject *allocObject(AClass *cl) {
    vlen_t a = cl->attrs;
    AObject *o = calloc(1, sizeof(AObject) + sizeof(AObject*) * a);
    o->attr[0] = (AObject*) cl;
    o->attrs = a;
    printf(" + alloc <%s %p> [%u/no-data]\n", className(o), o, a);
    return o;
}

AObject *default_copy(AObject *obj) {
    vlen_t len = sizeof(AObject) + sizeof(AObject*) * obj->attrs + obj->size;
    AObject *o = (AObject*) malloc(len);
    memcpy(o, obj, len);
    return o;
}

AObject *default_nocopy(AObject *obj) {
    return obj;
}

#define DIRECT_DATAPTR(O) ((void*)&((O)->attr[(O)->attrs + 1]))

void *default_dataPtr(AObject *obj) {
    return DIRECT_DATAPTR(obj);
}

vlen_t default_length(AObject *obj) {
    return obj->len;
}

AObject *default_eval(AObject *obj, AObject *where) {
    return obj;
}

AObject *default_call(AObject *obj, AObject *args, AObject *where) {
    A_error("call to a non-function");
    return obj;
}

/* special NULL object */
AObject nullObject[1] = { { 0, 0, 0, { (AObject*) nullClass } } };


/* symbols (more precisely attribute names) - this is really hacky for now ... */
#define MAX_SYM 1024

/* FIXME: attributes should include type/class as well (but not in ASymbol) */
typedef struct ASymbol_s {
    AObject obj;
    char *name;
} ASymbol;

ASymbol symbol[MAX_SYM];
vlen_t symbols = 0;

symbol_t newSymbol(const char *name) {
    vlen_t i = 0;
    for (;i < symbols; i++)
	if (!strcmp(symbol[i].name, name))
	    return i;
    symbol[symbols].obj.attrs = 1;
    symbol[symbols].obj.size = sizeof(char *);
    symbol[symbols].obj.attr[0] = (AObject*) symbolClass;
    symbol[symbols].name = strdup(name);
    printf(" - new symbol: [%d] %s\n", symbols + 1, name);
    return symbols++;
}

const char *symbolName(symbol_t sym) {
    return (const char*) symbol[sym].name;
}

/* attributes handling */
AObject *getAttr(AObject *o, symbol_t sym) {
    AClass *c = CLASS(o);
    AObject *a;
    smapi_t ao;
    printf(" - getAttr <%s %p> %s\n", className(o), o, symbolName(sym));
    if (!c) return A_error("class objects have no attributes");
    if (sym == 1) return (AObject*) c; /* class attribute is special */
    if (sym >= c->attr_map_len) return A_error("object has no %s attribute", symbolName(sym));
    ao = c->attr_map[sym];
    if (!ao) return A_error("object has no %s attribute", symbolName(sym));
    a = (ao > 0) ? o->attr[sym] : c->attr[1 - ao];
    return a ? a : nullObject;
}

void setAttr(AObject *o, symbol_t sym, AObject *val) {
    AClass *c = CLASS(o);
    smapi_t ao;
    if (sym == 1) {
	A_error("currently assignment to the class attribute is not permitted");
    }
    printf(" - setAttr <%s %p> %s <%s %p>\n", className(o), o, symbolName(sym), className(val), val);
    if (!c) A_error("class objects have no attributes");
    if (sym >= c->attr_map_len) A_error("object has no %s attribute", symbolName(sym));
    ao = c->attr_map[sym];
    if (!ao) A_error("object has no %s attribute", symbolName(sym));
    // FIXME: mem mgmt
    if (ao > 0)
	o->attr[sym] = val;
    else
	c->attr[1 - ao] = val;
}

/* creating subclasses (currently only single inheritance is implmeneted) */
AClass *subclass(AClass *cl, const char *name, symbol_t *new_attributes, AClass **new_classes) {
    AClass *nc = (AClass*) calloc(1, sizeof(AClass));
    nc->class_obj.attr[0] = (AObject*) classClass;
    nc->name = strdup(name);
    symbol_t highest_sym = cl->attr_map_len, ca = cl->attrs, *a;
    if (new_attributes) {
	vlen_t new_atts = 0;
	/* find the necessary size of the attribute map */
	a = new_attributes;
	while (*a) {
	    if (*a > highest_sym) highest_sym = *a;
	    a++; new_atts++;
	}
	/* copy superclass' attr map */
	nc->attr_map = (smapi_t*) calloc(highest_sym + 1, sizeof(smapi_t));
	nc->attr_map_len = highest_sym + 1;
	if (cl->attr_map_len)
	    memcpy(nc->attr_map, cl->attr_map, sizeof(smapi_t) * cl->attr_map_len);
	nc->attr_classes = malloc((cl->attrs + new_atts) * sizeof(AClass*));
	if (cl->attr_classes)
	    memcpy(nc->attr_classes, cl->attr_classes, sizeof(AClass*) * cl->attrs);
	if (new_classes)
	    memcpy(nc->attr_classes + cl->attrs, new_classes, sizeof(AClass*) * new_atts);
	else {
	    vlen_t i;
	    for (i = 0; i < new_atts; i++)
		nc->attr_classes[i + cl->attrs] = objectClass;
	}
    } else {
	nc->attr_map = cl->attr_map;
	nc->attr_map_len = cl->attr_map_len;
	nc->attr_classes = cl->attr_classes;
    }
    /* add new attributes to the map */
    if (new_attributes) {
	a = new_attributes;
	while (*a) {
	    nc->attr_map[*a] = ++ca;
	    a++;
	}
    }
    nc->attrs = ca;
  
    /* we should really memcpy those .. */
    nc->copy = cl->copy;
    nc->length = cl->length;
    nc->dataPtr = cl->dataPtr;
    nc->eval = cl->eval;
    nc->call = cl->call;

    return nc;
}

/** is assignable from class cc to class cl */
int isAssignableClass(AClass *cc, AClass *cl) {
    if (cc == cl) return 1;
    if (cc->supers == 0) return 0;
    if (isAssignableClass(cc->super[0], cl)) return 1;
    if (cc->supers > 1 && isAssignableClass(cc->super[1], cl)) return 1;
    if (cc->supers > 2 && isAssignableClass(cc->super[2], cl)) return 1;
    {
	vlen_t i = 0, n = cc->supers - BUILTIN_SUPERCLASSES;
	for (; i < n; i++)
	    if (isAssignableClass(cc->more_super[i], cl)) return 1;
    }
    return 0;
}

/** just a shorthand for isAssignableClass(CLASS(object), class) */
int isAssignable(AObject *o, AClass *cl) {
    return isAssignableClass(CLASS(o), cl);
}

#define ADataPtr(O) (CLASS(O)->dataPtr(O))

#define LENGTH(X) (CLASS(X)->length(X))
#define DATAPTR(X) ADataPtr(X)

/*=========================================================================================================*/
/* derived basic data types and R compatibility macros */

typedef int bool_t; /* FIXME: logical is now 4 byte for compatibility, but we'll want to use 1 byte or so later ... */

AClass *vectorClass, *numericClass, *realClass, *integerClass, *listClass, *charClass, *stringClass, *pairlistClass, *langClass, *complexClass, *logicalClass;

AObject *allocRealVector(vlen_t n) {
    return allocVarObject(realClass, sizeof(double) * n, n);
}

AObject *allocIntVector(vlen_t n) {
    return allocVarObject(integerClass, sizeof(int) * n, n);
}

AObject *allocLogicalVector(vlen_t n) {
    return allocVarObject(logicalClass, sizeof(bool_t) * n, n);
}

AObject *allocObjectVector(AClass *cl, vlen_t n) {
    return allocVarObject(cl, sizeof(AObject*) * n, n);
}

#define DIRECT_CDR(X) ((X)->attr[1])
#define DIRECT_CAR(X) ((X)->attr[2])
#define DIRECT_TAG(X) ((X)->attr[3])

AObject *consPairs(AClass *cl, AObject *car, AObject *cdr, AObject *tag) {
    AObject *o = allocObject(cl);
    DIRECT_CAR(o) = car;
    DIRECT_CDR(o) = cdr;
    DIRECT_TAG(o) = tag;
}

#define CONS(X,Y) consPairs(pairlistClass, X, Y, nullObject)
#define LCONS(X,Y) consPairs(langClass, X, Y, nullObject)

/* for now we map CAR/CDR/TAG to its direct counterparts */
#define CAR DIRECT_CAR
#define CDR DIRECT_CDR
#define TAG DIRECT_TAG
#define SET_TAG(X, Y) (DIRECT_TAG(X) = Y)
#define SETCAR(X, Y) (DIRECT_CAR(X) = Y)
#define SETCDR(X, Y) (DIRECT_CDR(X) = Y)

#define ASymbol2sym_t(name)  ((symbol_t) ((ASymbol*)(name) - symbol))

#define PRINTNAME(X) mkChar(symbolName(ASymbol2sym_t(X)))

AObject *mkChar(const char *str) {
    vlen_t len = strlen(str);
    AObject *o = allocVarObject(charClass, len + 1, len);
    memcpy(DIRECT_DATAPTR(o), str, len + 1);
    return o;
}

AObject *mkCharLen(const char *str, vlen_t len) {
    AObject *o = allocVarObject(charClass, len + 1, len);
    char *c = (char*) DIRECT_DATAPTR(o);
    memcpy(c, str, len);
    c[len] = 0;
    return o;
}

#define mkCharLenCE(S, L, E) mkCharLen(S, L)

typedef struct {
    double r;
    double i;
} complex_t;

AObject *allocComplexVector(vlen_t n) {
    return allocVarObject(complexClass, sizeof(complex_t) * n, n);
}

#define REAL(X) ((double*)ADataPtr(X))
#define INTEGER(X) ((int*)ADataPtr(X))
#define LOGICAL(X) ((bool_t*)ADataPtr(X))
#define COMPLEX(X) ((complex_t*)ADataPtr(X))
#define CHAR(X) ((const char*)ADataPtr(X))

#define GET_VECTOR_ELT(X,I) (((AObject**)ADataPtr(X))[I])
#define SET_VECTOR_ELT(X,I,V) ((AObject**)ADataPtr(X))[I] = V
#define GET_STRING_ELT GET_VECTOR_ELT
#define STRING_ELT GET_VECTOR_ELT
#define SET_STRING_ELT SET_VECTOR_ELT

AObject *mkString(const char *str) {
    AObject *o = allocObjectVector(stringClass, 1);
    SET_STRING_ELT(o, 0, mkChar(str));
    return o;
}

AObject *ScalarReal(double val) {
    AObject *o = allocRealVector(1);
    REAL(o)[0] = val;
    return o;
}

AObject *ScalarInteger(int val) {
    AObject *o = allocIntVector(1);
    INTEGER(o)[0] = val;
    return o;
}

AObject *ScalarLogical(bool_t val) {
    AObject *o = allocIntVector(1);
    INTEGER(o)[0] = val;
    return o;
}

void PrintValue(AObject *obj) {
    if (!obj)
	printf("AObject<0x0!>\n"); 
    else if(obj == nullObject)
	printf("NULL object\n");
    else
	printf("AObject<%p>, class=%s, attrs=%u, size=%lu, len=%u\n", obj, className(obj), obj->attrs, obj->size, obj->len);
}

/* primitive methods (e.g. length) - could be class-level attributes or (probably better for speed) direct function pointers */
/* question: namespaces? we have a list of attributes, do we need separation - e.g. primitives, attributes, ... ? */

/* TODO: functions, methods ... */

AObject *symbol_eval(AObject *obj, AObject *where) {
    ASymbol *sym = (ASymbol*) obj;
    /* TODO: look up symbol */
    A_error("symbol %s is undefined", sym->name);
    return nullObject;
}

#define R_NilValue nullObject

#define isNull(X) ((X) == nullObject)

#define install(C) ((AObject*)&symbol[newSymbol(C)])

/*=========================================================================================================*/

#ifndef NO_MAIN

static AObject *parsingTest(FILE *f);
static void init_Rcompat();

int main(int argc, char **argv) {
    ON_ERROR {
	fprintf(stderr, "Error ocurred, quitting.\n");
	return 1;
    }
    
    printf("sizeof(AObject) = %u, sizeof(AClass) = %u\n", (unsigned int) sizeof(AObject), (unsigned int) sizeof(AClass));
    
    newSymbol(""); /* symbol at index 0 is the empty symbol - it is interpreted as no symbol in maps */
    newSymbol("class"); /* symbol #1 is the class */
    
    /* adjust class to have object as its superclass - we can't do that statically since there is a loop */
    classClass->supers = 1;
    classClass->super[0] = objectClass;
    
    /* set low-level functions for the most basic classes that we created statically */
    symbolClass->copy = classClass->copy = objectClass->copy = default_copy;
    nullClass->copy = symbolClass->copy = default_nocopy;
    nullClass->length = symbolClass->length = classClass->length = objectClass->length = default_length;
    nullClass->dataPtr = symbolClass->dataPtr = classClass->dataPtr = objectClass->dataPtr = default_dataPtr;
    nullClass->eval = classClass->eval = objectClass->eval = default_eval;
    symbolClass->eval = symbol_eval;
    nullClass->call = symbolClass->call = classClass->call = objectClass->call = default_call;
    
    /* define most basic classes (in fact we could do that in Aleph code alone - save for the "names" property class) */
    charClass = subclass(objectClass, "characterString", NULL, NULL); /* this one doesn't really exist in R */
    symbol_t vectorAttrs[2] = { newSymbol("names"), 0 };
    vectorClass = subclass(objectClass, "vector", vectorAttrs, NULL); /* we cannot specify type because character class doesn't exist yet */
    stringClass = subclass(vectorClass, "character", NULL, NULL);
    vectorClass->attr_classes[0] = stringClass; /* fix up class for "names" now that we have defined "character" class */
    stringClass->attr_classes[0] = stringClass; /* the fixup is needed in both class object */
    /* remaining vector classes */
    numericClass = subclass(vectorClass, "numeric", NULL, NULL);
    realClass = subclass(numericClass, "real", NULL, NULL);
    integerClass = subclass(numericClass, "integer", NULL, NULL);
    listClass = subclass(vectorClass, "list", NULL, NULL);
    logicalClass = subclass(vectorClass, "logical", NULL, NULL);
    complexClass = subclass(vectorClass, "complex", NULL, NULL); /* FIXME: this is for compatibility with R, but maybe a class with Re Im attrs may make more sense .. */
    
    symbol_t pairlistAttrs[4] = { newSymbol("next"), newSymbol("head"), newSymbol("tag"), 0 };
    pairlistClass = subclass(objectClass, "pairlist", pairlistAttrs, NULL);
    pairlistClass->attr_classes[0] = pairlistClass; /* "next" is recursive */
    langClass = subclass(pairlistClass, "language", NULL, NULL);
    
    /* initialize R compatibility code */
    init_Rcompat();

    AObject *str = mkString("foo");
    
    AObject *obj = allocRealVector(10);
    double *d = REAL(obj);
    int i, n = LENGTH(obj);
    for (i = 0; i < n; i++)
	d[i] = (double) i / 2.0;
    
    PrintValue(GET_VECTOR_ELT(str, 0));
    
    PrintValue(obj);
    PrintValue(getAttr(obj, newSymbol("names")));
    PrintValue(getAttr(obj, newSymbol("class")));
    
    PrintValue((AObject*) &symbol[newSymbol("class")]);
    
    FILE *f = fopen("test.R", "r");
    if (!f) { fprintf(stderr, "ERROR: cannot open test.R for reading\n"); return 1; }

    AObject *p = parsingTest(f); 
    PrintValue(p);
    
    return 0;
}

#endif
