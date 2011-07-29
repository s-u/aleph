#include "aleph.h"


/* this is a hack until we have proper dispatch */
AObject *coerce(AObject *obj, AClass *cls) {
  if (CLASS(obj) == cls) return obj;
  if (cls == realClass) {
    if (CLASS(obj) == integerClass) {
      vlen_t n = LENGTH(obj), i;
      AObject *res = allocRealVector(n);
      double *d = REAL(res);
      int *s = INTEGER(obj);
      for (i = 0; i < n; i++) d[i] = (double) s[i];
      return res;
    }
  }
  A_error("no method to coerce '%s' into '%s'", CLASS(obj), cls);
  return nullObject;
}

/* some very basic arithmetics */
AObject *fn_add(AObject *args, AObject *where) {
    AObject *left = getAttr(args, AS_head), *right;
    args = getAttr(args, AS_next);
    left = eval(left, where);
    if (args == nullObject) /* unary + */
	return left;
    right = eval(getAttr(args, AS_head), where);
    /* FIXME: eventually this will use method dispatch ... */
    if (CLASS(left) != CLASS(right)) {
	if (CLASS(left) == integerClass && CLASS(right) == realClass)
	    left = coerce(left, realClass);
	else if (CLASS(left) == realClass && CLASS(right) == integerClass)
	    right = coerce(right, realClass);
	else
	    A_error("no method for '%s' + '%s'", className(left), className(right));
    }
    if (CLASS(left) == CLASS(right)) {
	if (CLASS(left) == realClass) {
	    double *a = REAL(left);
	    double *b = REAL(right);
	    vlen_t m = LENGTH(left), n = LENGTH(right), k = (m >= n) ? m : n, i;
	    AObject *res = allocRealVector(k);
	    double *c = REAL(res);
	    for (i = 0; i < k; i++) c[i] = a[i % m] + b[i % n];
	    return res;
	} else if (CLASS(left) == integerClass) {
	    int *a = INTEGER(left);
	    int *b = INTEGER(right);
	    vlen_t m = LENGTH(left), n = LENGTH(right), k = (m >= n) ? m : n, i;
	    AObject *res = allocIntVector(k);
	    int *c = INTEGER(res);
	    for (i = 0; i < k; i++) c[i] = a[i % m] + b[i % n];
	    return res;
	}
    }
    A_error("no method for '%s' + '%s'", className(left), className(right));
    return nullObject;
}


AObject *fn_seq(AObject *args, AObject *where) {
    vdiff_t s0, s1, step = 1;
    vlen_t n, i;
    AObject *left = getAttr(args, AS_head), *right, *res;
    args = getAttr(args, AS_next);
    left = eval(left, where);
    if (args == nullObject) A_error("missing argument");
    right = eval(getAttr(args, AS_head), where);
    if (LENGTH(left) != 1 || LENGTH(right) != 1) A_error("both arguments must have the length 1");
    if (CLASS(left) == realClass)
	s0 = (vlen_t) (REAL(left)[0] + 0.5);
    else if (CLASS(left) == integerClass)
	s0 = INTEGER(left)[0];
    else A_error("no method for '%s' : '%s'", className(left), className(right));
    if (CLASS(right) == realClass)
	s1 = (vlen_t) (REAL(right)[0] + 0.5);
    else if (CLASS(right) == integerClass)
	s1 = INTEGER(right)[0];
    else A_error("no method for '%s' : '%s'", className(left), className(right));
    n = (s0 < s1) ? (s1 - s0) : (s0 - s1);
    n++;
    if (s1 < s0) step = -1;
    res = allocIntVector(n);
    int *rv = INTEGER(res);
    for (i = 0; i < n; i++, s0 += step)
	rv[i] = s0;
    return res;
}
