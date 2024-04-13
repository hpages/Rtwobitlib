/* Obscure stuff that is handy every now and again. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "localmem.h"
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

