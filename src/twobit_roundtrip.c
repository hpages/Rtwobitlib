#include "twobit_roundtrip.h"

#include <kent/hash.h> /* for newHash(), freeHash(), hashLookup(), hashAdd() */
#include <kent/dnautil.h>  /* for dnaUtilOpen() */
#include <kent/dnaseq.h>  /* for dnaSeqFree() */
#include <kent/twoBit.h>

#include <stdio.h>  /* for fopen(), fclose() */
#include <string.h>  /* for strerror() */


/****************************************************************************
 * C_twobit_write()
 */

static struct twoBit *make_twoBitList_from_STRSXP(SEXP x, boolean skip_dups)
{
	SEXP x_names, x_names_elt, x_elt;
	struct twoBit *twoBitList = NULL, *twoBit;
	struct hash *uniqHash = newHash(18);
	int i;
	const char *seqname;
	struct dnaSeq seq;

	x_names = GET_NAMES(x);
	for (i = 0; i < LENGTH(x); i++) {
		x_names_elt = STRING_ELT(x_names, i);
		if (x_names_elt == NA_STRING) {
			freeHash(&uniqHash);
			twoBitFreeList(&twoBitList);
			error("the names on 'x' cannot contain NAs");
		}
		seqname = CHAR(x_names_elt);
		x_elt = STRING_ELT(x, i);
		if (x_elt == NA_STRING) {
			freeHash(&uniqHash);
			twoBitFreeList(&twoBitList);
			error("'x' cannot contain NAs");
		}
		if (LENGTH(x_elt) == 0) {
			warning("sequence %s has length 0 ==> skipping it",
				seqname);
			continue;
		}
		if (hashLookup(uniqHash, seqname)) {
			if (skip_dups) {
				warning("duplicate sequence name %s "
					"==> skipping it", seqname);
				continue;
			}
			freeHash(&uniqHash);
			twoBitFreeList(&twoBitList);
			error("duplicate sequence name %s", seqname);
		}
		hashAdd(uniqHash, seqname, NULL);

		/* We discard the 'const' qualifier to avoid a compilation
		   warning. Safe to do here because twoBitFromDnaSeq() will
		   actually treat 'seq.dna' and 'seq.name' as 'const char *'. */
		seq.dna = (char *) CHAR(x_elt);
		seq.name = (char *) CHAR(x_names_elt);
		seq.size = LENGTH(x_elt);

		twoBit = twoBitFromDnaSeq(&seq, TRUE);
		slAddHead(&twoBitList, twoBit);
	}
	freeHash(&uniqHash);
	slReverse(&twoBitList);
	return twoBitList;
}

/* --- .Call ENTRY POINT --- */
SEXP C_twobit_write(SEXP x, SEXP filepath, SEXP skip_dups)
{
	SEXP path;
	struct twoBit *twoBitList, *twoBit;
	FILE *f;
	boolean useLong;

	path = STRING_ELT(filepath, 0);
	if (path == NA_STRING)
		error("'filepath' cannot be NA");

	dnaUtilOpen();

	/* Preprocess the data to write. */
	twoBitList = make_twoBitList_from_STRSXP(x, LOGICAL(skip_dups)[0]);

	/* Open destination file. */
	f = fopen(CHAR(path), "wb");
	if (f == NULL)
		error("cannot open %s to write: %s",
		      CHAR(path), strerror(errno));

	/* Write data to destination file. */
	/* TODO: Try useLong = TRUE for >4Gb assemblies */
	useLong = FALSE;
	twoBitWriteHeaderExt(twoBitList, f, useLong);
	for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
		twoBitWriteOne(twoBit, f);

	/* Close file and free memory. */
	fclose(f);
	twoBitFreeList(&twoBitList);
	return R_NilValue;
}


/****************************************************************************
 * C_twobit_read()
 */

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

