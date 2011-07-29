#define MAIN__

#include "aleph.h"
#include "Rcompat.h"

static int aleph_initialized = 0;

typedef AObject *(*native_fn_ptr)(AObject *args, AObject *where);

/* native functions */
static AObject *native_fn_call(AObject *obj, AObject *args, AObject *where) {
    native_fn_ptr ptr = (native_fn_ptr) obj->attr[obj->attrs + 1];
    if (!ptr) A_error("Attempt to call a native function pointing to NULL");
    return ptr(args, where);
}

#include <dlfcn.h>

AClass *natFnClass;

static AObject *create_native_fn(AObject *args, AObject *where) {
    if (CLASS(args) != pairlistClass || CAR(args) == nullObject)
	A_error("'name' is missing in call to nativeFunction");
    const char *name = CHAR(STRING_ELT(CAR(args), 0));
    void *dl = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);
    if (!dl) A_error("cannot open dynamc library");
    void *addr = dlsym(dl, name);
    if (!addr) {
	dlclose(dl);
	A_error("unable to find symbol '%s'", name);
    }
    AObject *fn = allocVarObject(natFnClass, sizeof(void*), 0);
    setAttr(fn, newSymbol("formals"), CDR(args));
    setAttr(fn, newSymbol("environment"), where);
    fn->attr[fn->attrs + 1] = addr;
    return fn;
}

int alephInitialize() {
    if (aleph_initialized) return 0;

    ON_ERROR {
	fprintf(stderr, "Error ocurred while initializing Aleph.\n");
	return 1;
    }

    A_printf("----------------------------------------\n\n Aleph v0.0 (absolutely experimental)\n\n");

    gc_pool = newCustomPool(NULL, 1024*1024); /* create the garbage collector pool. It's large by default since we don't want to be allocating too many of them (but it could be probably smaller ...) */
    root_pool = newPool(); /* create the root pool */
    
    A_debug(ADL_info, "sizeof(AObject) = %u, sizeof(AClass) = %u", (unsigned int) sizeof(AObject), (unsigned int) sizeof(AClass));
    
    /* fix up pools for all static objects -- we are currently flagging constants with gc_pool even though they are not incuded it in, because objects with gc_pool are not freed */
    nullObject->pool = gc_pool;
    
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
    
    /* set pool to gc_pool for all static classes as constants */
    symbolClass->class_obj.pool = nullClass->class_obj.pool = classClass->class_obj.pool = objectClass->class_obj.pool = gc_pool;

    /* FIXME: this is a temporary definitiong of environments */
    symbol_t envAttrs[4] = { newSymbol("names"), newSymbol("values"), newSymbol("parent"), 0 };
    envClass = subclass(objectClass, "environment", envAttrs, NULL);
    envClass->copy = default_nocopy; /* reference semantics */
    
    /* define most basic classes that are not directly definable due to cycles */
    charClass = subclass(objectClass, "characterString", NULL, NULL); /* this one doesn't really exist in R */
    symbol_t vectorAttrs[2] = { AS_names = newSymbol("names"), 0 };
    vectorClass = subclass(objectClass, "vector", vectorAttrs, NULL); /* we cannot specify type because character class doesn't exist yet */
    stringClass = subclass(vectorClass, "character", NULL, NULL);
    vectorClass->attr_classes[0] = stringClass; /* fix up class for "names" now that we have defined "character" class */
    stringClass->attr_classes[0] = stringClass; /* the fixup is needed in both class object */
    symbol_t pairlistAttrs[4] = { AS_head = newSymbol("head"), AS_tag = newSymbol("tag"), AS_next = newSymbol("next"), 0 };
    pairlistClass = subclass(objectClass, "pairlist", pairlistAttrs, NULL);
    pairlistClass->attr_classes[0] = pairlistClass; /* "next" is recursive */

    /* remaining vector classes (we could really do that in Aleph code)  */
    langClass = subclass(pairlistClass, "language", NULL, NULL);
    langClass->eval = lang_eval;
    numericClass = subclass(vectorClass, "numeric", NULL, NULL);
    realClass = subclass(numericClass, "real", NULL, NULL);
    integerClass = subclass(numericClass, "integer", NULL, NULL);
    listClass = subclass(vectorClass, "list", NULL, NULL);
    logicalClass = subclass(vectorClass, "logical", NULL, NULL);
    complexClass = subclass(vectorClass, "complex", NULL, NULL);

    AClass *pointerClass = subclass(objectClass, "pointer", NULL, NULL);
    /* FIXME: this is a quick hack for experiments - we will need arguments (formals) and possibly other info */
    symbol_t natFnAttr[3] = { newSymbol("formals"), newSymbol("environment"), 0 };
    natFnClass = subclass(pointerClass, "nativeFunction", natFnAttr, NULL);
    natFnClass->call = native_fn_call;

    /* initialize R compatibility code */
    /* NOTE: this will create some objects in the root pool, so the root pool should never go away until you're done with R */
    init_Rcompat();
    return 0;
}

/* from gram.y */
SEXP parsingTest(FILE *f);

AObject *foo(AObject *args, AObject *where) {
    printf("foo has been invoked with: ");
    PrintValue(args);
    return mkString("foo");
}


int main(int argc, char **argv) {
    if (alephInitialize())
	return 1;

    ON_ERROR {
	fprintf(stderr, "Terminating due to a run-time error\n");
	return 1;
    }
    
    AllocationPool *pool = newPool();
    
    AObject *str = mkString("foo");
    
#ifdef ADEBUG
    /* some simple tests of the API */
    AObject *obj = allocRealVector(10);
    double *d = REAL(obj);
    int i, n = LENGTH(obj);
    for (i = 0; i < n; i++)
	d[i] = (double) i / 2.0;
    
    PrintValue(GET_VECTOR_ELT(str, 0));
    
    PrintValue(obj);
    //PrintValue(getAttr(obj, newSymbol("names")));
    //PrintValue(getAttr(obj, newSymbol("class")));
    
    //PrintValue((AObject*) &symbol[newSymbol("class")]);
#endif

    /* our evaluation environemnt */
    AObject *env = allocEnv();

    /* create one object - the constructor to native functions so we can create them */
    AObject *natFnConstr = allocVarObject(natFnClass, sizeof(native_fn_ptr), 1);
    natFnConstr->attr[natFnConstr->attrs + 1] = (AObject*) create_native_fn;
    symbol_set(newSymbol("nativeFunction"), natFnConstr, env);
    symbol_set(newSymbol("="), create_native_fn(list1(mkString("fn_assign")), env), env);

    /* PrintValue(env); */

    /* load initial bootstrap code if present */
    A_printf("--- Loading bootstrap code\n");
    FILE *f = fopen("init.R", "r");
    if (f) {
	while (!feof(f)) {
	    AObject *p = parsingTest(f);
	    if (p) eval(p, env);
	}
	fclose(f);
    }

    f = stdin;
    const char * fn = (argc < 2) ? "test.R" : argv[1];
    if (fn[0] != '-' || fn[1]) {
	A_printf("--- Read input from %s\n", fn);
	f = fopen(fn, "r");
    } else A_printf("--- Ready for input from the console\n");
    if (!f)
	fprintf(stderr, "ERROR: cannot open test.R for reading\n");
    else {
	while (1) {
	    A_debug(ADL_info, "-- parsing ...");
	    AObject *p = parsingTest(f);
	    A_debug(ADL_info, "-- parser result:");
#ifdef ADEBUG
	    PrintValue(p);
#endif
	    if (!p) break;
	    A_debug(ADL_info, "-- evaluate:");
	    NEW_CONTEXT
		p = eval(p, env);
	    A_debug(ADL_info, "-- result:");
	    PrintValue(p);
	}
    }

    A_printf("The local pool contains %d objects\n", pool->count);
    releasePool(pool);

    A_printf("The root pool contains %d objects\n", root_pool->count);
    releasePool(root_pool); /* remove the root pool */

    A_printf("The gc pool contains %d objects\n", gc_pool->count);
    releasePool(gc_pool); /* also remove the gc pool and thus all objects */

    return 0;
}

