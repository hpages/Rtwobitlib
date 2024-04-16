#ifndef _TWOBIT_ROUNDTRIP_H_
#define _TWOBIT_ROUNDTRIP_H_

#include <Rdefines.h>

SEXP C_twobit_write(SEXP x, SEXP filepath, SEXP use_long, SEXP skip_dups);

SEXP C_twobit_read(SEXP filepath);

#endif  /* _TWOBIT_ROUNDTRIP_H_ */

