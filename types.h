#ifndef ALEPH_TYPES_H__
#define ALEPH_TYPES_H__

/* most basic types used in Aleph */

typedef unsigned int  vlen_t;
typedef unsigned long vsize_t;
typedef unsigned int  symbol_t;
typedef signed int    smapi_t;

typedef struct {
    double r;
    double i;
} complex_t;

typedef int bool_t; /* FIXME: logical is now 4 byte for compatibility, but we'll want to use 1 byte or so later ... */

typedef struct AClass_s AClass;
typedef struct AObject_s AObject;
typedef struct ASymbol_s ASymbol;

typedef struct AllocationPool_s AllocationPool;

/*============== internals =============*/

/* the object layout is very simple: [[NOTE if changed, you must also update static class definitions!]]
 0  # of attrs
 1  length (in elements - for vector objects)
 2  pointer to the pool owning this obejct (or NULL is it is owned by a single other object)
 3  size (in excess of sizeof(AObject))
 4  class (zero-attribute)
 *  any further attributes
 *  data 
 
 the smallest object is 20 (32-bit) or 32 bytes (64-bit) long
 */

struct AObject_s {
    vlen_t attrs, len; /* number of attributes, length */
    AllocationPool *pool; /* this the allocation pool owning this object. */
    vsize_t size;      /* size of the data portion (64-bit safe) */
    AObject *attr[1];  /* array of attributes - the first one is not counted in attrs and is the class object */
};

struct ASymbol_s {
    AObject obj;
    char *name;
};

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
    /* -- maybe a destructor? -- */
};

#endif
