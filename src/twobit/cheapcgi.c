/* Routines for getting variables passed in from web page
 * forms via CGI.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "linefile.h"
#include "errAbort.h"
//#include "filePath.h"
//#include "htmshell.h"
#ifndef GBROWSE
//#include "mime.h"
#endif /* GBROWSE */
#include <signal.h>

//============ javascript inline-separation routines ===============

// One of the main services that CSP (Content Security Policy) provides
// is protecting from reflected and stored XSS attacks by disabling all inline javacript,
// both in script tags, and in inline event handlers.  The separated javascript 
// can be either added back to the end of the html page with a nonce or sha hashid,
// or it can be saved to a temp file in trash and then included as a non-inline, off-page .js.

struct dyString *jsInlineLines = NULL;

void jsInlineInit()
/* init if needed */
{
if (!jsInlineLines) 
    {
    jsInlineLines = dyStringNew(1024);	
    }
}

void jsInline(char *javascript)
/* Add javascript text to output file or memory structure */
{
jsInlineInit(); // init if needed
dyStringAppend(jsInlineLines, javascript);
}

void jsInlineF(char *format, ...)
/* Add javascript text to output file or memory structure */
{
jsInlineInit(); // init if needed
va_list args;
va_start(args, format);
dyStringVaPrintf(jsInlineLines, format, args);
va_end(args);
}

boolean jsInlineFinishCalled = FALSE;

void jsInlineReset()
/* used by genomeSpace to repeatedly output multiple pages to stdout */
{
jsInlineFinishCalled = FALSE;
}

const char * const jsEvents[] = { 
"abort",
"activate",
"afterprint",
"afterupdate",
"beforeactivate",
"beforecopy",
"beforecut",
"beforedeactivate",
"beforeeditfocus",
"beforepaste",
"beforeprint",
"beforeunload",
"beforeupdate",
"blur",
"bounce",
"cellchange",
"change",
"click",
"contextmenu",
"controlselect",
"copy",
"cut",
"dataavailable",
"datasetchanged",
"datasetcomplete",
"dblclick",
"deactivate",
"drag",
"dragend",
"dragenter",
"dragleave",
"dragover",
"dragstart",
"drop",
"error",
"errorupdate",
"filterchange",
"finish",
"focus",
"focusin",
"focusout",
"hashchange",
"help",
"input",
"keydown",
"keypress",
"keyup",
"load",
"losecapture",
"message",
"mousedown",
"mouseenter",
"mouseleave",
"mousemove",
"mouseout",
"mouseover",
"mouseup",
"mousewheel",
"move",
"moveend",
"movestart",
"offline",
"line",
"online",
"paste",
"propertychange",
"readystatechange",
"reset",
"resize",
"resizeend",
"resizestart",
"rowenter",
"rowexit",
"rowsdelete",
"rowsinserted",
"scroll",
"search",
"select",
"selectionchange",
"selectstart",
"start",
"stop",
"submit",
"unload",
"" };


boolean findJsEvent(char *event)
/* see if it is in the list */
{
int i = 0;
while (TRUE)
    {
    const char *w = jsEvents[i];
    if (sameString(w, event))
	return TRUE;
    if (sameString(w, ""))
	return FALSE;
    ++i;
    }
return FALSE; // should never get here
}

void checkValidEvent(char *event)
/* check if it is lowercase and a known valid event name */
{
char *temp = cloneString(event);
tolowers(temp);
if (!sameString(temp, event))
    warn("jsInline: javascript event %s should be given in lower-case", event);
event = temp; 
if (!findJsEvent(event))
    warn("jsInline: unknown javascript event %s", event);
freeMem (event);
}

void jsAddEventForId(char *eventName, char *idText, char *jsText)
{
checkValidEvent(eventName);
jsInlineF("document.getElementById('%s').addEventListener('%s', %s);\n", idText, eventName, jsText);
}

void jsOnEventById(char *eventName, char *idText, char *jsText)
/* Add js mapping for inline event */
{
checkValidEvent(eventName);
jsInlineF("document.getElementById('%s').on%s = function(event) {if (!event) {event=window.event}; %s};\n", idText, eventName, jsText);
}

void jsOnEventBySelector(char *eventName, char *query, char *jsText)
/* Add js mapping for inline event given a query selector, e.g. '.className' */
{
checkValidEvent(eventName);
jsInlineF("document.querySelector('%s').addEventListener( '%s', function(event) { %s };\n", eventName, query, jsText);
}


void jsOnEventByIdF(char *eventName, char *idText, char *format, ...)
/* Add js mapping for inline event with printf formatting */
{
checkValidEvent(eventName);
jsInlineF("document.getElementById('%s').on%s = function(event) {if (!event) {event=window.event}; ", idText, eventName);
va_list args;
va_start(args, format);
dyStringVaPrintf(jsInlineLines, format, args);
va_end(args);
jsInlineF("};\n");
}


//============ END of javascript inline-separation routines ===============


/* These three variables hold the parsed version of cgi variables. */
static char *inputString = NULL;
static unsigned long inputSize;
static struct hash *inputHash = NULL;
static struct cgiVar *inputList = NULL;

static boolean haveCookiesHash = FALSE;
static struct hash *cookieHash = NULL;
static struct cgiVar *cookieList = NULL;

/* should cheapcgi use temp files to store uploaded files */
static boolean doUseTempFile = FALSE;

void dumpCookieList()
/* Print out the cookie list. */
{
struct cgiVar *v;
for (v=cookieList; v != NULL; v = v->next)
    printf("%s=%s (%d)\n", v->name, v->val, v->saved);
}

void useTempFile()
/* tell cheapcgi to use temp files */
{
doUseTempFile = TRUE;
}

boolean cgiIsOnWeb()
/* Return TRUE if looks like we're being run as a CGI. */
{
return getenv("REQUEST_METHOD") != NULL;
}

char *cgiRequestMethod()
/* Return CGI REQUEST_METHOD (such as 'GET/POST/PUT/DELETE/HEAD') */
{
return getenv("REQUEST_METHOD");
}

char *cgiRequestUri()
/* Return CGI REQUEST_URI */
{
return getenv("REQUEST_URI");
}

char *cgiRequestContentLength()
/* Return HTTP REQUEST CONTENT_LENGTH if available*/
{
return getenv("CONTENT_LENGTH");
}

char *cgiScriptName()
/* Return name of script so libs can do context-sensitive stuff. */
{
char *scriptName = getenv("SCRIPT_NAME");
if (scriptName == NULL)
    scriptName = "cgiSpoofedScript";
return scriptName;
}

char *cgiServerName()
/* Return name of server, better to use cgiServerNamePort() for
   actual URL construction */
{
return getenv("SERVER_NAME");
}

char *cgiServerPort()
/* Return port number of server, default 80 if not found */
{
char *port = getenv("SERVER_PORT");
if (port)
    return port;
else
    return "80";
}

boolean cgiServerHttpsIsOn()
/* Return true if HTTPS is on */
{
char *httpsIsOn = getenv("HTTPS");
if (httpsIsOn)
    return sameString(httpsIsOn, "on");
else
    return FALSE;
}

char *cgiAppendSForHttps()
/* if running on https, add the letter s to the url protocol */
{
if (cgiServerHttpsIsOn())
    return "s";
return "";
}


char *cgiServerNamePort()
/* Return name of server with port if different than 80 */
{
char *port = cgiServerPort();
char *name = cgiServerName();
struct dyString *result = dyStringNew(256);
char *defaultPort = "80";
if (cgiServerHttpsIsOn())
    defaultPort = "443";

if (name)
    {
    dyStringPrintf(result,"%s",name);
    if (differentString(port, defaultPort))
	dyStringPrintf(result,":%s",port);
    return dyStringCannibalize(&result);
    }
else
    return NULL;
}

char *cgiRemoteAddr()
/* Return IP address of client (or "unknown"). */
{
static char *dunno = "unknown";
char *remoteAddr = getenv("REMOTE_ADDR");
if (remoteAddr == NULL)
    remoteAddr = dunno;
return remoteAddr;
}

char *cgiUserAgent()
/* Return remote user agent (HTTP_USER_AGENT) or NULL if remote user agent is not known */
{
return getenv("HTTP_USER_AGENT");
}

enum browserType cgiClientBrowser(char **browserQualifier, enum osType *clientOs,
                                  char **clientOsQualifier)
/* Return client browser type determined from (HTTP_USER_AGENT)
   Optionally requuest the additional info about the client */
{
// WARNING: The specifics of the HTTP_USER_AGENT vary widely.
//          This has only been tested on a few cases.
static enum browserType clientBrowser = btUnknown;
static enum browserType clientOsType  = (enum browserType)osUnknown;
static char *clientBrowserExtra       = NULL;
static char *clientOsExtra            = NULL;

if (clientBrowser == btUnknown)
    {
    char *userAgent = cgiUserAgent();
    if (userAgent != NULL)
        {
        //warn(userAgent);  // Use this to investigate other cases
        char *ptr=NULL;

        // Determine the browser
        if ((ptr = stringIn("Opera",userAgent)) != NULL) // Must be before IE
            {
            clientBrowser = btOpera;
            }
        else if ((ptr = stringIn("MSIE ",userAgent)) != NULL)
            {
            clientBrowser = btIE;
            ptr += strlen("MSIE ");
            clientBrowserExtra = cloneFirstWordByDelimiter(ptr,';');
            }
        else if ((ptr = stringIn("Firefox",userAgent)) != NULL)
            {
            clientBrowser = btFF;
            ptr += strlen("(Firefox/");
            clientBrowserExtra = cloneFirstWordByDelimiter(ptr,' ');
            }
        else if ((ptr = stringIn("Chrome",userAgent)) != NULL)  // Must be before Safari
            {
            clientBrowser = btChrome;
            ptr += strlen("Chrome/");
            clientBrowserExtra = cloneFirstWordByDelimiter(ptr,' ');
            }
        else if ((ptr = stringIn("Safari",userAgent)) != NULL)
            {
            clientBrowser = btSafari;
            ptr += strlen("Safari/");
            clientBrowserExtra = cloneFirstWordByDelimiter(ptr,' ');
            }
        else
            {
            clientBrowser = btOther;
            }

        // Determine the OS
        if ((ptr = stringIn("Windows",userAgent)) != NULL)
            {
            clientOsType = (enum browserType)osWindows;
            ptr += strlen("Windows ");
            clientOsExtra = cloneFirstWordByDelimiter(ptr,';');
            }
        else if ((ptr = stringIn("Linux",userAgent)) != NULL)
            {
            clientOsType = (enum browserType)osLinux;
            ptr += strlen("Linux ");
            clientOsExtra = cloneFirstWordByDelimiter(ptr,';');
            }
        else if ((ptr = stringIn("Mac ",userAgent)) != NULL)
            {
            clientOsType = (enum browserType)osMac;
            ptr += strlen("Mac ");
            clientOsExtra = cloneFirstWordByDelimiter(ptr,';');
            }
        else
            {
            clientOsType = (enum browserType)osOther;
            }
        }
    }
if (browserQualifier != NULL)
    {
    if (clientBrowserExtra != NULL)
        *browserQualifier = cloneString(clientBrowserExtra);
    else
        *browserQualifier = NULL;
    }
if (clientOs != NULL)
    *clientOs = (enum osType)clientOsType;
if (clientOsQualifier != NULL)
    {
    if (clientOsExtra != NULL)
        *clientOsQualifier = cloneString(clientOsExtra);
    else
        *clientOsQualifier = NULL;
    }

return clientBrowser;
}

char *_cgiRawInput()
/* For debugging get the unprocessed input. */
{
return inputString;
}

static void getQueryInputExt(boolean abortOnErr)
/* Get query string from environment if they've used GET method. */
{
inputString = getenv("QUERY_STRING");
if (inputString == NULL)
    {
    if (abortOnErr)
	errAbort("No QUERY_STRING in environment.");
    inputString = cloneString("");
    return;
    }
inputString = cloneString(inputString);
}

static void getQueryInput()
/* Get query string from environment if they've used GET method. */
{
getQueryInputExt(TRUE);
}

static void getPostInput()
/* Get input from file if they've used POST method.
 * Grab any GET QUERY_STRING input first. */
{
char *s;
long i;
int r;

getQueryInputExt(FALSE);
int getSize = strlen(inputString);

s = getenv("CONTENT_LENGTH");
if (s == NULL)
    errAbort("No CONTENT_LENGTH in environment.");
if (sscanf(s, "%lu", &inputSize) != 1)
    errAbort("CONTENT_LENGTH isn't a number.");
s = getenv("CONTENT_TYPE");
if (s != NULL && startsWith("multipart/form-data", s))
    {
    /* use MIME parse on input stream instead, can handle large uploads */
    /* inputString must not be NULL so it knows it was set */
    return;
    }
int len = getSize + inputSize;
if (getSize > 0)
    ++len;
char *temp = needMem((size_t)len+1);
for (i=0; i<inputSize; ++i)
    {
    r = getc(stdin);
    if (r == EOF)
	errAbort("Short POST input to %s: CONTENT_LENGTH=%ld, only %ld supplied",
                 cgiScriptName(), inputSize, i);
    temp[i] = r;
    }
if (getSize > 0)
  temp[i++] = '&';
strncpy(temp+i, inputString, getSize);
temp[len] = 0;
freeMem(inputString);
inputString = temp;
}

#define memmem(hay, haySize, needle, needleSize) \
    memMatch(needle, needleSize, hay, haySize)


static void parseCookies(struct hash **retHash, struct cgiVar **retList)
/* parses any cookies and puts them into the given hash and list */
{
char* str;
char *namePt, *dataPt, *nextNamePt;
struct hash *hash;
struct cgiVar *list = NULL, *el;

/* don't build the hash table again */
if(haveCookiesHash == TRUE)
	return;

str = cloneString(getenv("HTTP_COOKIE"));
if(str == NULL) /* don't have a cookie */
	return;

hash = newHash(6);

namePt = str;
while (isNotEmpty(namePt))
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	errAbort("Mangled Cookie input string: no = in '%s' (offset %d in complete cookie string: '%s')",
		 namePt, (int)(namePt - str), getenv("HTTP_COOKIE"));
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, ';');
    if (nextNamePt != NULL)
	{
         *nextNamePt++ = 0;
	 if (*nextNamePt == ' ')
	     nextNamePt++;
	}
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    AllocVar(el);
    el->val = dataPt;
    slAddHead(&list, el);
    hashAddSaveName(hash, namePt, el, &el->name);
    namePt = nextNamePt;
    }

haveCookiesHash = TRUE;

slReverse(&list);
*retList = list;
*retHash = hash;
}

char *findCookieData(char *varName)
/* Get the string associated with varName from the cookie string. */
{
struct hashEl *hel;
char *firstResult;

/* make sure that the cookie hash table has been created */
parseCookies(&cookieHash, &cookieList);
if (cookieHash == NULL)
    return NULL;
/* Watch out for multiple cookies with the same name (hel is a list) --
 * warn if we find them. */
hel = hashLookup(cookieHash, varName);
if (hel == NULL)
    return NULL;
else
    firstResult = ((struct cgiVar *)hel->val)->val;
hel = hel->next;
while (hel != NULL)
    {
    char *val = ((struct cgiVar *)(hel->val))->val;
    if (sameString(varName, hel->name) && !sameString(firstResult, val))
	{
	/* This is too early to call warn -- it will mess up html output. */
	fprintf(stderr,
		"findCookieData: Duplicate cookie value from IP=%s: "
		"%s has both %s and %s\n",
		cgiRemoteAddr(),
		varName, firstResult, val);
	}
    hel = hel->next;
    }
return firstResult;
}

static char *cgiInputSource(char *s)
/* For NULL sources make a guess as to real source. */
{
char *qs;
if (s != NULL)
    return s;
qs = getenv("QUERY_STRING");
if (qs == NULL)
    return "POST";
char *cl = getenv("CONTENT_LENGTH");
if (cl != NULL && atoi(cl) > 0)
    return "POST";
return "QUERY";
}

static void _cgiFindInput(char *method)
/* Get raw CGI input into inputString.  Method can be "POST", "QUERY", "GET" or NULL
 * for unknown. */
{
if (inputString == NULL)
    {
    method = cgiInputSource(method);
    if (sameWord(method, "POST"))
        getPostInput();
    else if (sameWord(method, "QUERY") || sameWord(method, "GET"))
        getQueryInput();
    else
        errAbort("Unknown form method");
    }
}

struct cgiDictionary *cgiDictionaryFromEncodedString(char *encodedString)
/* Giving a this=that&this=that string,  return cgiDictionary parsed out from it. 
 * This does *not* destroy input like the lower level cgiParse functions do. */
{
struct cgiDictionary *d;
AllocVar(d);
d->stringData = cloneString(encodedString);
cgiParseInputAbort(d->stringData, &d->hash, &d->list);
return d;
}

void cgiDictionaryFree(struct cgiDictionary **pD)
/* Free up resources associated with dictionary. */
{
struct cgiDictionary *d = *pD;
if (d != NULL)
    {
    slFreeList(&d->list);
    hashFree(&d->hash);
    freez(&d->stringData);
    freez(pD);
    }
}

void cgiDictionaryFreeList(struct cgiDictionary **pList)
/* Free up a whole list of cgiDictionaries */
{
struct cgiDictionary *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cgiDictionaryFree(&el);
    }
*pList = NULL;
}

boolean cgiParseNext(char **pInput, char **retVar, char **retVal)
/* Parse out next var/val in a var=val&var=val... cgi formatted string 
 * This will insert zeroes and other things into string. 
 * Usage:
 *     char *pt = cgiStringStart;
 *     char *var, *val
 *     while (cgiParseNext(&pt, &var, &val))
 *          printf("%s\t%s\n", var, val); */
{
char *var = *pInput;
if (var == NULL || var[0] == 0)
    return FALSE;
char *val = strchr(var, '=');
if (val == NULL)
    errAbort("Mangled CGI input string %s", var);
*val++ = 0;
char *end = strchr(val, '&');
if (end == NULL)
    end = strchr(val, ';');  // For DAS
if (end == NULL)
    {
    end = val + strlen(val);
    *pInput = NULL;
    }
else
    {
    *pInput = end+1;
    *end = 0;
    }
*retVar = var;
*retVal = val;
cgiDecode(val,val,end-val);
return TRUE;
}

void cgiParseInputAbort(char *input, struct hash **retHash,
        struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back
 * into input, so please don't free input until you're done with
 * the hash. Prints message aborts if there's an error.
 * To clean up - slFreeList, hashFree, and only then free input. */
{
char *namePt, *dataPt, *nextNamePt;
struct hash *hash = *retHash;
struct cgiVar *list = *retList, *el;

if (!hash)
  hash = newHash(6);
slReverse(&list);

namePt = input;
while (namePt != NULL && namePt[0] != 0)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	{
	errAbort("Mangled CGI input string %s", namePt);
	}
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt == NULL)
	nextNamePt = strchr(dataPt, ';');	/* Accomodate DAS. */
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
    cgiDecode(namePt,namePt,strlen(namePt));	/* for unusual ct names */
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    AllocVar(el);
    el->val = dataPt;
    slAddHead(&list, el);
    hashAddSaveName(hash, namePt, el, &el->name);
    namePt = nextNamePt;
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

static jmp_buf cgiParseRecover;

static void cgiParseAbort()
/* Abort cgi parsing. */
{
longjmp(cgiParseRecover, -1);
}

boolean cgiParseInput(char *input, struct hash **retHash,
        struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back
 * into input, so please don't free input until you're done with
 * the hash. Prints message and returns FALSE if there's an error.
 * To clean up - slFreeList, hashFree, and only then free input. */
{
boolean ok = TRUE;
int status = setjmp(cgiParseRecover);
if (status == 0)    /* Always true except after long jump. */
    {
    pushAbortHandler(cgiParseAbort);
    cgiParseInputAbort(input, retHash, retList);
    }
else    /* They long jumped here because of an error. */
    {
    ok = FALSE;
    }
popAbortHandler();
return ok;
}

char *cgiStringNewValForVar(char *cgiIn, char *varName, char *newVal)
/* Return a cgi-encoded string with newVal in place of what was oldVal.
 * It is an error for var not to exist.   Do a freeMem of this string
 * when you are through. */
{
struct dyString *dy = dyStringNew(strlen(cgiIn) + strlen(newVal));
struct cgiParsedVars *cpv = cgiParsedVarsNew(cgiIn);
struct cgiVar *var;
boolean doneSub = FALSE;
for (var = cpv->list; var != NULL; var = var->next)
    {
    char *val = var->val;
    if (sameString(var->name, varName))
	{
        val = newVal;
	doneSub = TRUE;
	}
    cgiEncodeIntoDy(var->name, val, dy);
    }
if (!doneSub)
    errAbort("Couldn't find %s in %s", varName, cgiIn);
cgiParsedVarsFree(&cpv);
return dyStringCannibalize(&dy);
}


static boolean dumpStackOnSignal = FALSE;  // should a stack dump be generated?

void cgiBadVar(char *varName)
/* Complain about a variable that's not there. */
{
if (varName == NULL) varName = "";
errAbort("Sorry, didn't find CGI input variable '%s'", varName);
}

char *javaScriptLiteralEncode(char *inString)
/* Use backslash escaping on newline
 * and quote chars, backslash and others.
 * Intended that the encoded string will be
 * put between quotes at a higher level and
 * then interpreted by Javascript. */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (c == '\''
     || c == '\"'
     || c == '&'
     || c == '\\'
     || c == '\n'
     || c == '\r'
     || c == '\t'
     || c == '\b'
     || c == '\f'
	)
        outSize += 2;
    else
        outSize += 1;
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    if (c == '\''
     || c == '\"'
     || c == '&'
     || c == '\\'
     || c == '\n'
     || c == '\r'
     || c == '\t'
     || c == '\b'
     || c == '\f'
	)
        *out++ = '\\';
    *out++ = c;
    }
*out++ = 0;
return outString;

}


/* NOTE: Where in the URL to use which of these functions:
 *
 * Parts of a URL:
 *   protocol://user:password@server.com:port/path/filename?var1=val1&var2=val2
 *
 * Note that a space should only be encoded to a plus and decoded from a plus
 * when dealing with http URLs in the query part of the string,
 * which is the part after the ? above.
 * It should not be used in the rest of the URL.  
 * So in the query string part of a URL, do use cgiEncode/cgiDecode. 
 * And in the rest of the URL, use cgiEncodeFUll/cgiDecodeFull 
 * which do not code space as plus.
 * Since FTP does not use URLs with query parameters, use the Full version.
 */

void cgiDecode(char *in, char *out, int inLength)
/* Decode from cgi pluses-for-spaces format to normal.
 * Out will be a little shorter than in typically, and
 * can be the same buffer. */
{
char c;
int i;
for (i=0; i<inLength;++i)
    {
    c = *in++;
    if (c == '+')
	*out++ = ' ';
    else if (c == '%')
	{
	int code;
        if (sscanf(in, "%2x", &code) != 1)
	    code = '?';
	in += 2;
	i += 2;
	*out++ = code;
	}
    else
	*out++ = c;
    }
*out++ = 0;
}

void cgiDecodeFull(char *in, char *out, int inLength)
/* Out will be a cgi-decoded version of in (no space from plus!).
 * Out will be a little shorter than in typically, and
 * can be the same buffer. */
{
char c;
int i;
for (i=0; i<inLength;++i)
    {
    c = *in++;
    if (c == '%')
	{
	int code;
        if (sscanf(in, "%2x", &code) != 1)
	    code = '?';
	in += 2;
	i += 2;
	*out++ = code;
	}
    else
	*out++ = c;
    }
*out++ = 0;
}

char *cgiEncode(char *inString)
/* Return a cgi-encoded version of inString.
 * Alphanumerics kept as is, space translated to plus,
 * and all other characters translated to %hexVal. */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == ' ' || c == '.' || c == '_')
        outSize += 1;
    else
        outSize += 3;
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        *out++ = c;
    else if (c == ' ')
        *out++ = '+';
    else
        {
        unsigned char uc = c;
        char buf[4];
        *out++ = '%';
        safef(buf, sizeof(buf), "%02X", uc);
        *out++ = buf[0];
        *out++ = buf[1];
        }
    }
*out++ = 0;
return outString;
}

char *cgiEncodeFull(char *inString)
/* Return a cgi-encoded version of inString (no + for space!).
 * Alphanumerics/./_ kept as is and all other characters translated to
 * %hexVal. */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        outSize += 1;
    else
        outSize += 3;
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        *out++ = c;
    else
        {
        unsigned char uc = c;
        char buf[4];
        *out++ = '%';
        safef(buf, sizeof(buf), "%02X", uc);
        *out++ = buf[0];
        *out++ = buf[1];
        }
    }
*out++ = 0;
return outString;
}

void cgiMakeSubmitButton()
/* Make 'submit' type button. */
{
cgiMakeButton("Submit", "Submit");
}

void cgiMakeResetButton()
/* Make 'reset' type button. */
{
printf("<INPUT TYPE=RESET NAME=\"Reset\" VALUE=\" Reset \">");
}

void cgiMakeCancelButton(char *label)
/* Make button named 'Cancel' (for modal dialogs), with optional label (e.g. OK) */
{
char *buttonLabel = (label ? label : "Cancel");
printf("<input type='button' name='Cancel' value='%s'>", buttonLabel);
}

void cgiMakeClearButton(char *form, char *field)
/* Make button to clear a text field. */
{
char id[256];
safef(id, sizeof id, "%s_clickBut", form);
char javascript[1024];
safef(javascript, sizeof(javascript),
    "document.%s.%s.value = ''; document.%s.submit();", form, field, form);
cgiMakeOnClickButton(id, javascript, " Clear  ");
}

void cgiMakeClearButtonNoSubmit(char *form, char *field)
/* Make button to clear a text field, without resubmitting the form. */
{
char id[256];
safef(id, sizeof id, "%s_clear", field);
char javascript[1024];
safef(javascript, sizeof javascript,
        "document.%s.%s.value = '';", form, field);
cgiMakeOnClickButton(id, javascript, " Clear ");
}

void cgiMakeButtonMaybePressedMaybeSubmit(char *name, char *label, char *msg, 
                                        char *onClick, boolean pressed, boolean isSubmit)
/* Make button type input, with optional messsage and onclick javascript. Optionally this
   is a submit button.  Set styling to indicate whether button has been pressed (for buttons 
   that change browser mode).
 */
{
printf("<input type='%s' name='%s' id='%s' value='%s'",
                isSubmit ? "submit" : "button", name, name, label);
if (pressed)
    printf(" class='pressed'");
if (msg)
    printf(" title='%s'", msg);
printf(">");
if (onClick)
    jsOnEventById("click", name, onClick);
}

void cgiMakeSubmitButtonMaybePressed(char *name, char *label, char *msg, 
                                        char *onClick, boolean pressed)
/* Make 'submit' type button, with optional messsage and onclick javascript.
   Set styling to indicate whether button has been pressed (for buttons that change browser mode).
 */
{
cgiMakeButtonMaybePressedMaybeSubmit(name, label, msg, onClick, pressed, TRUE);
}

void cgiMakeNonSubmitButtonMaybePressed(char *name, char *label, char *msg, 
                                        char *onClick, boolean pressed)
/* Make 'submit' type button, with optional messsage and onclick javascript.
   Set styling to indicate whether button has been pressed (for buttons that change browser mode).
 */
{
cgiMakeButtonMaybePressedMaybeSubmit(name, label, msg, onClick, pressed, FALSE);
}

void cgiMakeButtonWithMsg(char *name, char *value, char *msg)
/* Make 'submit' type button. Display msg on mouseover, if present*/
{
cgiMakeSubmitButtonMaybePressed(name, value, msg, NULL, FALSE);
}

void cgiMakeOnClickSubmitButton(char *command, char *name, char *value)
/* Make submit button with both variable name and value with client side
 * onClick (java)script. */
{
cgiMakeSubmitButtonMaybePressed(name, value, NULL, command, FALSE);
}

void cgiMakeButtonWithOnClick(char *name, char *value, char *msg, char *onClick)
/* Make 'submit' type button, with onclick javascript */
{
cgiMakeSubmitButtonMaybePressed(name, value, msg, onClick, FALSE);
}

void cgiMakeButton(char *name, char *value)
/* Make 'submit' type button */
{
cgiMakeButtonWithMsg(name, value, NULL);
}

void cgiMakeOnClickButtonWithMsg(char *id, char *command, char *value, char *msg)
/* Make button (not submit) with client side onClick javascript. Display msg on mouseover. */
{
printf("<input type='button' id='%s' value=\"%s\"", id, value);
if (msg)
    printf(" title='%s'", msg);
printf(">");
jsOnEventById("click", id, command);
}

void cgiMakeOnClickButton(char *id, char *command, char *value)
/* Make button (not submit) with client side onClick javascript. */
{
cgiMakeOnClickButtonWithMsg(id, command, value, NULL);
}

void cgiMakeOptionalButton(char *name, char *value, boolean disabled)
/* Make 'submit' type button that can be disabled. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\"", name, value);
if (disabled)
    printf(" DISABLED");
printf(">");
}

void cgiMakeFileEntry(char *name)
/* Make file entry box/browser */
{
    printf("<INPUT TYPE=FILE NAME=\"%s\">", name);
}

void cgiSimpleTableStart()
/* start HTML table  -- no customization. Leaves room
 * for a fancier implementation */
{
printf("<TABLE>\n");
}

void cgiTableEnd()
/* end HTML table */
{
printf("</TABLE>\n");
}

void cgiSimpleTableRowStart()
/* Start table row */
{
printf("<TR>\n");
}

void cgiTableRowEnd()
/* End table row */
{
printf("</TR>\n");
}

void cgiSimpleTableFieldStart()
/* Start table field */
{
printf("<TD>");
}

void cgiTableFieldStartAlignRight()
/* Start table field and align right*/
{
printf("<TD ALIGN = RIGHT>");
}

void cgiTableFieldEnd()
/* End table field */
{
printf("</TD>\n");
}

void cgiTableField(char *text)
/* Make table field entry */
{
printf("<TD> %s </TD>\n", text);
}

void cgiTableFieldWithMsg(char *text, char *msg)
/* Make table field entry with mouseover */
{
printf("<TD %s%s%s> %s </TD>\n",
        (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ),
        text);
}

void cgiParagraph(char *text)
/* Make text paragraph */
{
printf("<P> %s\n", text);
}

void cgiMakeRadioButton(char *name, char *value, boolean checked)
/* Make radio type button.  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. */
{
printf("<input type=radio name='%s' id='%s' value='%s'",
        name, name, value);
if (checked)
   printf(" CHECKED");
printf(">");
}

void cgiMakeOnEventRadioButtonWithClass(char *name, char *value, boolean checked,
    char *class, char *event, char *command)
/* Make radio type button with an event and an optional class attribute.
 *  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. If class is non-null it is included. */
{
char temp[256];
safef(temp, sizeof temp, "%s_%s", name, value);
char *valNoSpc = replaceChars(temp, " ", "_"); // replace spaces with underscore
char *id = replaceChars(valNoSpc, ".", "_"); // replace dots with underscore, js does not like dots in ids
freeMem(valNoSpc);
printf("<input type=radio name='%s' id='%s' value='%s'",
        name, id, value);
if (checked)
   printf(" CHECKED");
printf(" class='%s'", class);
printf(">");
jsOnEventById(event, id, command);
freeMem(id);
}

char *cgiBooleanShadowPrefix()
/* Prefix for shadow variable set with boolean variables. */
{
return "boolshad.";
}
#define BOOLSHAD_EXTRA "class='cbShadow'"

static void cgiMakeCheckBox2Bool(char *name, boolean checked, boolean enabled,
                                 char *id, char *moreHtml)
/* Make check box - designed to be called by the variously overloaded
 * cgiMakeCheckBox functions, and should NOT be called directly.
 * moreHtml: optional additional html like javascript call or mouseover msg (may be NULL)
 * id: button id (may be NULL)
 * Also make a shadow hidden variable and support 2 boolean states:
 *    checked/unchecked and enabled/disabled. */
{
char buf[256], idBuf[256], shadId[256];

if(id)
    safef(idBuf, sizeof(idBuf), " id=\"%s\"", id);
else
    idBuf[0] = 0;

printf("<INPUT TYPE=CHECKBOX NAME=\"%s\"%s VALUE=on %s%s%s>", name, idBuf,
        (moreHtml ? moreHtml : ""),
        (checked ? " CHECKED" : ""),
        (enabled ? "" : " DISABLED"));
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
if (id)
    safef(shadId, sizeof(shadId), "%s%s", cgiBooleanShadowPrefix(), id);
cgiMakeHiddenVarWithIdExtra(buf, id ? shadId : NULL, ( enabled ? "0" : (checked ? "-1" : "-2")),BOOLSHAD_EXTRA);
}

void cgiMakeCheckBoxUtil(char *name, boolean checked, char *msg, char *id)
/* Make check box - can be called directly, though it was originally meant
 * as the common code for all lower level checkbox routines.
 * However, it's util functionality has been taken over by
 * cgiMakeCheckBoxWithIdAndOptionalHtml() */
{
char buf[256];

if (msg)
    safef(buf, sizeof(buf), "TITLE=\"%s\"", msg);
else
    buf[0] = 0;

cgiMakeCheckBox2Bool(name, checked, TRUE, id, buf);
}

void cgiMakeCheckBoxWithMsg(char *name, boolean checked, char *msg)
{
char buf[512];
safef(buf, sizeof(buf), "title='%s'", msg);
cgiMakeCheckBox2Bool(name, checked, TRUE, NULL, buf);
}

void cgiMakeCheckBoxWithId(char *name, boolean checked, char *id)
/* Make check box, which includes an ID. */
{
cgiMakeCheckBox2Bool(name, checked, TRUE, id, NULL);
}

void cgiMakeCheckBox(char *name, boolean checked)
/* Make check box. */
{
cgiMakeCheckBox2Bool(name, checked, TRUE, NULL, NULL);
}

void cgiMakeCheckBoxEnabled(char *name, boolean checked, boolean enabled)
/* Make check box, optionally enabled/disabled. */
{
cgiMakeCheckBox2Bool(name, checked, enabled, NULL, NULL);
}

void cgiMakeCheckBoxMore(char *name, boolean checked, char *moreHtml)
/* Make check box with moreHtml. */
{
cgiMakeCheckBox2Bool(name,checked,TRUE,NULL,moreHtml);
}

void cgiMakeCheckBoxIdAndMore(char *name, boolean checked, char *id, char *moreHtml)
/* Make check box with ID and extra (non-javascript) html. */
{
cgiMakeCheckBox2Bool(name,checked,TRUE,id,moreHtml);
}

void cgiMakeCheckBoxFourWay(char *name, boolean checked, boolean enabled, char *id,
                            char *classes, char *moreHtml)
/* Make check box - with fourWay functionality (checked/unchecked by enabled/disabled)
 * Also makes a shadow hidden variable that supports the 2 boolean states. */
{
char shadName[256];
char shadId[256];

printf("<INPUT TYPE=CHECKBOX NAME='%s'", name);
if (id)
    printf(" id='%s'", id);
if (checked)
    printf(" CHECKED");
if (!enabled)
    {
    if (findWordByDelimiter("disabled",' ', classes) == NULL) // fauxDisabled ?
        printf(" DISABLED");
    }
if (classes)
    printf(" class='%s'",classes);
if (moreHtml)
    printf(" %s",moreHtml);
printf(">");

// The hidden var needs to hold the 4way state
safef(shadName, sizeof(shadName), "%s%s", cgiBooleanShadowPrefix(), name);
if (id)
    safef(shadId, sizeof(shadId), "%s%s", cgiBooleanShadowPrefix(), id);
cgiMakeHiddenVarWithIdExtra(shadName, id ? shadId : NULL, ( enabled ? "0" : (checked ? "-1" : "-2")),BOOLSHAD_EXTRA);
}


void cgiMakeHiddenBoolean(char *name, boolean on)
/* Make hidden boolean variable. Also make a shadow hidden variable so we
 * can distinguish between variable not present and
 * variable set to false. */
{
char buf[256];
cgiMakeHiddenVar(name, on ? "on" : "off");
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
cgiMakeHiddenVarWithExtra(buf, "1",BOOLSHAD_EXTRA);
}

void cgiMakeDoubleVar(char *varName, double initialVal, int maxDigits)
/* Make a text control filled with initial floating-point value.  */
{
if (maxDigits == 0) maxDigits = 4;

printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%g>", varName,
        maxDigits, initialVal);
}

void cgiMakeDoubleVarWithExtra(char *varName, double initialVal, int maxDigits, char *extra)
/* Make a text control filled with initial value and optional extra HTML.  */
{
if (maxDigits == 0) maxDigits = 4;
printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%g %s>", varName,
        maxDigits, initialVal, emptyForNull(extra));
}

void cgiMakeDoubleVarInRange(char *varName, double initialVal, char *title, int width,
                             char *min, char *max)
/* Make a floating point control filled with initial value.
   If min and/or max are non-NULL will enforce range
   Requires utils.js jQuery.js and inputBox class */
{
if (width==0)
    {
    if (max)
        width=strlen(max)*10;
    }
if (width < 65)
    width = 65;

printf("<INPUT TYPE=TEXT class='inputBox' name='%s' id='%s' style='width: %dpx' value=%s",
       varName,varName,width,shorterDouble(initialVal));
jsOnEventByIdF("change", varName, "return validateFloat(this,%s,%s);",
       (min ? min : "\"null\""),(max ? max : "\"null\""));
if (title)
    printf(" title='%s'",title);
printf(">\n");
}

void cgiMakeDoubleVarWithLimits(char *varName, double initialVal, char *title, int width,
                                double min, double max)
{
char minLimit[20];
char maxLimit[20];
char *minStr=NULL;
char *maxStr=NULL;
if ((int)min != NO_VALUE)
    {
    safef(minLimit,sizeof(minLimit),"%s",shorterDouble(min));
    minStr = minLimit;
    }
if ((int)max != NO_VALUE)
    {
    safef(maxLimit,sizeof(maxLimit),"%s",shorterDouble(max));
    maxStr = maxLimit;
    }
cgiMakeDoubleVarInRange(varName,initialVal,title,width,minStr,maxStr);
}

void cgiMakeDoubleVarWithMin(char *varName, double initialVal, char *title, int width, double min)
{
char minLimit[20];
char *minStr=NULL;
if ((int)min != NO_VALUE)
    {
    safef(minLimit,sizeof(minLimit),"%g",min);
    minStr = minLimit;
    }
cgiMakeDoubleVarInRange(varName,initialVal,title,width,minStr,NULL);
}

void cgiMakeDoubleVarWithMax(char *varName, double initialVal, char *title, int width, double max)
{
char maxLimit[20];
char *maxStr=NULL;
if ((int)max != NO_VALUE)
    {
    safef(maxLimit,sizeof(maxLimit),"%g",max);
    maxStr = maxLimit;
    }
cgiMakeDoubleVarInRange(varName,initialVal,title,width,NULL,maxStr);
}

void cgiMakeDropListClassWithIdStyleAndJavascript(char *name, char *id, char *menu[],
        int menuSize, char *checked, char *class, char *style, struct slPair *events)
/* Make a drop-down list with name, id, text class, style and javascript. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
printf("<SELECT");
if (name)
    printf(" NAME='%s'", name);
if (events && !id)  // use name as id
    id = name;
if (id)
    printf(" id='%s'", id);
if (class)
    printf(" class='%s'", class);
if (events)
    {
    struct slPair *e;
    for(e = events; e; e = e->next)
	{
	jsOnEventById(e->name, id, e->val);
	}    
    }
if (style)
    printf(" style='%s'", style);
printf(">\n");
for (i=0; i<menuSize; ++i)
    {
    if (sameWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>\n", selString, menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeDropListClassWithStyleAndJavascript(char *name, char *menu[],
        int menuSize, char *checked, char *class, char *style, struct slPair *events)
/* Make a drop-down list with names, text class, style and javascript. */
{
cgiMakeDropListClassWithIdStyleAndJavascript(name,NULL,menu,menuSize,checked,class,style,events);
}

void cgiMakeDropListClassWithStyle(char *name, char *menu[],
                                   int menuSize, char *checked, char *class, char *style)
/* Make a drop-down list with names, text class and style. */
{
cgiMakeDropListClassWithStyleAndJavascript(name,menu,menuSize,checked,class,style,NULL);
}

void cgiMakeDropListClass(char *name, char *menu[],
                          int menuSize, char *checked, char *class)
/* Make a drop-down list with names. */
{
cgiMakeDropListClassWithStyle(name, menu, menuSize, checked, class, NULL);
}

void cgiMakeDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names.
 * uses style "normalText" */
{
    cgiMakeDropListClass(name, menu, menuSize, checked, "normalText");
}

char *cgiMultListShadowPrefix()
/* Prefix for shadow variable set with multi-select inputs. */
{
return "multishad.";
}

void cgiMakeMultList(char *name, char *menu[], int menuSize, struct slName *checked, int length)
/* Make a list of names with window height equalt to length,
 * which can have multiple selections. Same as drop-down list
 * except "multiple" is added to select tag. */
{
int i;
char *selString;
if (checked == NULL) checked = slNameNew(menu[0]);
printf("<SELECT MULTIPLE SIZE=%d ALIGN=CENTER NAME=\"%s\">\n", length, name);
for (i=0; i<menuSize; ++i)
    {
    if (slNameInList(checked, menu[i]))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>\n", selString, menu[i]);
    }
printf("</SELECT>\n");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "1");
}

void cgiMakeCheckboxGroupWithVals(char *name, char *menu[], char *values[], int menuSize,
				  struct slName *checked, int tableColumns)
/* Make a table of checkboxes that have the same variable name but different
 * values (same behavior as a multi-select input), with nice labels in menu[]. */
{
int i;
if (values == NULL) values = menu;
if (menu == NULL) menu = values;
puts("<TABLE BORDERWIDTH=0><TR>");
for (i = 0;  i < menuSize;  i++)
    {
    if (i > 0 && (i % tableColumns) == 0)
	printf("</TR><TR>");
    printf("<TD><INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=\"%s\" %s></TD>"
	   "<TD>%s</TD>\n",
	   name, values[i], (slNameInList(checked, values[i]) ? "CHECKED" : ""),
	   menu[i]);
    }
if ((i % tableColumns) != 0)
    while ((i++ % tableColumns) != 0)
	printf("<TD></TD>");
puts("</TR></TABLE>");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "0");
}

void cgiMakeCheckboxGroup(char *name, char *menu[], int menuSize, struct slName *checked,
			  int tableColumns)
/* Make a table of checkboxes that have the same variable name but different
 * values (same behavior as a multi-select input). */
{
cgiMakeCheckboxGroupWithVals(name, menu, NULL, menuSize, checked, tableColumns);
}

void cgiMakeDropListFullExt(char *name, char *menu[], char *values[],
                         int menuSize, char *checked, char *event, char *javascript, char *style, char *class)
/* Make a drop-down list with names and values.
 * Optionally include values for style and class */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];

printf("<SELECT NAME='%s'", name);
if (class)
    printf(" class='%s'", class);
if (javascript)
    {
    printf(" id='%s'", name);
    jsOnEventById(event, name, javascript);
    }
if (style)
    printf(" style='%s'", style);
printf(">\n");

for (i=0; i<menuSize; ++i)
    {
    if (sameWord(values[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s VALUE=\"%s\">%s</OPTION>\n", selString, values[i], menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeDropListFull(char *name, char *menu[], char *values[],
                         int menuSize, char *checked, char *event, char *javascript)
/* Make a table of checkboxes that have the same variable name but different
 * values (same behavior as a multi-select input). */
{
cgiMakeDropListFullExt(name, menu, values, menuSize, checked, event, javascript, NULL, NULL);
}

char *cgiMakeSelectDropList(boolean multiple, char *name, struct slPair *valsAndLabels,
                            char *selected, char *anyAll,char *extraClasses, char *event, char *javascript, char *style, char *id)
// Returns allocated string of HTML defining a drop-down select
// (if multiple, REQUIRES ui-dropdownchecklist.js)
// valsAndLabels: val (pair->name) must be filled in but label (pair->val) may be NULL.
// selected: if not NULL is a val found in the valsAndLabels (multiple then comma delimited list).
//           If null and anyAll not NULL, that will be selected
// anyAll: if not NULL is the string for an initial option. It can contain val and label,
//         delimited by a comma
// event: click, etc.
// javacript: what to execute when the event happens.
// style: inline style
// id is optional
{
struct dyString *output = dyStringNew(1024);
boolean checked = FALSE;

dyStringPrintf(output,"<SELECT name='%s'",name);
if (multiple)
    dyStringAppend(output," MULTIPLE");
if (extraClasses != NULL)
    dyStringPrintf(output," class='%s%s'",extraClasses,(multiple ? " filterBy" : ""));
else if (multiple)
    dyStringAppend(output," class='filterBy'");

char *autoId = NULL;
if (javascript)
    {
    if (!id)
	{
    	id = name;
	}
    }
if (id)
    dyStringPrintf(output, " id='%s'", id);
if (javascript)
    {
    jsOnEventById(event, id, javascript);
    freeMem(autoId);
    }

if (style)
    {
    dyStringPrintf(output, " style='%s'", style);
    }

dyStringAppend(output,">\n");

// Handle initial option "Any" or "All"
if (anyAll != NULL)
    {
    char *val = anyAll;  // Could contain a label after the value
    char *label = strchr(val,',');  // Could contain a label after the value
    if (label != NULL)
        {
        val = cloneString(anyAll);
        label = strchr(val,',');  // again because this is new mem
        *label = '\0';
        label = label+1;
        }
    else
        label = val;
    checked = TRUE; // The default case
    if (selected != NULL)
        {
        if (multiple)
            checked = (findWordByDelimiter(val,',', selected) != NULL);
        else
            checked = sameString(val,selected);
        }
    dyStringPrintf(output, "<OPTION%s VALUE='%s'>%s</OPTION>\n",(checked ? " SELECTED" : ""),
                   val, javaScriptLiteralEncode(label));
    if (label != val)
        freeMem(val);
    }

// All other options
struct slPair *valPair = valsAndLabels;
for (; valPair != NULL; valPair = valPair->next)
    {
    checked = FALSE;
    if (selected != NULL)
        {
        if (multiple)
            checked = (findWordByDelimiter(valPair->name,',', selected) != NULL);
        else
            checked = sameString(valPair->name,selected);
        }
    char *label = valPair->name;
    if (valPair->val != NULL)
        label = valPair->val;
    dyStringPrintf(output, "<OPTION%s VALUE='%s'>%s</OPTION>\n",(checked ? " SELECTED" : ""),
                   (char *)valPair->name, javaScriptLiteralEncode(label));
    }

dyStringPrintf(output,"</SELECT>\n");

return dyStringCannibalize(&output);
}

void cgiMakeDropListWithVals(char *name, char *menu[], char *values[],
                         int menuSize, char *checked)
/* Make a drop-down list with names and values. In this case checked
 * corresponds to a value, not a menu. */
{
int i;
char *selString;
if (checked == NULL) checked = values[0];

printf("<SELECT NAME=\"%s\">\n", name);
for (i=0; i<menuSize; ++i)
    {
    if (sameWord(values[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s VALUE=\"%s\">%s</OPTION>\n", selString, values[i], menu[i]);
    }
printf("</SELECT>\n");
}

void cgiDropDownWithTextValsAndExtra(char *name, char *text[], char *values[],
                                     int count, char *selected, char *extra)
/* Make a drop-down list with both text and values. */
{
int i;
char *selString;
assert(values != NULL && text != NULL);
if (selected == NULL)
    selected = values[0];
printf("<SELECT");
if (name)
    printf(" NAME='%s'", name);
if (extra)
    printf("%s", extra);
printf(">\n");
for (i=0; i<count; ++i)
    {
    if (sameWord(values[i], selected))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s value='%s'>%s</OPTION>\n", selString, values[i], text[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeHiddenVarWithIdExtra(char *varName, char *id, char *string,char *extra)
/* Store string in hidden input for next time around. */
{
printf("<INPUT TYPE=HIDDEN NAME='%s'", varName);
if (id)
    printf(" ID='%s'", id);
if (extra)
    printf(" %s",extra);
printf(" VALUE='%s'>\n", string);
}

struct dyString *cgiUrlString()
/* Get URL-formatted that expresses current CGI variable state. */
{
struct dyString *dy = dyStringNew(0);
struct cgiVar *cv;
char *e;


for (cv = inputList; cv != NULL; cv = cv->next)
    {
    if (cv != inputList)
       dyStringAppend(dy, "&");
    e = cgiEncode(cv->val);
    dyStringPrintf(dy, "%s=", cv->name);
    dyStringAppend(dy, e);
    freez(&e);
    }
return dy;
}

void cgiEncodeIntoDy(char *var, char *val, struct dyString *dy)
/* Add a CGI-encoded &var=val string to dy. */
{
if (dy->stringSize != 0)
    dyStringAppendC(dy, '&');
dyStringAppend(dy, var);
dyStringAppendC(dy, '=');
char *s = cgiEncode(val);
dyStringAppend(dy, s);
freez(&s);
}

void cgiContinueAllVars()
/* Write back all CGI vars as hidden input for next time around. */
{
struct cgiVar *cv;
for (cv = inputList; cv != NULL; cv = cv->next)
    cgiMakeHiddenVar(cv->name, cv->val);
}

void cgiResetState()
/* This is for reloading CGI settings multiple times in the same program
 * execution.  No effect if state has not yet been initialized. */
{
freez(&inputString);
inputString = NULL;
if (inputHash != NULL)
    hashFree(&inputHash);
inputHash = NULL;
inputList = NULL;

haveCookiesHash = FALSE;
if (cookieHash != NULL)
    hashFree(&cookieHash);
cookieHash = NULL;
cookieList = NULL;
}

void cgiDown(float lines)
// Drop down a certain number of lines (may be fractional)
{
printf("<div style='height:%fem;'></div>\n",lines);
}

char *commonCssStyles()
/* Returns a string of common CSS styles */
{
// Contents currently is OBSOLETE as these have been moved to HGStyle.css
// TODO: remove all traces (from web.c, hgTracks, hgTables) as this funtion does nothing.
return "";
}

static void turnCgiVarsToVals(struct hashEl *hel)
/* To save cgiParseVars clients from doing an extra lookup, replace
 * hash filled with cgiVars as values with one filled with cgiVar->val
 * instead.  Since cgiVar->name is same as hashEl->name,  no info is really
 * lost, and it simplifies the code that uses us. */
{
struct cgiVar *var = hel->val;
hel->val = var->val;
}

struct cgiParsedVars *cgiParsedVarsNew(char *cgiString)
/* Build structure containing parsed out cgiString */
{
struct cgiParsedVars *tags;
AllocVar(tags);
tags->stringBuf = cloneString(cgiString);
cgiParseInputAbort(tags->stringBuf, &tags->hash, &tags->list);
hashTraverseEls(tags->hash, turnCgiVarsToVals);
return tags;
}

void cgiParsedVarsFree(struct cgiParsedVars **pTags)
/* Free up memory associated with cgiParsedVars */
{
struct cgiParsedVars *tags = *pTags;
if (tags != NULL)
    {
    slFreeList(&tags->list);
    hashFree(&tags->hash);
    freeMem(tags->stringBuf);
    freez(pTags);
    }
}

void cgiParsedVarsFreeList(struct cgiParsedVars **pList)
/* Free up list of cgiParsedVars */
{
struct cgiParsedVars *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cgiParsedVarsFree(&el);
    }
*pList = NULL;
}

void cgiEncodeHash(struct hash *hash, struct dyString *dy)
/* Put a cgi-encoding of a string valued hash into dy.  Tags are always
 * alphabetical to make it easier to compare if two hashes are same. */
{
struct hashEl *el, *elList = hashElListHash(hash);
slSort(&elList, hashElCmp);
boolean firstTime = TRUE;
char *s = NULL;
for (el = elList; el != NULL; el = el->next)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	dyStringAppendC(dy, '&');
    dyStringAppend(dy, el->name);
    dyStringAppendC(dy, '=');
    s = cgiEncode(el->val);
    dyStringAppend(dy, s);
    freez(&s);
    }
hashElFreeList(&elList);
}
