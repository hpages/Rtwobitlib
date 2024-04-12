#include "twobit_seqlengths.h"

#include <twobit/twoBit.h>

/* --- .Call ENTRY POINT --- */
SEXP C_get_twobit_seqlengths(SEXP filepath)
{
	SEXP path, ans, ans_names;
	struct twoBitFile *tbf;
	struct twoBitIndex *tbi;
	int ans_len, i;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");
	tbf = twoBitOpen(CHAR(path));
	tbi = tbf->indexList;
	ans_len = slCount(tbi);
	ans = PROTECT(NEW_INTEGER(ans_len));
	ans_names = PROTECT(NEW_CHARACTER(ans_len));
	SET_NAMES(ans, ans_names);
	UNPROTECT(1);
	for (i = 0; i < ans_len; i++) {
		if (tbi == NULL)
			error("Rtwobitlib internal error in "
			      "C_get_twobit_seqlengths():\n"
			      "    tbi == NULL");
		SET_STRING_ELT(ans_names, i, mkChar(tbi->name));
		INTEGER(ans)[i] = twoBitSeqSize(tbf, tbi->name);
		tbi = tbi->next;
	}
	twoBitClose(&tbf);
	UNPROTECT(1);
	return ans;
}

