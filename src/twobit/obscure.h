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

void sprintLongWithCommas(char *s, long long l);
/* Print out a long number with commas a thousands, millions, etc. */

void printLongWithCommas(FILE *f, long long l);
/* Print out a long number with commas a thousands, millions, etc. */

void writeGulp(char *file, char *buf, int size);
/* Write out a bunch of memory. */

void readInGulp(char *fileName, char **retBuf, size_t *retSize);
/* Read whole file in one big gulp. */

int countWordsInFile(char *fileName);
/* Count number of words in file. */

struct slName *readAllLines(char *fileName);
/* Read all lines of file into a list.  (Removes trailing carriage return.) */

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

boolean parseQuotedStringNoEscapes( char *in, char *out, char **retNext);
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end.
 * Unlike parseQuotedString() do not treat backslash as an escape
 *	character, merely pass it on through.
 */

boolean parseQuotedString( char *in, char *out, char **retNext);
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end. */

char *nextQuotedWord(char **pLine);
/* Generalization of nextWord.  Returns next quoted
 * string or if no quotes next word.  Updates *pLine
 * to point past word that is returned. Does not return
 * quotes. */

struct slName *slNameListOfUniqueWords(char *text,boolean respectQuotes);
/* Return list of unique words found by parsing string delimited by whitespace.
 * If respectQuotes then ["Lucy and Ricky" 'Fred and Ethyl'] will yield 2 slNames no quotes */

char *makeQuotedString(char *in, char quoteChar);
/* Create a string surrounded by quoteChar, with internal
 * quoteChars escaped.  freeMem result when done. */

char *makeEscapedString(char *in, char toEscape);
/* Return string that is a copy of in, but with all
 * toEscape characters preceded by '\' 
 * When done freeMem result. */

void escCopy(char *in, char *out, char toEscape, char escape);
/* Copy in to out, escaping as needed.  Out better be big enough. 
 * (Worst case is strlen(in)*2 + 1.) */

struct slName *stringToSlNames(char *string);
/* Convert string to a list of slNames separated by
 * white space, but allowing multiple words in quotes.
 * Quotes if any are stripped.  */

struct slName *commaSepToSlNames(char *commaSep);
/* Convert comma-separated list of items to slName list. */

struct slName *charSepToSlNames(char *string, char c);
/* Convert character-separated list of items to slName list. 
 * Note that the last occurence of c is optional.  (That
 * is for a comma-separated list a,b,c and a,b,c, are
 * equivalent. */

struct hash *hashVarLine(char *line, int lineIx);
/* Return a symbol table from a line of form:
 *   var1=val1 var2='quoted val2' var3="another val" */

struct hash *hashThisEqThatLine(char *line, int lineIx, boolean firstStartsWithLetter);
/* Return a symbol table from a line of form:
 *   1-this1=val1 2-this='quoted val2' var3="another val" 
 * If firstStartsWithLetter is true, then the left side of the equals must start with
 * and equals. */

struct hash *hashWordsInFile(char *fileName, int hashSize);
/* Create a hash of space delimited words in file. 
 * hashSize is as in hashNew() - pass 0 for default. */

struct hash *hashNameIntFile(char *fileName);
/* Given a two column file (name, integer value) return a
 * hash keyed by name with integer values */

struct hash *hashTsvBy(char *in, int keyColIx, int *retColCount);
/* Return a hash of rows keyed by the given column */

void writeTsvRow(FILE *f, int rowSize, char **row);
/* Write out row of strings to a line in tab-sep file */

struct slPair *slPairTwoColumnFile(char *fileName);
/* Read in a two column file into an slPair list */

char *stripCommas(char *position);
/* make a new string with commas stripped out */

#endif /* OBSCURE_H */
