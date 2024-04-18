/* Obscure.h  - stuff that's relatively rarely used
 * but still handy. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef OBSCURE_H
#define OBSCURE_H

long incCounterFile(char *fileName);
/* Increment a 32 bit value on disk. */

int digitsBaseTwo(unsigned long x);
/* Return base two # of digits. */

int digitsBaseTen(int x);
/* Return number of digits base 10. */

void writeGulp(char *file, char *buf, int size);
/* Write out a bunch of memory. */

int countWordsInFile(char *fileName);
/* Count number of words in file. */

void copyFile(char *source, char *dest);
/* Copy file from source to dest. */

void copyOpenFile(FILE *inFh, FILE *outFh);
/* copy an open stdio file */

void cpFile(int s, int d);
/* Copy from source file to dest until reach end of file. */

void *charToPt(char c);
/* Convert char to pointer. Use when really want to store
 * a char in a pointer field. */

char ptToChar(void *pt);
/* Convert pointer to char.  Use when really want to store a
 * pointer in a char. */

void *intToPt(int i);
/* Convert integer to pointer. Use when really want to store an
 * int in a pointer field. */

int ptToInt(void *pt);
/* Convert pointer to integer.  Use when really want to store a
 * pointer in an int. */

void *sizetToPt(size_t i);
/* Convert size_t to pointer. Use when really want to store a
 * size_t in a pointer. */

size_t ptToSizet(void *pt);
/* Convert pointer to size_t.  Use when really want to store a
 * pointer in a size_t. */

char *makeEscapedString(char *in, char toEscape);
/* Return string that is a copy of in, but with all
 * toEscape characters preceded by '\' 
 * When done freeMem result. */

void escCopy(char *in, char *out, char toEscape, char escape);
/* Copy in to out, escaping as needed.  Out better be big enough. 
 * (Worst case is strlen(in)*2 + 1.) */

#endif /* OBSCURE_H */
