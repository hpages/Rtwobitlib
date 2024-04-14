#include "twobit_seqstats.h"

#include <kent/twoBit.h>


/****************************************************************************
 * C_get_twobit_seqstats()
 */

static const char *stats_colnames[] = {"seqlengths", "A", "C", "G", "T", "N"};
static const int stats_ncol = sizeof(stats_colnames) / sizeof(char *);

static SEXP make_stats_dimnames(SEXP rownames)
{
	SEXP dimnames, colnames, colname;
	int j;

	dimnames = PROTECT(NEW_LIST(2));
	SET_VECTOR_ELT(dimnames, 0, rownames);
	colnames = PROTECT(NEW_CHARACTER(stats_ncol));
	SET_VECTOR_ELT(dimnames, 1, colnames);
	UNPROTECT(1);
	for (j = 0; j < stats_ncol; j++) {
		colname = PROTECT(mkChar(stats_colnames[j]));
		SET_STRING_ELT(colnames, j, colname);
		UNPROTECT(1);
	}
	UNPROTECT(1);
	return dimnames;
}

/* --- .Call ENTRY POINT --- */
SEXP C_get_twobit_seqstats(SEXP filepath)
{
	SEXP path, ans, ans_rownames, seqname, ans_dimnames;
	struct twoBitFile *tbf;
	struct twoBitIndex *tbi;
	int ans_nrow, i;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");
	tbf = twoBitOpen(CHAR(path));
	ans_nrow = tbf->seqCount;
	ans = PROTECT(allocMatrix(INTSXP, ans_nrow, stats_ncol));
	ans_rownames = PROTECT(NEW_CHARACTER(ans_nrow));
	for (i = 0, tbi = tbf->indexList; i < ans_nrow; i++, tbi = tbi->next) {
		if (tbi == NULL) {  /* should never happen */
			twoBitClose(&tbf);
			error("Rtwobitlib internal error in "
			      "C_get_twobit_seqstats():\n"
			      "    tbi == NULL");
		}
		seqname = PROTECT(mkChar(tbi->name));
		SET_STRING_ELT(ans_rownames, i, seqname);
		UNPROTECT(1);
		INTEGER(ans)[i] = twoBitSeqSize(tbf, tbi->name);
	}
	twoBitClose(&tbf);

	ans_dimnames = PROTECT(make_stats_dimnames(ans_rownames));
	SET_DIMNAMES(ans, ans_dimnames);
	UNPROTECT(3);
	return ans;
}


/****************************************************************************
 * C_get_twobit_seqlengths()
 */

/* --- .Call ENTRY POINT --- */
SEXP C_get_twobit_seqlengths(SEXP filepath)
{
	SEXP path, ans, ans_names, seqname;
	struct twoBitFile *tbf;
	struct twoBitIndex *tbi;
	int ans_len, i;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");
	tbf = twoBitOpen(CHAR(path));
	ans_len = tbf->seqCount;
	ans = PROTECT(NEW_INTEGER(ans_len));
	ans_names = PROTECT(NEW_CHARACTER(ans_len));
	SET_NAMES(ans, ans_names);
	UNPROTECT(1);
	for (i = 0, tbi = tbf->indexList; i < ans_len; i++, tbi = tbi->next) {
		if (tbi == NULL) {  /* should never happen */
			twoBitClose(&tbf);
			error("Rtwobitlib internal error in "
			      "C_get_twobit_seqlengths():\n"
			      "    tbi == NULL");
		}
		seqname = PROTECT(mkChar(tbi->name));
		SET_STRING_ELT(ans_names, i, seqname);
		UNPROTECT(1);
		INTEGER(ans)[i] = twoBitSeqSize(tbf, tbi->name);
	}
	twoBitClose(&tbf);
	UNPROTECT(1);
	return ans;
}

