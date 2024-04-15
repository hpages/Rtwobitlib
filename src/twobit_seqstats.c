#include "twobit_seqstats.h"

#include <kent/twoBit.h>
#include <kent/dnaseq.h>  /* for dnaSeqFree() */

#include <string.h>  /* for memset() */


/****************************************************************************
 * C_get_twobit_seqstats()
 */

static const char *stats_colnames[] = {"seqlengths", "A", "C", "G", "T", "N"};
static const int stats_ncol = sizeof(stats_colnames) / sizeof(char *);

static SEXP make_seqstats_dimnames(SEXP rownames)
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

/* Map 'a', 'c', 'g', 't', 'n' to 0, 1, 2, 3, 4 respectively. */
static inline int encode_dna_letter(unsigned char c)
{
	static const int map[] = {  0, -1,  1, -1, -1, -1,  2, -1, -1, -1,
				   -1, -1, -1,  4, -1, -1, -1, -1, -1,  3};

	if (c < 'a' || c > 't')
		return -1;
	return map[c - 'a'];
}

static int tabulate_sequence_letters(struct twoBitFile *tbf, char *name,
				     int *out, int out_nrow)
{
	struct dnaSeq *seq;
	int i, code;

	/* twoBitReadSeqFragExt() loads the sequence data in memory. */
	seq = twoBitReadSeqFragExt(tbf, name, 0, 0, FALSE, out);
	out += out_nrow;
	for (i = 0; i < seq->size; i++) {
		code = encode_dna_letter((unsigned char) seq->dna[i]);
		if (code < 0) {
			dnaSeqFree(&seq);
			return -1;
		}
		out[code * out_nrow]++;
	}
	dnaSeqFree(&seq);
	return 0;
}

/* --- .Call ENTRY POINT --- */
SEXP C_get_twobit_seqstats(SEXP filepath)
{
	SEXP path, ans, ans_rownames, seqname, ans_dimnames;
	struct twoBitFile *tbf;
	struct twoBitIndex *index;
	int ans_nrow, i, ret;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");
	tbf = twoBitOpen(CHAR(path));

	ans_nrow = tbf->seqCount;
	ans = PROTECT(allocMatrix(INTSXP, ans_nrow, stats_ncol));
	ans_rownames = PROTECT(NEW_CHARACTER(ans_nrow));
	ans_dimnames = PROTECT(make_seqstats_dimnames(ans_rownames));
	SET_DIMNAMES(ans, ans_dimnames);
	UNPROTECT(2);

	memset(INTEGER(ans), 0, sizeof(int) * XLENGTH(ans));
	for (i = 0, index = tbf->indexList;
	     i < ans_nrow;
	     i++, index = index->next)
	{
		if (index == NULL) {  /* should never happen */
			twoBitClose(&tbf);
			UNPROTECT(1);
			error("Rtwobitlib internal error in "
			      "C_get_twobit_seqstats():\n"
			      "    index == NULL");
		}
		seqname = PROTECT(mkChar(index->name));
		SET_STRING_ELT(ans_rownames, i, seqname);
		UNPROTECT(1);
		ret = tabulate_sequence_letters(tbf, index->name,
						INTEGER(ans) + i, ans_nrow);
		if (ret < 0) {
			twoBitClose(&tbf);
			UNPROTECT(1);
			error("DNA sequences in .2bit file contain "
			      "unrecognized letters");
		}
	}

	twoBitClose(&tbf);
	UNPROTECT(1);
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
	struct twoBitIndex *index;
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

	for (i = 0, index = tbf->indexList;
	     i < ans_len;
	     i++, index = index->next)
	{
		if (index == NULL) {  /* should never happen */
			twoBitClose(&tbf);
			UNPROTECT(1);
			error("Rtwobitlib internal error in "
			      "C_get_twobit_seqlengths():\n"
			      "    index == NULL");
		}
		seqname = PROTECT(mkChar(index->name));
		SET_STRING_ELT(ans_names, i, seqname);
		UNPROTECT(1);
		/* twoBitSeqSize() does not load the sequence data in memory. */
		INTEGER(ans)[i] = twoBitSeqSize(tbf, index->name);
	}

	twoBitClose(&tbf);
	UNPROTECT(1);
	return ans;
}

