#ifndef ALEPH_H__
#define ALEPH_H__

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

#define ALEPH 1


/*=========================================================================================================*/

#define API_VAR extern
#define API_CALL static
#define API_FN API_CALL
#define GHVAR extern

#pragma mark --- objects and classes ---

#include "types.h"

#define CLASS(O) ((AClass*)((O)->attr[0]))

/* most basic classes (we could cut down to just "class" and "object" since others could be defined in Aleph */
/* also note that we'll populate the class functions on init */
API_VAR AClass classClass[1], objectClass[1];
API_VAR AClass nullClass[1], symbolClass[1];

/* very crude error handling for now */
GHVAR jmp_buf error_jmpbuf;

API_CALL AObject *A_error(const char *fmt, ...) {
    va_list (ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    longjmp(error_jmpbuf, 1);
    return NULL;
}

API_CALL AObject *A_warning(const char *fmt, ...) {
    va_list (ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return NULL;
}

#define NEW_CONTEXT if (setjmp(error_jmpbuf) == 0)
#define ON_ERROR if (setjmp(error_jmpbuf))

API_CALL const char *className(AObject *o) {
    return CLASS(o)->name;
}

API_CALL int isClass(AObject *o) { /* this is really obsolete now - it is from times when classes didn't have a class object */
    return CLASS(o) == classClass;
}

API_CALL AClass *getClass(AObject *o) {
    return CLASS(o);
}

/** allocate variable-length objects (with data) */
API_CALL AObject *allocVarObject(AClass *cl, vsize_t size, vlen_t len) {
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
API_CALL AObject *allocObject(AClass *cl) {
    vlen_t a = cl->attrs;
    AObject *o = calloc(1, sizeof(AObject) + sizeof(AObject*) * a);
    o->attr[0] = (AObject*) cl;
    o->attrs = a;
    printf(" + alloc <%s %p> [%u/no-data]\n", className(o), o, a);
    return o;
}

API_FN AObject *default_copy(AObject *obj) {
    vlen_t len = sizeof(AObject) + sizeof(AObject*) * obj->attrs + obj->size;
    AObject *o = (AObject*) malloc(len);
    memcpy(o, obj, len);
    return o;
}

API_FN AObject *default_nocopy(AObject *obj) {
    return obj;
}

#define DIRECT_DATAPTR(O) ((void*)&((O)->attr[(O)->attrs + 1]))

API_FN void *default_dataPtr(AObject *obj) {
    return DIRECT_DATAPTR(obj);
}

API_FN vlen_t default_length(AObject *obj) {
    return obj->len;
}

API_FN AObject *default_eval(AObject *obj, AObject *where) {
    return obj;
}

API_FN AObject *default_call(AObject *obj, AObject *args, AObject *where) {
    A_error("call to a non-function");
    return obj;
}

/* special NULL object */
API_VAR AObject nullObject[1];

/* symbols (more precisely attribute names) - this is really hacky for now ... */
#define MAX_SYM 1024

/* FIXME: attributes should include type/class as well (but not in ASymbol) */
GHVAR ASymbol symbol[MAX_SYM];
GHVAR vlen_t symbols;

API_CALL symbol_t newSymbol(const char *name) {
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

API_CALL const char *symbolName(symbol_t sym) {
    return (const char*) symbol[sym].name;
}

/* attributes handling */
API_CALL AObject *getAttr(AObject *o, symbol_t sym) {
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

API_CALL void setAttr(AObject *o, symbol_t sym, AObject *val) {
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
API_CALL AClass *subclass(AClass *cl, const char *name, symbol_t *new_attributes, AClass **new_classes) {
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
API_CALL int isAssignableClass(AClass *cc, AClass *cl) {
    if (cc == cl) return 1;
    if (cc->supers == 0) return 0;
    if (isAssignableClass(cc->super[0], cl)) return 1;
    if (cc->supers > 1 && isAssignableClass(cc->super[1], cl)) return 1;
    if (cc->supers > 2 && isAssignableClass(cc->super[2], cl)) return 1;
    if (cc->supers > 3) {
	vlen_t i = 0, n = cc->supers - BUILTIN_SUPERCLASSES;
	for (; i < n; i++)
	    if (isAssignableClass(cc->more_super[i], cl)) return 1;
    }
    return 0;
}

/** just a shorthand for isAssignableClass(CLASS(object), class) */
API_CALL int isAssignable(AObject *o, AClass *cl) {
    return isAssignableClass(CLASS(o), cl);
}

#define ADataPtr(O) (CLASS(O)->dataPtr(O))

#define LENGTH(X) (CLASS(X)->length(X))
#define DATAPTR(X) ADataPtr(X)

/*=========================================================================================================*/
/* derived basic data types and R compatibility macros */

API_VAR AClass *vectorClass, *numericClass, *realClass, *integerClass, *listClass, *charClass;
API_VAR AClass *stringClass, *pairlistClass, *langClass, *complexClass, *logicalClass;

API_CALL AObject *allocRealVector(vlen_t n) {
    return allocVarObject(realClass, sizeof(double) * n, n);
}

API_CALL AObject *allocIntVector(vlen_t n) {
    return allocVarObject(integerClass, sizeof(int) * n, n);
}

API_CALL AObject *allocLogicalVector(vlen_t n) {
    return allocVarObject(logicalClass, sizeof(bool_t) * n, n);
}

API_CALL AObject *allocObjectVector(AClass *cl, vlen_t n) {
    return allocVarObject(cl, sizeof(AObject*) * n, n);
}

#define DIRECT_CDR(X) ((X)->attr[1])
#define DIRECT_CAR(X) ((X)->attr[2])
#define DIRECT_TAG(X) ((X)->attr[3])

API_CALL AObject *consPairs(AClass *cl, AObject *car, AObject *cdr, AObject *tag) {
    AObject *o = allocObject(cl);
    DIRECT_CAR(o) = car;
    DIRECT_CDR(o) = cdr;
    DIRECT_TAG(o) = tag;
    return o;
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

API_CALL AObject *mkChar(const char *str) {
    vlen_t len = strlen(str);
    AObject *o = allocVarObject(charClass, len + 1, len);
    memcpy(DIRECT_DATAPTR(o), str, len + 1);
    return o;
}

API_CALL AObject *mkCharLen(const char *str, vlen_t len) {
    AObject *o = allocVarObject(charClass, len + 1, len);
    char *c = (char*) DIRECT_DATAPTR(o);
    memcpy(c, str, len);
    c[len] = 0;
    return o;
}

#define mkCharLenCE(S, L, E) mkCharLen(S, L)

API_CALL AObject *allocComplexVector(vlen_t n) {
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

API_CALL AObject *mkString(const char *str) {
    AObject *o = allocObjectVector(stringClass, 1);
    SET_STRING_ELT(o, 0, mkChar(str));
    return o;
}

API_CALL AObject *ScalarReal(double val) {
    AObject *o = allocRealVector(1);
    REAL(o)[0] = val;
    return o;
}

API_CALL AObject *ScalarInteger(int val) {
    AObject *o = allocIntVector(1);
    INTEGER(o)[0] = val;
    return o;
}

API_CALL AObject *ScalarLogical(bool_t val) {
    AObject *o = allocIntVector(1);
    INTEGER(o)[0] = val;
    return o;
}

API_CALL void PrintValue(AObject *obj) {
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

API_CALL AObject *symbol_eval(AObject *obj, AObject *where) {
    ASymbol *sym = (ASymbol*) obj;
    /* TODO: look up symbol */
    A_error("symbol %s is undefined", sym->name);
    return nullObject;
}

#define R_NilValue nullObject

#define isNull(X) ((X) == nullObject)

#define install(C) ((AObject*)&symbol[newSymbol(C)])

#endif
