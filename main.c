#define MAIN__

#include "aleph.h"
#include "Rcompat.h"

static int aleph_initialized = 0;

int alephInitialize() {
    if (aleph_initialized) return 0;

    ON_ERROR {
	fprintf(stderr, "Error ocurred while initializing Aleph.\n");
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
    
    /* define most basic classes that are not directly definable due to cycles */
    charClass = subclass(objectClass, "characterString", NULL, NULL); /* this one doesn't really exist in R */
    symbol_t vectorAttrs[2] = { newSymbol("names"), 0 };
    vectorClass = subclass(objectClass, "vector", vectorAttrs, NULL); /* we cannot specify type because character class doesn't exist yet */
    stringClass = subclass(vectorClass, "character", NULL, NULL);
    vectorClass->attr_classes[0] = stringClass; /* fix up class for "names" now that we have defined "character" class */
    stringClass->attr_classes[0] = stringClass; /* the fixup is needed in both class object */
    symbol_t pairlistAttrs[4] = { newSymbol("next"), newSymbol("head"), newSymbol("tag"), 0 };
    pairlistClass = subclass(objectClass, "pairlist", pairlistAttrs, NULL);
    pairlistClass->attr_classes[0] = pairlistClass; /* "next" is recursive */

    /* remaining vector classes (we could really do that in Aleph code)  */
    langClass = subclass(pairlistClass, "language", NULL, NULL);
    numericClass = subclass(vectorClass, "numeric", NULL, NULL);
    realClass = subclass(numericClass, "real", NULL, NULL);
    integerClass = subclass(numericClass, "integer", NULL, NULL);
    listClass = subclass(vectorClass, "list", NULL, NULL);
    logicalClass = subclass(vectorClass, "logical", NULL, NULL);
    complexClass = subclass(vectorClass, "complex", NULL, NULL);
    
    /* initialize R compatibility code */
    init_Rcompat();
    return 0;
}

/* from gram.y */
SEXP parsingTest(FILE *f);

int main(int argc, char **argv) {
    if (alephInitialize())
	return 1;
    
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

