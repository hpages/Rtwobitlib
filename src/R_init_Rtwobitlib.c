#include <R_ext/Rdynload.h>

#include "twobit_seqlengths.h"

#define CALLMETHOD_DEF(fun, numArgs) {#fun, (DL_FUNC) &fun, numArgs}

static const R_CallMethodDef callMethods[] = {
	CALLMETHOD_DEF(C_get_twobit_seqlengths, 1),
	{NULL, NULL, 0}
};

void R_init_Rtwobitlib(DllInfo *info)
{
	R_registerRoutines(info, NULL, callMethods, NULL, NULL);
	R_useDynamicSymbols(info, 0);
	return;
}

