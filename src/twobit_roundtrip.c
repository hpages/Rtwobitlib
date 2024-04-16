#include "twobit_roundtrip.h"

#include <kent/twoBit.h>
#include <kent/dnaseq.h>  /* for dnaSeqFree() */


/* --- .Call ENTRY POINT --- */
SEXP C_twobit_write(SEXP x, SEXP filepath)
{
	return R_NilValue;
}

static SEXP load_sequence_as_CHARSXP(struct twoBitFile *tbf, char *name)
{
	struct dnaSeq *seq;
	int n;
	SEXP ans;

	/* twoBitReadSeqFragExt() loads the sequence data in memory. */
	seq = twoBitReadSeqFragExt(tbf, name, 0, 0, TRUE, &n);
	ans = PROTECT(mkCharLen(seq->dna, n));
	dnaSeqFree(&seq);
	UNPROTECT(1);
	return ans;
}

/* --- .Call ENTRY POINT --- */
SEXP C_twobit_read(SEXP filepath)
{
	SEXP path, ans, ans_names, tmp;
	struct twoBitFile *tbf;
	struct twoBitIndex *index;
	int ans_len, i;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");
	tbf = twoBitOpen(CHAR(path));

	ans_len = tbf->seqCount;
	ans = PROTECT(NEW_CHARACTER(ans_len));
	ans_names = PROTECT(NEW_CHARACTER(ans_len));
	SET_NAMES(ans, ans_names);
	UNPROTECT(1);

	for (i = 0, index = tbf->indexList;
	     i < ans_len;
	     i++, index = index->next)
	{
		if (index == NULL) {  /* should never happen */
			twoBitClose(&tbf);
			UNPROTECT(1);
			error("Rtwobitlib internal error in "
			      "C_twobit_read():\n"
			      "    index == NULL");
		}
		tmp = PROTECT(mkChar(index->name));
		SET_STRING_ELT(ans_names, i, tmp);
		UNPROTECT(1);
		tmp = PROTECT(load_sequence_as_CHARSXP(tbf, index->name));
		SET_STRING_ELT(ans, i, tmp);
		UNPROTECT(1);
	}

	twoBitClose(&tbf);
	UNPROTECT(1);
	return ans;
}

