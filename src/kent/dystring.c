/* dystring - dynamically resizing string.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dystring.h"


struct dyString *newDyString(long initialBufSize)
/* Allocate dynamic string with initial buffer size.  (Pass zero for default) */
{
struct dyString *ds;
AllocVar(ds);
if (initialBufSize == 0)
    initialBufSize = 512;
ds->string = needMem(initialBufSize+1);
ds->bufSize = initialBufSize;
return ds;
}

void dyStringFree(struct dyString **pDs)
/* Free up dynamic string. */
{
struct dyString *ds;
if ((ds = *pDs) != NULL)
    {
    freeMem(ds->string);
    freez(pDs);
    }
}

char *dyStringCannibalize(struct dyString **pDy)
/* Kill dyString, but return the string it is wrapping
 * (formerly dy->string).  This should be free'd at your
 * convenience. */
{
char *s;
struct dyString *ds = *pDy;
assert(ds != NULL);
s = ds->string;
freez(pDy);
return s;
}

void dyStringListFree(struct dyString **pDs)
/* free up a list of dyStrings */
{
struct dyString *ds, *next;
for(ds = *pDs; ds != NULL; ds = next)
    {
    next = ds->next;
    dyStringFree(&ds);
    }
*pDs = NULL;
}

static void dyStringExpandBuf(struct dyString *ds, long newSize)
/* Expand buffer to new size. */
{
ds->string = needMoreMem(ds->string, ds->stringSize+1, newSize+1);
ds->bufSize = newSize;
}

void dyStringAppendN(struct dyString *ds, char *string, long stringSize)
/* Append string of given size to end of string. */
{
long oldSize = ds->stringSize;
long newSize = oldSize + stringSize;
char *buf;
if (newSize > ds->bufSize)
    {
    long newAllocSize = newSize + oldSize;
    long oldSizeTimesOneAndAHalf = oldSize * 1.5;
    if (newAllocSize < oldSizeTimesOneAndAHalf)
        newAllocSize = oldSizeTimesOneAndAHalf;
    dyStringExpandBuf(ds,newAllocSize);
    }
buf = ds->string;
memcpy(buf+oldSize, string, stringSize);
ds->stringSize = newSize;
buf[newSize] = 0;
}

char dyStringAppendC(struct dyString *ds, char c)
/* Append char to end of string. */
{
char *s;
if (ds->stringSize >= ds->bufSize)
     dyStringExpandBuf(ds, ds->bufSize+256);
s = ds->string + ds->stringSize++;
*s++ = c;
*s = 0;
return c;
}

void dyStringAppendMultiC(struct dyString *ds, char c, int n)
/* Append N copies of char to end of string. */
{
long oldSize = ds->stringSize;
long newSize = oldSize + n;
long newAllocSize = newSize + oldSize;
char *buf;
if (newSize > ds->bufSize)
    dyStringExpandBuf(ds,newAllocSize);
buf = ds->string;
memset(buf+oldSize, c, n);
ds->stringSize = newSize;
buf[newSize] = 0;
}

void dyStringAppend(struct dyString *ds, char *string)
/* Append zero terminated string to end of dyString. */
{
dyStringAppendN(ds, string, strlen(string));
}

void dyStringAppendEscapeQuotes(struct dyString *ds, char *string,
	char quot, char esc)
/* Append escaped-for-quotation version of string to dy. */
{
char c;
char *s = string;
while ((c = *s++) != 0)
     {
     if (c == quot)
         dyStringAppendC(ds, esc);
     dyStringAppendC(ds, c);
     }
}

struct dyString * dyStringSub(char *orig, char *in, char *out)
/* Make up a duplicate of orig with all occurences of in substituted
 * with out. */
{
long inLen = strlen(in), outLen = strlen(out), origLen = strlen(orig);
struct dyString *ds = dyStringNew(origLen + 2*outLen);
char *s, *e;

if (orig == NULL) return NULL;
for (s = orig; ;)
    {
    e = stringIn(in, s);
    if (e == NULL)
	{
        e = orig + origLen;
	dyStringAppendN(ds, s, e - s);
	break;
	}
    else
        {
	dyStringAppendN(ds, s, e - s);
	dyStringAppendN(ds, out, outLen);
	s = e + inLen;
	}
    }
return ds;
}

void dyStringResize(struct dyString *ds, long newSize)
/* resize a string, if the string expands, blanks are appended */
{
long oldSize = ds->stringSize;
if (newSize > oldSize)
    {
    /* grow */
    if (newSize > ds->bufSize)
        dyStringExpandBuf(ds, newSize + ds->stringSize);
    memset(ds->string+newSize, ' ', newSize);
    }
ds->string[newSize] = '\0';
ds->stringSize = newSize;
}

void dyStringQuoteString(struct dyString *ds, char quotChar, char *text)
/* Append quotChar-quoted text (with any internal occurrences of quotChar
 * \-escaped) onto end of dy. */
{
char c;

dyStringAppendC(ds, quotChar);
while ((c = *text++) != 0)
    {
    if (c == quotChar || c == '\\')
        dyStringAppendC(ds, '\\');
    dyStringAppendC(ds, c);
    }
dyStringAppendC(ds, quotChar);
}

