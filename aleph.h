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
#define HIDDEN_CALL static
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

/** ------ context ------- */

typedef struct ThreadContext_s {
    AutoreleasePool *pool;
} ThreadContext;

GHVAR ThreadContext mainThreadContext;

#define currentThreadContext() (&mainThreadContext)
#define currentPool() (currentThreadContext()->pool)

/** ------ memory management ------- */

/** this is the internal, low-level free call - it should never be made available to the outside code. This call is used to free an object that is already devoid of any references. */
HIDDEN_CALL void _freeObject(AObject *o) {
    /* FIXME: recursively delete ... destructor? */
    printf(" - freeing object <%p>\n", o);
    free(o);
}

/* Idea for implementation: use (stacked?) local allocation pools. Each object is owned by the pool. On assignment the object is moved from the pool (if it's the first assignment), otherwise nothing to do. Objects left in the pool are orphans and can be deleted. This means that we don't need explicit PROTECT/UNPROTECT as all objects are owned until the end of the call (if we wrap calls in pools). We could allow for an explicit release (another perk: in a debug version we could warn if the released object still has a parent but an optimized build could simply skip such checks). For this we may need something in the objects -- a flag that an object has not yet been assigned or alternatively a pointer to the primary owner (once it has multiple owners it can point to a special value or something... - in fact if everything is an obejct is should be the parent and we can check by class whether it's a pool...). The nice part of the latter would be that single-parent objects could be removed immediately (really we just want to know the local pool). [Mabe: a "movable" bit meaning that the object has only one owner] */

/** autorelease pools are currently outside of the object structure simply to allow linear dependency. It it was an object it would first need lists etc. defined before it could work, so we'd rather not go there ... */
struct AutoreleasePool_s {
    struct AutoreleasePool_s *prev, *next;
    vlen_t count, ptr, length;
    AObject *item[1];
};

GHVAR AutoreleasePool *gc_pool; /* garbage collector pool -- all garbage-collected objects live there */

/* Allocate a new autorelease pool with the given parent. This function does not affect the current pool. */
API_CALL AutoreleasePool *newCustomPool(AutoreleasePool *parent, vlen_t size) {
    AutoreleasePool *np = (AutoreleasePool*) calloc(1, sizeof(AutoreleasePool) + sizeof(AObject*) * size - sizeof(AObject*));
    if (!np) return (AutoreleasePool*) A_error("unable to allocate new memory pool for %d objects", size);
    np->length = size;
    np->prev = parent;
    if (parent) {
	if (parent->next) { /* if there is already a pool, insert instead */
	    np->next = parent->next;
	    np->next->prev = np;
	}
	parent->next = np;
    }
    printf(" + new autorelease pool <%p>, size=%d\n", np, size);
    return np;
}

API_CALL void releasePool(AutoreleasePool *pool) {
    vlen_t i = 0, n;
    if (!pool) return;
    if (pool->next) releasePool(pool->next);
    printf(" - releasing pool <%p> (count=%d)\n", pool, pool->count);
    if (pool->count) {
	/* all objects in the pool are only owned by the pool so we can free them directly */
	n = pool->length;
	while (i < n) {
	    if (pool->item[i])
		_freeObject(pool->item[i]);
	    i++;
	}
    }
    /* free the pool itself */
    if (pool->prev) {
	pool->prev->next = NULL;
	pool->prev = NULL;
    }
    free(pool);
}

#define DEFAULT_POOL_SIZE 256

/* create a new autorelease pool. All furhter local allocations will be placed in that pool */
API_CALL AutoreleasePool *newPool() {
    AutoreleasePool *cp = newCustomPool(currentPool(), DEFAULT_POOL_SIZE);
    currentThreadContext()->pool = cp;
    return cp;
}

API_CALL AObject *addObjectToPool(AObject *obj, AutoreleasePool *pool) {
    int is_gc = (pool == gc_pool);
    printf(" - move <%p> to pool <%p>(%d/%d,%d)%s\n", obj, pool, pool->count, pool->length, pool->ptr, is_gc ? " (gc_pool)" : "");
    while (pool->next && pool->count == pool->length) pool = pool->next; /* find some available pool ...*/
    if (pool->count == pool->length) /* or .. if there is none, create another pool (take the size from the parent) */
	pool = newCustomPool(pool, pool->length);
    pool->item[pool->ptr] = obj;
    pool->count++;
    if (pool->count != pool->length) /* update the ptr to the next empty spot in case there are any */
	while (pool->item[++(pool->ptr)]) {};
    /* FIXME: we may have to think about the gc_pool business some more ... */
    obj->pool = is_gc ? gc_pool: pool;
    return obj;
}

API_CALL AObject *removeObjectFromPool(AObject *obj, AutoreleasePool *pool) {
    if (pool) {
	vlen_t i = pool->ptr;
	printf(" - remove <%p> from pool <%p>(%d/%d,%d)\n", obj, pool, pool->count, pool->length, pool->ptr);
	/* fast-track heuristic -- if there were no holes we can optimize FILO by looking just before ptr */
	if (i) i--;
	if (pool->item[i] != obj) { /* if we didn't find it right away, start a full search */
	    vsize_t n = pool->length;
	    i = 0;
	    while (i < n) {
		if (pool->item[i] == obj) break;
		i++;
	    }
	    if (i == n) /* not found - this is a fatal error -- we should really supply some more info */
		return A_error("attempt to remove non-existing object from a pool");
	}
	pool->item[i] = 0;
	if (i < pool->ptr) pool->ptr = i;
	obj->pool = 0;
	pool->count--;
    }
    return obj;
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
    addObjectToPool(o, currentPool());
    return o;
}

/** allocate fixed-length objets (no data) */
API_CALL AObject *allocObject(AClass *cl) {
    vlen_t a = cl->attrs;
    AObject *o = calloc(1, sizeof(AObject) + sizeof(AObject*) * a);
    o->attr[0] = (AObject*) cl;
    o->attrs = a;
    printf(" + alloc <%s %p> [%u/no-data]\n", className(o), o, a);
    addObjectToPool(o, currentPool());
    return o;
}

API_FN AObject *default_copy(AObject *obj) {
    vlen_t len = sizeof(AObject) + sizeof(AObject*) * obj->attrs + obj->size;
    AObject *o = (AObject*) malloc(len);
    memcpy(o, obj, len);
    o->pool = NULL;
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

API_CALL AObject *set(AObject **ptr, AObject *val) {
    AObject *ov = ptr[0];
    if (ov != val) {
	ptr[0] = val;
	if (ov && ov->pool != gc_pool) /* if the value was not GC'd we can free it [since we owned it it can't be in a local pool - but we could add a sanity check] */
	    _freeObject(ov);
	if (val && val->pool != gc_pool) { /* if the value is not multi-owned, we have to adjust the pool */
	    if (val->pool) /* has a local pool -> moves from the pool to NULL state which is single ownership */
		removeObjectFromPool(val, val->pool);
	    else /* is already single-owned -> has to be moved to the gc pool */
		addObjectToPool(val, gc_pool);
	}
    }
    return val;
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
    if (ao > 0) { /* object-level attribute */
	AObject *ov = o->attr[sym];
	if (ov != val) {
	    o->attr[sym] = val;
	    if (ov && ov->pool != gc_pool) /* if the value was not GC'd we can free it [since we owned it it can't be in a local pool - but we could add a sanity check] */
		_freeObject(ov);
	    if (val->pool != gc_pool) { /* if the value is not multi-owned, we have to adjust the pool */
		if (val->pool) /* has a local pool -> moves from the pool to NULL state which is single ownership */
		    removeObjectFromPool(val, val->pool);
		else /* is already single-owned -> has to be moved to the gc pool */
		    addObjectToPool(val, gc_pool);
	    }
	}
    } else { /* class-level attribute */
	AObject *ov = c->attr[1 - ao];
	if (ov != val) { 
	    c->attr[1 - ao] = val;
	    if (ov && ov->pool != gc_pool) /* if the value was not GC'd we can free it [since we owned it it can't be in a local pool - but we could add a sanity check] */
		_freeObject(ov);
	    if (val->pool != gc_pool) { /* if the value is not multi-owned, we have to adjust the pool */
		if (val->pool) /* has a local pool -> moves from the pool to NULL state which is single ownership */
		    removeObjectFromPool(val, val->pool);
		else /* is already single-owned -> has to be moved to the gc pool */
		    addObjectToPool(val, gc_pool);
	    }
	}
    }
}

/* creating subclasses (currently only single inheritance is implmeneted) */
API_CALL AClass *subclass(AClass *cl, const char *name, symbol_t *new_attributes, AClass **new_classes) {
    AClass *nc = (AClass*) calloc(1, sizeof(AClass));
    nc->class_obj.attr[0] = (AObject*) classClass;
    nc->name = strdup(name);
    nc->class_obj.pool = gc_pool; /* FIXME: classes are currently considered constants so they are flagged with gc_pool even though they are not part of it. Maybe they should be subject to the usual memory management.. */
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
#define SET_TAG(X, Y) set(&DIRECT_TAG(X), Y)
#define SETCAR(X, Y) set(DIRECT_CAR(X), Y)
#define SETCDR(X, Y) set(DIRECT_CDR(X), Y)

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
#define SET_VECTOR_ELT(X,I,V) set(((AObject**)ADataPtr(X)) + (I), V)
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
