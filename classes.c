#include "types.h"

AClass classClass[1]  = { { { 0, sizeof(AClass) - sizeof(AObject), 0, 0, { (AObject*) classClass } }, "class" } };
AClass objectClass[1] = { { { 0, 0, 0, 0, { (AObject*) classClass } } , "object" } };
AClass nullClass[1]   = { { { 0, 0, 0, 0, { (AObject*) classClass } } , "null", 0, 1, { objectClass, 0, 0 } } };
AClass symbolClass[1] = { { { 0, 0, 0, 0, { (AObject*) classClass } } , "symbol", 1, 1, { objectClass, 0, 0 } } };

AObject nullObject[1] = { { 0, 0, 0, 0, { (AObject*) nullClass } } };

AClass *vectorClass, *numericClass, *realClass, *integerClass, *listClass, *charClass, *envClass;
AClass *stringClass, *pairlistClass, *langClass, *complexClass, *logicalClass;
