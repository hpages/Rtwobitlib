#include <R_ext/Rdynload.h>

void R_init_Rtwobitlib(DllInfo *info)
{
	//R_registerRoutines(info, NULL, callMethods, NULL, NULL);
	R_useDynamicSymbols(info, 0);
	return;
}

