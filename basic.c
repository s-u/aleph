#include "aleph.h"

/* assignment */
AObject *fn_assign(AObject *args, AObject *where) {
    AObject *name = getAttr(args, newSymbol("head")), *value;
    const char *name_str;
    args = getAttr(args, newSymbol("next"));
    value = getAttr(args, newSymbol("head"));
    if (CLASS(name) == symbolClass)
	name_str = ((ASymbol*)name)->name;
    else A_error("LHS of an assignment is not a symbol");
    if (value != nullObject)
	value = eval(value, where);
    symbol_set(ASymbol2sym_t(name), value, where);
    return value;
}
