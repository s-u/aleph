#ifndef METHODS_H__
#define METHODS_H__

/* this is a separate implementation of methods, but it is header file for efficiency */

/* for now we use C-struct for signatures, but we should try using AObjects -- if it
   doesn't perform much worse, it would be worth for consistency */
typedef struct method_sig {
    AClass  *cls;
    AObject *value;
    struct method_sig *next;
} method_sig_t;

typedef struct {
    AObject obj;
    method_sig_t sig[1];
} AMethod;

API_VAR AClass *methodClass;

static AObject *method_call(AObject *obj, AObject *args, AObject *where) {
    return obj;
}

HIDDEN_CALL void init_methods(AObject *env) {
    methodClass = subclass(objectClass, "method", NULL, NULL);
    methodClass->call = method_call;
}

static AObject *matchMethodArgClasses(AMethod *m, vlen_t sig, AObject *args) {
    if (args == nullObject && m->sig[sig].cls == NULL)
	return m->sig[sig].value;
    if (m->sig[sig].cls != NULL) { /* continue only if the signature has more */
	AObject *xa = getAttr(args, AS_head);
	AClass *xc = CLASS(xa);
	AObject *subargs = getAttr(args, AS_next);
	if (isAssignableClass(xc, m->sig[sig].cls))
	    return m->sig[sig].value;
	/* FIXME: continue to other sigs... */
    }
    if (sig < m->obj.len)
	return matchMethodArgClasses(m, sig + 1, args);
    return NULL;
}

static AObject *findMethodInClass(symbol_t sym, AClass *cls, AObject* args) {
    AMethod *m = (AMethod*) getClassAttr(cls, sym);
    AObject *value;
    if (m) {
	value = matchMethodArgClasses(m, 0, args);
	if (value)
	    return value;
    }
    /* class doesn't have the method, go on to superclasses */
    if (cls->supers == 0) return NULL;
    if ((value = findMethodInClass(sym, cls->super[0], args)) || cls->supers == 1) return value;
    if ((value = findMethodInClass(sym, cls->super[1], args)) || cls->supers == 2) return value;
    if ((value = findMethodInClass(sym, cls->super[2], args)) || cls->supers == 3) return value;
    if (cls->supers > 3) {
	vlen_t i = 0, n = cls->supers - BUILTIN_SUPERCLASSES;
	for (; i < n; i++)
	    if ((value = findMethodInClass(sym, cls->more_super[i], args))) return value;
    }
    return NULL;
}

API_CALL AObject *findMethod(const char *name, AObject *args) {
    AObject *head = getAttr(args, AS_head);
    AClass *cls = CLASS(head);
    args = getAttr(args, AS_next);
    char tmp[64];
    /* FIXME: check overflow */
    strcpy(tmp + 1, name);
    tmp[0] = 1;
    symbol_t sym = newSymbol(tmp);
    return findMethodInClass(sym, cls, args);    	    
}

API_CALL AObject *create_method(AObject *args, AObject *where) {
    AObject *name = getAttr(args, AS_head);
    args = getAttr(args, AS_next);
    return name;
}

#endif
