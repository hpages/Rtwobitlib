/* Obscure stuff that is handy every now and again. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "localmem.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"

long incCounterFile(char *fileName)
/* Increment a 32 bit value on disk. */
{
long val = 0;
FILE *f = fopen(fileName, "r+b");
if (f != NULL)
    {
    mustReadOne(f, val);
    rewind(f);
    }
else
    {
    f = fopen(fileName, "wb");
    }
++val;
if (f != NULL)
    {
    fwrite(&val, sizeof(val), 1, f);
    if (fclose(f) != 0)
        errnoAbort("fclose failed");
    }
return val;
}

int digitsBaseTwo(unsigned long x)
/* Return base two # of digits. */
{
int digits = 0;
while (x)
    {
    digits += 1;
    x >>= 1;
    }
return digits;
}

int digitsBaseTen(int x)
/* Return number of digits base 10. */
{
int digCount = 1;
if (x < 0)
    {
    digCount = 2;
    x = -x;
    }
while (x >= 10)
    {
    digCount += 1;
    x /= 10;
    }
return digCount;
}

void writeGulp(char *file, char *buf, int size)
/* Write out a bunch of memory. */
{
FILE *f = mustOpen(file, "w");
mustWrite(f, buf, size);
carefulClose(&f);
}

void readInGulp(char *fileName, char **retBuf, size_t *retSize)
/* Read whole file in one big gulp. */
{
if (fileExists(fileName) && !isRegularFile(fileName))
    errAbort("can only read regular files with readInGulp: %s", fileName);
size_t size = (size_t)fileSize(fileName);
char *buf;
FILE *f = mustOpen(fileName, "rb");
*retBuf = buf = needLargeMem(size+1);
mustRead(f, buf, size);
buf[size] = 0;      /* Just in case it needs zero termination. */
fclose(f);
if (retSize != NULL)
    *retSize = size;
}

int countWordsInFile(char *fileName)
/* Count number of words in file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int wordCount = 0;
while (lineFileNext(lf, &line, NULL))
    wordCount += chopByWhite(line, NULL, 0);
lineFileClose(&lf);
return wordCount;
}

struct hash *hashWordsInFile(char *fileName, int hashSize)
/* Create a hash of space delimited words in file. */
{
struct hash *hash = newHash(hashSize);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
        hashAdd(hash, word, NULL);
    }
lineFileClose(&lf);
return hash;
}

struct hash *hashNameIntFile(char *fileName)
/* Given a two column file (name, integer value) return a
 * hash keyed by name with integer values */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(16);
while (lineFileRow(lf, row))
    hashAddInt(hash, row[0], lineFileNeedNum(lf, row, 1));
lineFileClose(&lf);
return hash;
}

struct hash *hashTsvBy(char *in, int keyColIx, int *retColCount)
/* Return a hash of rows keyed by the given column */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
struct hash *hash = hashNew(0);
char *line = NULL, **row = NULL;
int colCount = 0, colAlloc=0;	/* Columns as counted and as allocated */
while (lineFileNextReal(lf, &line))
    {
    if (colCount == 0)
        {
	*retColCount = colCount = chopByChar(line, '\t', NULL, 0);
	verbose(2, "Got %d columns in first real line\n", colCount);
	colAlloc = colCount + 1;  // +1 so we can detect unexpected input and complain 
	lmAllocArray(hash->lm, row, colAlloc);
	}
    int count = chopByChar(line, '\t', row, colAlloc);
    if (count != colCount)
        {
	errAbort("Expecting %d words, got more than that line %d of %s", 
	    colCount, lf->lineIx, lf->fileName);
	}
    hashAdd(hash, row[keyColIx], lmCloneRow(hash->lm, row, colCount) );
    }
lineFileClose(&lf);
return hash;
}

void writeTsvRow(FILE *f, int rowSize, char **row)
/* Write out row of strings to a line in tab-sep file */
{
if (rowSize > 0)
    {
    fprintf(f, "%s", row[0]);
    int i;
    for (i=1; i<rowSize; ++i)
        fprintf(f, "\t%s", row[i]);
    }
fprintf(f, "\n");
}

struct slPair *slPairTwoColumnFile(char *fileName)
/* Read in a two column file into an slPair list */
{
char *row[2];
struct slPair *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    slPairAdd(&list, row[0], cloneString(row[1]));
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct slName *readAllLines(char *fileName)
/* Read all lines of file into a list.  (Removes trailing carriage return.) */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slName *list = NULL, *el;
char *line;

while (lineFileNext(lf, &line, NULL))
     {
     el = newSlName(line);
     slAddHead(&list, el);
     }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void copyFile(char *source, char *dest)
/* Copy file from source to dest. */
{
int bufSize = 64*1024;
char *buf = needMem(bufSize);
int bytesRead;
int s, d;

s = open(source, O_RDONLY);
if (s < 0)
    errAbort("Couldn't open %s. %s\n", source, strerror(errno));
d = creat(dest, 0777);
if (d < 0)
    {
    close(s);
    errAbort("Couldn't open %s. %s\n", dest, strerror(errno));
    }
while ((bytesRead = read(s, buf, bufSize)) > 0)
    {
    if (write(d, buf, bytesRead) < 0)
        errAbort("Write error on %s. %s\n", dest, strerror(errno));
    }
close(s);
if (close(d) != 0)
    errnoAbort("close failed");
freeMem(buf);
}

void copyOpenFile(FILE *inFh, FILE *outFh)
/* copy an open stdio file */
{
int c;
while ((c = fgetc(inFh)) != EOF)
    fputc(c, outFh);
if (ferror(inFh))
    errnoAbort("file read failed");
if (ferror(outFh))
    errnoAbort("file write failed");
}

void cpFile(int s, int d)
/* Copy from source file to dest until reach end of file. */
{
int bufSize = 64*1024, readSize;
char *buf = needMem(bufSize);

for (;;)
    {
    readSize = read(s, buf, bufSize);
    if (readSize > 0)
        mustWriteFd(d, buf, readSize);
    if (readSize <= 0)
        break;
    }
freeMem(buf);
}

void *charToPt(char c)
/* Convert char to pointer. Use when really want to store
 * a char in a pointer field. */
{
char *pt = NULL;
return pt+c;
}

char ptToChar(void *pt)
/* Convert pointer to char.  Use when really want to store a
 * pointer in a char. */
{
char *a = NULL, *b = pt;
return b - a;
}


void *intToPt(int i)
/* Convert integer to pointer. Use when really want to store an
 * int in a pointer field. */
{
char *pt = NULL;
return pt+i;
}

int ptToInt(void *pt)
/* Convert pointer to integer.  Use when really want to store a
 * pointer in an int. */
{
char *a = NULL, *b = pt;
return b - a;
}

void *sizetToPt(size_t i)
/* Convert size_t to pointer. Use when really want to store a
 * size_t in a pointer. */
{
char *pt = NULL;
return pt+i;
}

size_t ptToSizet(void *pt)
/* Convert pointer to size_t.  Use when really want to store a
 * pointer in a size_t. */
{
char *a = NULL, *b = pt;
return b - a;
}

boolean parseQuotedStringNoEscapes( char *in, char *out, char **retNext)
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end.
 * Unlike parseQuotedString() do not treat backslash as an escape
 *	character, merely pass it on through.
 */
{
char c, *s = in;
int quoteChar = *s++;

for (;;)
   {
   c = *s++;
   if (c == 0)
       {
       warn("Unmatched %c", quoteChar);
       return FALSE;
       }
   else if (c == quoteChar)
       break;
   else
       *out++ = c;
   }
*out = 0;
if (retNext != NULL)
    *retNext = s;
return TRUE;
}

boolean parseQuotedString( char *in, char *out, char **retNext)
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end. */
{
char c, *s = in;
int quoteChar = *s++;
boolean escaped = FALSE;

for (;;)
   {
   c = *s++;
   if (c == 0)
       {
       warn("Unmatched %c", quoteChar);
       return FALSE;
       }
   if (escaped)
       {
       if (c == '\\' || c == quoteChar)
          *out++ = c;
       else
          {
	  *out++ = '\\';
	  *out++ = c;
	  }
       escaped = FALSE;
       }
   else
       {
       if (c == '\\')
           escaped = TRUE;
       else if (c == quoteChar)
           break;
       else
           *out++ = c;
       }
   }
*out = 0;
if (retNext != NULL)
    *retNext = s;
return TRUE;
}

char *nextQuotedWord(char **pLine)
/* Generalization of nextWord.  Returns next quoted
 * string or if no quotes next word.  Updates *pLine
 * to point past word that is returned. Does not return
 * quotes. */
{
char *line, c;
line = skipLeadingSpaces(*pLine);
if (line == NULL || line[0] == 0)
    return NULL;
c = *line;
if (c == '"' || c == '\'')
    {
    if (!parseQuotedString(line, line, pLine))
        return NULL;
    return line;
    }
else
    {
    return nextWord(pLine);
    }
}

struct slName *slNameListOfUniqueWords(char *text,boolean respectQuotes)
/* Return list of unique words found by parsing string delimited by whitespace.
 * If respectQuotes then ["Lucy and Ricky" 'Fred and Ethyl'] will yield 2 slNames no quotes */
{
struct slName *list = NULL;
char *word = NULL;
while (text != NULL)
    {
    if (respectQuotes)
        word = nextQuotedWord(&text);
    else
        word = nextWord(&text);
    if (word)
        slNameStore(&list, word);
    else
        break;
    }
slReverse(&list);
return list;
}

void escCopy(char *in, char *out, char toEscape, char escape)
/* Copy in to out, escaping as needed.  Out better be big enough. 
 * (Worst case is strlen(in)*2 + 1.) */
{
char c;
for (;;)
    {
    c = *in++;
    if (c == toEscape)
        *out++ = escape;
    *out++ = c;
    if (c == 0)
        break;
    }
}

char *makeEscapedString(char *in, char toEscape)
/* Return string that is a copy of in, but with all
 * toEscape characters preceded by '\' 
 * When done freeMem result. */
{
int newSize = strlen(in) + countChars(in, toEscape);
char *out = needMem(newSize+1);
escCopy(in, out, toEscape, '\\');
return out;
}

char *makeQuotedString(char *in, char quoteChar)
/* Create a string surrounded by quoteChar, with internal
 * quoteChars escaped.  freeMem result when done. */
{
int newSize = 2 + strlen(in) + countChars(in, quoteChar);
char *out = needMem(newSize+1);
out[0] = quoteChar;
escCopy(in, out+1, quoteChar, '\\');
out[newSize-1] = quoteChar;
return out;
}

struct hash *hashThisEqThatLine(char *line, int lineIx, boolean firstStartsWithLetter)
/* Return a symbol table from a line of form:
 *   1-this1=val1 2-this='quoted val2' var3="another val" 
 * If firstStartsWithLetter is true, then the left side of the equals must start with
 * a letter. */
{
char *dupe = cloneString(line);
char *s = dupe, c;
char *var, *val;
struct hash *hash = newHash(8);

for (;;)
    {
    if ((var = skipLeadingSpaces(s)) == NULL)
        break;

    if ((c = *var) == 0)
        break;
    if (firstStartsWithLetter && !isalpha(c))
	errAbort("line %d of custom input: variable needs to start with letter '%s'", lineIx, var);
    val = strchr(var, '=');
    if (val == NULL)
        {
        errAbort("line %d of var %s in custom input: %s \n missing = in var/val pair", lineIx, var, line);
        }
    *val++ = 0;
    c = *val;
    if (c == '\'' || c == '"')
        {
	if (!parseQuotedString(val, val, &s))
	    errAbort("line %d of input: missing closing %c", lineIx, c);
	}
    else
	{
	s = skipToSpaces(val);
	if (s != NULL) *s++ = 0;
	}
    hashAdd(hash, var, cloneString(val));
    }
freez(&dupe);
return hash;
}

struct hash *hashVarLine(char *line, int lineIx)
/* Return a symbol table from a line of form:
 *   var1=val1 var2='quoted val2' var3="another val" */
{
return hashThisEqThatLine(line, lineIx, TRUE);
}

struct slName *stringToSlNames(char *string)
/* split string and convert to a list of slNames separated by
 * white space, but allowing multiple words in quotes.
 * Quotes if any are stripped.  */
{
struct slName *list = NULL, *name;
char *dupe = cloneString(string);
char c, *s = dupe, *e = NULL;

for (;;)
    {
    if ((s = skipLeadingSpaces(s)) == NULL)
        break;
    if ((c = *s) == 0)
        break;
    if (c == '\'' || c == '"')
        {
	if (!parseQuotedString(s, s, &e))
	    errAbort("missing closing %c in %s", c, string);
	}
    else
        {
	e = skipToSpaces(s);
	if (e != NULL) *e++ = 0;
	}
    name = slNameNew(s);
    slAddHead(&list, name);
    s = e;
    }
freeMem(dupe);
slReverse(&list);
return list;
}

struct slName *charSepToSlNames(char *string, char c)
/* Split string and convert character-separated list of items to slName list. 
 * Note that the last occurence of c is optional.  (That
 * is for a comma-separated list a,b,c and a,b,c, are
 * equivalent. */
{
struct slName *list = NULL, *el;
char *s, *e;

s = string;
while (s != NULL && s[0] != 0)
    {
    e = strchr(s, c);
    if (e == NULL)
        {
	el = slNameNew(s);
	slAddHead(&list, el);
	break;
	}
    else
        {
	el = slNameNewN(s, e - s);
	slAddHead(&list, el);
	s = e+1;
	}
    }
slReverse(&list);
return list;
}

struct slName *commaSepToSlNames(char *commaSep)
/* Split string and convert comma-separated list of items to slName list.  */
{
return charSepToSlNames(commaSep, ',');
}


void sprintLongWithCommas(char *s, long long l)
/* Print out a long number with commas a thousands, millions, etc. */
{
long long trillions, billions, millions, thousands;
if (l >= 1000000000000LL)
    {
    trillions = l/1000000000000LL;
    l -= trillions * 1000000000000LL;
    billions = l/1000000000;
    l -= billions * 1000000000;
    millions = l/1000000;
    l -= millions * 1000000;
    thousands = l/1000;
    l -= thousands * 1000;
    sprintf(s, "%lld,%03lld,%03lld,%03lld,%03lld", trillions, billions, millions, thousands, l);
    }
else if (l >= 1000000000)
    {
    billions = l/1000000000;
    l -= billions * 1000000000;
    millions = l/1000000;
    l -= millions * 1000000;
    thousands = l/1000;
    l -= thousands * 1000;
    sprintf(s, "%lld,%03lld,%03lld,%03lld", billions, millions, thousands, l);
    }
else if (l >= 1000000)
    {
    millions = l/1000000;
    l -= millions * (long long)1000000;
    thousands = l/1000;
    l -= thousands * 1000;
    sprintf(s, "%lld,%03lld,%03lld", millions, thousands, l);
    }
else if (l >= 1000)
    {
    thousands = l/1000;
    l -= thousands * 1000;
    sprintf(s, "%lld,%03lld", thousands, l);
    }
else
    sprintf(s, "%lld", l);
}

void printLongWithCommas(FILE *f, long long l)
/* Print out a long number with commas at thousands, millions, etc. */
{
char ascii[32];
sprintLongWithCommas(ascii, l);
fprintf(f, "%s", ascii);
}

char *stripCommas(char *position)
/* make a new string with commas stripped out */
{
char *newPos = cloneString(position);
char *nPtr = newPos;

if (position == NULL)
    return NULL;
while((*nPtr = *position++))
    if (*nPtr != ',')
	nPtr++;

return newPos;
}

