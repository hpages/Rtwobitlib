/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* udc - url data cache - a caching system that keeps blocks of data fetched from URLs in
 * sparse local files for quick use the next time the data is needed. 
 *
 * This cache is enormously simplified by there being no local _write_ to the cache,
 * just reads.  
 *
 * The overall strategy of the implementation is to have a root cache directory
 * with a subdir for each file being cached.  The directory for a single cached file
 * contains two files - "bitmap" and "sparseData" that contains information on which
 * parts of the URL are cached and the actual cached data respectively. The subdirectory name
 * associated with the file is constructed from the URL in a straightforward manner.
 *     http://genome.ucsc.edu/cgi-bin/hgGateway
 * gets mapped to:
 *     rootCacheDir/http/genome.ucsc.edu/cgi-bin/hgGateway/
 * The URL protocol is the first directory under the root, and the remainder of the
 * URL, with some necessary escaping, is used to define the rest of the cache directory
 * structure, with each '/' after the protocol line translating into another directory
 * level.
 *    
 * The bitmap file contains time stamp and size data as well as an array with one bit
 * for each block of the file that has been fetched.  Currently the block size is 8K. */

#include <sys/file.h>
//#include <sys/mman.h>
#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "bits.h"
#include "linefile.h"
#include "obscure.h"
#include "portable.h"
#include "sig.h"
#include "cheapcgi.h"
#include "udc.h"
#include "hex.h"
#include <dirent.h>
#include <openssl/sha.h>

/* The stdio stream we'll use to output statistics on file i/o.  Off by default. */
FILE *udcLogStream = NULL;

void udcSetLog(FILE *fp)
/* Turn on logging of file i/o. 
 * For each UDC file two lines are written.  One line for the open, and one line for the close. 
 * The Open line just has the URL being opened.
 * The Close line has the the URL plus a bunch of counts of the number of seeks, reads, and writes
 *   for the following four files: the udc bitmap, the udc sparse data, the incoming calls
 *   to the UDC layer, and the network connection to the (possibly) remote file.
 *   There are two additional counts: the number of socket connects, and the 
 *   number of times a socket is reused instead of closed and reopened.
 */
{
udcLogStream = fp;
fprintf(fp, "Begin\n");
}

struct ioStats
/* Statistics concerning reads and seeks. */
    {
    bits64 numSeeks;            /* The number of seeks on this file */
    bits64 numReads;            /* The number of reads from this file */
    bits64 bytesRead;           /* The number of bytes read from this file */
    bits64 numWrites;           /* The number of writes to this file */
    bits64 bytesWritten;        /* The number of bytes written to this file */
    };

struct ios
/* Statistics concerning reads and seeks for sparse, bitmap, url, and to us. */
    {
    struct ioStats bit;         /* Statistics on file i/o to the bitmap file. */
    struct ioStats sparse;      /* Statistics on file i/o to the sparse data file. */
    struct ioStats udc;         /* Statistics on file i/o from the application to us. */
    struct ioStats net;         /* Statistics on file i/o over the network. */
    bits64 numConnects;         /* The number of socket connections made. */
    bits64 numReuse;            /* The number of socket reuses. */
    };

#define udcBlockSize (8*1024)
/* All fetch requests are rounded up to block size. */

#define udcMaxBytesPerRemoteFetch (udcBlockSize * 32)
/* Very large remote reads are broken down into chunks this size. */

struct connInfo
/* Socket descriptor and associated info, for keeping net connections open. */
    {
    int socket;                 /* Socket descriptor for data connection (or 0). */
    bits64 offset;		/* Current file offset of socket. */
    int ctrlSocket;             /* (FTP only) Control socket descriptor or 0. */
    char *redirUrl;             /* (HTTP(S) only) use redirected url */
    char *resolvedUrl;          /* resolved HTTPS URL, if url is a pseudo-URL (s3://) */
    };

typedef int (*UdcDataCallback)(char *url, bits64 offset, int size, void *buffer,
			       struct udcFile *file);
/* Type for callback function that fetches file data. */

struct udcRemoteFileInfo
/* Information about a remote file. */
    {
    bits64 updateTime;	/* Last update in seconds since 1970 */
    bits64 size;	/* Remote file size */
    struct connInfo ci; /* Connection info for open net connection */
    };

typedef boolean (*UdcInfoCallback)(char *url, struct udcRemoteFileInfo *retInfo);
/* Type for callback function that fetches file timestamp and size. */

struct udcProtocol
/* Something to handle a communications protocol like http, https, ftp, local file i/o, etc. */
    {
    struct udcProtocol *next;	/* Next in list */
    UdcDataCallback fetchData;	/* Data fetcher */
    UdcInfoCallback fetchInfo;	/* Timestamp & size fetcher */
    char *type;
    };

struct udcFile
/* A file handle for our caching system. */
    {
    struct udcFile *next;	/* Next in list. */
    char *url;			/* Name of file - includes protocol */
    char *protocol;		/* The URL up to the first colon.  http: etc. */
    struct udcProtocol *prot;	/* Protocol specific data and methods. */
    time_t updateTime;		/* Last modified timestamp. */
    bits64 size;		/* Size of file. */
    bits64 offset;		/* Current offset in file. */
    char *cacheDir;		/* Directory for cached file parts. */
    char *bitmapFileName;	/* Name of bitmap file. */
    char *sparseFileName;	/* Name of sparse data file. */
    char *redirFileName;	/* Name of redir file. */
    char *resolvedFileName;     /* Name of file that stores final, resolved URL */
    int fdSparse;		/* File descriptor for sparse data file. */
    boolean sparseReadAhead;    /* Read-ahead has something in the buffer */
    char *sparseReadAheadBuf;   /* Read-ahead buffer, if any */
    bits64 sparseRAOffset;      /* Read-ahead buffer offset */
    struct udcBitmap *bits;     /* udcBitMap */
    bits64 startData;		/* Start of area in file we know to have data. */
    bits64 endData;		/* End of area in file we know to have data. */
    bits32 bitmapVersion;	/* Version of associated bitmap we were opened with. */
    struct connInfo connInfo;   /* Connection info for open net connection. */
    struct ios ios;             /* Statistics on file access. */
    };

struct udcBitmap
/* The control structure including the bitmap of blocks that are cached. */
    {
    struct udcBitmap *next;	/* Next in list. */
    bits32 blockSize;		/* Number of bytes per block of file. */
    bits64 remoteUpdate;	/* Remote last update time. */
    bits64 fileSize;		/* File size */
    bits32 version;		/* Version - increments each time cache is stale. */
    bits64 localUpdate;		/* Time we last fetched new data into cache. */
    bits64 localAccess;		/* Time we last accessed data. */
    boolean isSwapped;		/* If true need to swap all bytes on read. */
    int fd;			/* File descriptor for file with current block. */
    };
static char *bitmapName = "bitmap";
static char *sparseDataName = "sparseData";
static char *redirName = "redir";
static char *resolvedName = "resolv";

#define udcBitmapHeaderSize (64)

#define MAX_SKIP_TO_SAVE_RECONNECT (udcMaxBytesPerRemoteFetch / 2)

/* pseudo-URLs with this prefix (e.g. "s3://" get run through a command to get resolved to real HTTPS URLs) */
struct slName *resolvProts = NULL;
static char *resolvCmd = NULL;

bool udcIsResolvable(char *url) 
/* check if third-party protocol resolving (e.g. for "s3://") is enabled and if the url starts with a protocol handled by the resolver */
{
if (!resolvProts || !resolvCmd)
    return FALSE;

char *colon = strchr(url, ':');
if (!colon)
    return FALSE;

int colonPos = colon - url;
char *protocol = cloneStringZ(url, colonPos);
bool isFound = (slNameFind(resolvProts, protocol) != NULL);
if (isFound)
    verbose(4, "Check: URL %s has special protocol://, will need resolving\n", url);
freez(&protocol);
return isFound;
}

static off_t ourMustLseek(struct ioStats *ioStats, int fd, off_t offset, int whence)
{
ioStats->numSeeks++;
return mustLseek(fd, offset, whence);
}


static void ourMustRead(struct ioStats *ioStats, int fd, void *buf, size_t size)
{
ioStats->numReads++;
ioStats->bytesRead += size;
mustReadFd(fd, buf, size);
}

static size_t ourFread(struct ioStats *ioStats, void *buf, size_t size, size_t nmemb, FILE *stream)
{
ioStats->numReads++;
ioStats->bytesRead += size * nmemb;
return fread(buf, size, nmemb, stream);
}


/********* Section for local file protocol **********/

static char *assertLocalUrl(char *url)
/* Make sure that url is local and return bits past the protocol. */
{
if (startsWith("local:", url))
    url += 6;
if (url[0] != '/')
    errAbort("Local urls must start at /");
if (stringIn("..", url) || stringIn("~", url) || stringIn("//", url) ||
    stringIn("/./", url) || endsWith("/.", url))
    {
    errAbort("relative paths not allowed in local urls (%s)", url);
    }
return url;
}

static int udcDataViaLocal(char *url, bits64 offset, int size, void *buffer, struct udcFile *file)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
/* Need to check time stamp here. */
verbose(4, "reading remote data - %d bytes at %lld - on %s\n", size, offset, url);
url = assertLocalUrl(url);
FILE *f = mustOpen(url, "rb");
fseek(f, offset, SEEK_SET);
int sizeRead = ourFread(&file->ios.net, buffer, 1, size, f);
if (ferror(f))
    {
    warn("udcDataViaLocal failed to fetch %d bytes at %lld", size, offset);
    errnoAbort("file %s", url);
    }
carefulClose(&f);
return sizeRead;
}

static boolean udcInfoViaLocal(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retTime with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
verbose(4, "checking remote info on %s\n", url);
url = assertLocalUrl(url);
struct stat status;
int ret = stat(url, &status);
if (ret < 0)
    return FALSE;
retInfo->updateTime = status.st_mtime;
retInfo->size = status.st_size;
return TRUE;
}

/********* Section for transparent file protocol **********/

static int udcDataViaTransparent(char *url, bits64 offset, int size, void *buffer,
				 struct udcFile *file)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
internalErr();	/* Should not get here. */
return size;
}

static boolean udcInfoViaTransparent(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retInfo with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
internalErr();	/* Should not get here. */
return FALSE;
}

/********* Section for slow local file protocol - simulates network... **********/

/********* Section for http protocol **********/

static char *defaultDir = "/tmp/udcCache";
static bool udcInitialized = FALSE;

static void initializeUdc()
/* Use the $TMPDIR environment variable, if set, to amend the default location
 * of the cache */
{
if (udcInitialized)
    return;
char *tmpDir = getenv("TMPDIR");
if (isNotEmpty(tmpDir))
    {
    char buffer[2048];
    safef(buffer, sizeof(buffer), "%s/udcCache", tmpDir);
    udcSetDefaultDir(buffer);
    }
}

char *udcDefaultDir()
/* Get default directory for cache */
{
if (!udcInitialized)
    initializeUdc();
return defaultDir;
}

void udcSetDefaultDir(char *path)
/* Set default directory for cache.  */
{
defaultDir = cloneString(path);
udcInitialized = TRUE;
}

void udcDisableCache()
/* Switch off caching. Re-enable with udcSetDefaultDir */
{
defaultDir = NULL;
udcInitialized = TRUE;
}

/********* Section for ftp protocol **********/

// fetchData method: See udcDataViaHttpOrFtp above.


/********* Non-protocol-specific bits **********/

boolean udcFastReadString(struct udcFile *f, char buf[256])
/* Read a string into buffer, which must be long enough
 * to hold it.  String is in 'writeString' format. */
{
UBYTE bLen;
int len;
if (!udcReadOne(f, bLen))
    return FALSE;
if ((len = bLen)> 0)
    udcMustRead(f, buf, len);
buf[len] = 0;
return TRUE;
}

void msbFirstWriteBits64(FILE *f, bits64 x);

static char *fileNameInCacheDir(struct udcFile *file, char *fileName)
/* Return the name of a file in the cache dir, from the cache root directory on down.
 * Do a freeMem on this when done. */
{
int dirLen = strlen(file->cacheDir);
int nameLen = strlen(fileName);
char *path = needMem(dirLen + nameLen + 2);
memcpy(path, file->cacheDir, dirLen);
path[dirLen] = '/';
memcpy(path+dirLen+1, fileName, nameLen);
return path;
}

static struct udcBitmap *udcBitmapOpen(char *fileName)
/* Open up a bitmap file and read and verify header.  Return NULL if file doesn't
 * exist, abort on error. */
{
/* Open file, returning NULL if can't. */
int fd = open(fileName, O_RDWR);
if (fd < 0)
    {
    if (errno == ENOENT)
	return NULL;
    else
	errnoAbort("Can't open(%s, O_RDWR)", fileName);
    }

/* Get status info from file. */
struct stat status;
fstat(fd, &status);
if (status.st_size < udcBitmapHeaderSize) // check for truncated invalid bitmap files.
    {
    close(fd);
    return NULL;  // returning NULL will cause the fresh creation of bitmap and sparseData files.
    }  

/* Read signature and decide if byte-swapping is needed. */
// TODO: maybe buffer the I/O for performance?  Don't read past header - 
// fd offset needs to point to first data block when we return.
bits32 magic;
boolean isSwapped = FALSE;
mustReadOneFd(fd, magic);
if (magic != udcBitmapSig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != udcBitmapSig)
       errAbort("%s is not a udcBitmap file", fileName);
    }

/* Allocate bitmap object, fill it in, and return it. */
struct udcBitmap *bits;
AllocVar(bits);
bits->blockSize = fdReadBits32(fd, isSwapped);
bits->remoteUpdate = fdReadBits64(fd, isSwapped);
bits->fileSize = fdReadBits64(fd, isSwapped);
bits->version = fdReadBits32(fd, isSwapped);
fdReadBits32(fd, isSwapped); // ignore result
fdReadBits64(fd, isSwapped); // ignore result
fdReadBits64(fd, isSwapped); // ignore result
fdReadBits64(fd, isSwapped); // ignore result
fdReadBits64(fd, isSwapped); // ignore result
bits->localUpdate = status.st_mtime;
bits->localAccess = status.st_atime;
bits->isSwapped = isSwapped;
bits->fd = fd;

return bits;
}

static void udcBitmapClose(struct udcBitmap **pBits)
/* Free up resources associated with udcBitmap. */
{
struct udcBitmap *bits = *pBits;
if (bits != NULL)
    {
    mustCloseFd(&(bits->fd));
    freez(pBits);
    }
}

static struct udcProtocol *udcProtocolNew(char *upToColon)
/* Build up a new protocol around a string such as "http" or "local" */
{
struct udcProtocol *prot;
AllocVar(prot);
if (sameString(upToColon, "local"))
    {
    prot->fetchData = udcDataViaLocal;
    prot->fetchInfo = udcInfoViaLocal;
    prot->type = "local";
    }
else if (sameString(upToColon, "transparent"))
    {
    prot->fetchData = udcDataViaTransparent;
    prot->fetchInfo = udcInfoViaTransparent;
    prot->type = "transparent";
    }
else
    {
    errAbort("Unrecognized protocol %s in udcProtNew", upToColon);
    }
return prot;
}

static void udcProtocolFree(struct udcProtocol **pProt)
/* Free up protocol resources. */
{
freez(pProt);
}

static boolean qEscaped(char c)
/* Returns TRUE if character needs to be escaped in q-encoding. */
{
if (isalnum(c))
    return c == 'Q';
else
    return c != '_' && c != '-' && c != '/' && c != '.';
}

static char *qEncode(char *input)
/* Do a simple encoding to convert input string into "normal" characters.
 * Abnormal letters, and '!' get converted into Q followed by two hexadecimal digits. */
{
/* First go through and figure out encoded size. */
int size = 0;
char *s, *d, c;
s = input;
while ((c = *s++) != 0)
    {
    if (qEscaped(c))
	size += 3;
    else
	size += 1;
    }

/* Allocate and fill in output. */
char *output = needMem(size+1);
s = input;
d = output;
while ((c = *s++) != 0)
    {
    if (qEscaped(c))
        {
	sprintf(d, "Q%02X", (unsigned)c);
	d += 3;
	}
    else
        *d++ = c;
    }
return output;
}

void udcParseUrlFull(char *url, char **retProtocol, char **retAfterProtocol, char **retColon,
		     char **retAuth)
/* Parse the URL into components that udc treats separately.
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free all *ret's except *retColon when done. */
{
char *protocol, *afterProtocol;
char *colon = strchr(url, ':');
if (!colon)
    {
    *retColon = NULL;
    return;
    }
int colonPos = colon - url;
protocol = cloneStringZ(url, colonPos);
afterProtocol = url + colonPos + 1;
while (afterProtocol[0] == '/')
   afterProtocol += 1;
char *userPwd = strchr(afterProtocol, '@');
if (userPwd)
    {
    if (retAuth)
	{
	char auth[1024];
	safencpy(auth, sizeof(auth), afterProtocol, userPwd+1-afterProtocol);
	*retAuth = qEncode(auth);
	}
    char *afterHost = strchr(afterProtocol, '/');
    if (!afterHost)
	{
	afterHost = afterProtocol+strlen(afterProtocol);
	}
    if (userPwd < afterHost)
	afterProtocol = userPwd + 1;
    }
else if (retAuth)
    *retAuth = NULL;
afterProtocol = qEncode(afterProtocol);
*retProtocol = protocol;
*retAfterProtocol = afterProtocol;
*retColon = colon;
}

void udcParseUrl(char *url, char **retProtocol, char **retAfterProtocol, char **retColon)
/* Parse the URL into components that udc treats separately.
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free  *retProtocol and *retAfterProtocol but not *retColon when done. */
{
udcParseUrlFull(url, retProtocol, retAfterProtocol, retColon, NULL);
}

static void addElementToDy(struct dyString *dy, int maxLen, char *name)
/* add one element of a path to a dyString, hashing it if it's longer 
 * than NAME_MAX */
{
if (strlen(name) > maxLen)
    {
    unsigned char hash[SHA_DIGEST_LENGTH];
    char newName[(SHA_DIGEST_LENGTH + 1) * 2];

    SHA1((const unsigned char *)name, strlen(name), hash);
    hexBinaryString(hash,  SHA_DIGEST_LENGTH, newName, (SHA_DIGEST_LENGTH + 1) * 2);
    
    dyStringAppend(dy, newName);
    }
else
    dyStringAppend(dy, name);
}

static char *longDirHash(char *cacheDir, char *name)
/* take a path and hash the elements that are longer than NAME_MAX */
{
int maxLen = pathconf(cacheDir, _PC_NAME_MAX);
if (maxLen < 0)   // if we can't get the real system max, assume it's 255
    maxLen = 255;
struct dyString *dy = dyStringNew(strlen(name));
char *ptr = strchr(name, '/');

while(ptr)
    {
    *ptr = 0;
    addElementToDy(dy, maxLen, name);

    dyStringAppend(dy, "/");

    name = ptr + 1;
    ptr = strchr(name, '/');
    }

addElementToDy(dy, maxLen, name);

return dyStringCannibalize(&dy);
}

void udcPathAndFileNames(struct udcFile *file, char *cacheDir, char *protocol, char *afterProtocol)
/* Initialize udcFile path and names */
{
if (cacheDir==NULL)
    return;
char *hashedAfterProtocol = longDirHash(cacheDir, afterProtocol);
int len = strlen(cacheDir) + 1 + strlen(protocol) + 1 + strlen(hashedAfterProtocol) + 1;
file->cacheDir = needMem(len);
safef(file->cacheDir, len, "%s/%s/%s", cacheDir, protocol, hashedAfterProtocol);
verbose(4, "UDC dir: %s\n", file->cacheDir);

/* Create file names for bitmap and data portions. */
file->bitmapFileName = fileNameInCacheDir(file, bitmapName);
file->sparseFileName = fileNameInCacheDir(file, sparseDataName);
file->redirFileName = fileNameInCacheDir(file, redirName);
file->resolvedFileName = fileNameInCacheDir(file, resolvedName);
}

static long long int udcSizeAndModTimeFromBitmap(char *bitmapFileName, time_t *retTime)
/* Look up the file size from the local cache bitmap file, or -1 if there
 * is no cache for url. If retTime is non-null, store the remote update time in it. */
{
long long int ret = -1;
struct udcBitmap *bits = udcBitmapOpen(bitmapFileName);
if (bits != NULL)
    {
    ret = bits->fileSize;
    if (retTime)
	*retTime = bits->remoteUpdate;
    }
udcBitmapClose(&bits);
return ret;
}

struct udcFile *udcFileMayOpen(char *url, char *cacheDir)
/* Open up a cached file. cacheDir may be null in which case udcDefaultDir() will be
 * used.  Return NULL if file doesn't exist. 
 * Caching is inactive if defaultDir is NULL or the protocol is "transparent".
 * */
{
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
verbose(4, "udcfileOpen(%s, %s)\n", url, cacheDir);
if (udcLogStream)
    fprintf(udcLogStream, "Open %s\n", url);
/* Parse out protocol.  Make it "transparent" if none specified. */
char *protocol = NULL, *afterProtocol = NULL, *colon;
boolean isTransparent = FALSE;
udcParseUrl(url, &protocol, &afterProtocol, &colon);
if (!colon)
    {
    freeMem(protocol);
    protocol = cloneString("transparent");
    freeMem(afterProtocol);
    afterProtocol = cloneString(url);
    isTransparent = TRUE;
    }
struct udcProtocol *prot;
prot = udcProtocolNew(protocol);

/* Figure out if anything exists. */
boolean useCacheInfo = FALSE;
struct udcRemoteFileInfo info;
ZeroVar(&info);
if (!isTransparent)
    {
    if (!useCacheInfo)
	{
	if (!prot->fetchInfo(url, &info))
	    {
	    udcProtocolFree(&prot);
	    freeMem(protocol);
	    freeMem(afterProtocol);
	    return NULL;
	    }
	}
    }

if (useCacheInfo)
    verbose(4, "Cache is used for %s", url);
else
    verbose(4, "Cache is not used for %s", url);

/* Allocate file object and start filling it in. */
struct udcFile *file;
AllocVar(file);
file->url = cloneString(url);
file->protocol = protocol;
file->prot = prot;
if (isTransparent)
    {
    /* If transparent dummy up things so that the "sparse" file pointer is actually
     * the file itself, which appears to be completely loaded in cache. */
    if (!fileExists(url))
	return NULL;
    int fd = file->fdSparse = mustOpenFd(url, O_RDONLY);
    struct stat status;
    fstat(fd, &status);
    file->startData = 0;
    file->endData = file->size = status.st_size;
    }
else 
    {
    udcPathAndFileNames(file, cacheDir, protocol, afterProtocol);

    file->connInfo.resolvedUrl = info.ci.resolvedUrl; // no need to resolve again if udcInfoViaHttp already did that
    if (udcIsResolvable(file->url) && !file->connInfo.resolvedUrl)
	errAbort("internal error in udcFileMayOpen(): udcLoadCachedResolvedUrl() not available");
        //udcLoadCachedResolvedUrl(file);

    if (!useCacheInfo)
	{
	file->updateTime = info.updateTime;
	file->size = info.size;
	memcpy(&(file->connInfo), &(info.ci), sizeof(struct connInfo));
	}
    }
freeMem(afterProtocol);
return file;
}

struct udcFile *udcFileOpen(char *url, char *cacheDir)
/* Open up a cached file.  cacheDir may be null in which case udcDefaultDir() will be
 * used.  Abort if file doesn't exist. */
{
struct udcFile *udcFile = udcFileMayOpen(url, cacheDir);
if (udcFile == NULL)
    errAbort("Couldn't open %s", url);
return udcFile;
}


struct slName *udcFileCacheFiles(char *url, char *cacheDir)
/* Return low-level list of files used in cache. */
{
char *protocol, *afterProtocol, *colon;
struct udcFile *file;
udcParseUrl(url, &protocol, &afterProtocol, &colon);
if (colon == NULL)
    return NULL;
AllocVar(file);
udcPathAndFileNames(file, cacheDir, protocol, afterProtocol);
struct slName *list = NULL;
slAddHead(&list, slNameNew(file->bitmapFileName));
slAddHead(&list, slNameNew(file->sparseFileName));
slAddHead(&list, slNameNew(file->redirFileName));
slReverse(&list);
freeMem(file->cacheDir);
freeMem(file->bitmapFileName);
freeMem(file->sparseFileName);
freeMem(file);
freeMem(protocol);
freeMem(afterProtocol);
return list;
}

void udcFileClose(struct udcFile **pFile)
/* Close down cached file. */
{
struct udcFile *file = *pFile;
if (file != NULL)
    {
    if (udcLogStream)
        {
        fprintf(udcLogStream, "Close %s %s %lld %lld bit %lld %lld %lld %lld %lld sparse %lld %lld %lld %lld %lld udc  %lld %lld %lld %lld %lld net %lld %lld %lld %lld %lld \n",
           file->url, file->prot->type, file->ios.numConnects, file->ios.numReuse,
           file->ios.bit.numSeeks, file->ios.bit.numReads, file->ios.bit.bytesRead, file->ios.bit.numWrites,  file->ios.bit.bytesWritten, 
           file->ios.sparse.numSeeks, file->ios.sparse.numReads, file->ios.sparse.bytesRead, file->ios.sparse.numWrites,  file->ios.sparse.bytesWritten, 
           file->ios.udc.numSeeks, file->ios.udc.numReads, file->ios.udc.bytesRead, file->ios.udc.numWrites,  file->ios.udc.bytesWritten, 
           file->ios.net.numSeeks, file->ios.net.numReads, file->ios.net.bytesRead, file->ios.net.numWrites,  file->ios.net.bytesWritten);
        }
    if (file->connInfo.socket != 0)
	mustCloseFd(&(file->connInfo.socket));
    if (file->connInfo.ctrlSocket != 0)
	mustCloseFd(&(file->connInfo.ctrlSocket));
    freeMem(file->url);
    freeMem(file->protocol);
    udcProtocolFree(&file->prot);
    freeMem(file->cacheDir);
    freeMem(file->bitmapFileName);
    freeMem(file->sparseFileName);
    freeMem(file->sparseReadAheadBuf);
    if (file->fdSparse != 0)
        mustCloseFd(&(file->fdSparse));
    udcBitmapClose(&file->bits);
    }
freez(pFile);
}

static void qDecode(const char *input, char *buf, size_t size)
/* Reverse the qEncode performed on afterProcotol above into buf or abort. */
{
safecpy(buf, size, input);
char c, *r = buf, *w = buf;
while ((c = *r++) != '\0')
    {
    if (c == 'Q')
	{
	int q;
	if (sscanf(r, "%02X", &q))
	    {
	    *w++ = (char)q;
	    r += 2;
	    }
	else
	    errAbort("qDecode: input \"%s\" does not appear to be properly formatted "
		     "starting at \"%s\"", input, r);
	}
    else
	*w++ = c;
    }
*w = '\0';
}

char *udcPathToUrl(const char *path, char *buf, size_t size, char *cacheDir)
/* Translate path into an URL, store in buf, return pointer to buf if successful
 * and NULL if not. */
{
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
int offset = 0;
if (startsWith(cacheDir, (char *)path))
    offset = strlen(cacheDir);
if (path[offset] == '/')
    offset++;
char protocol[16];
strncpy(protocol, path+offset, sizeof(protocol));
protocol[ArraySize(protocol)-1] = '\0';
char *p = strchr(protocol, '/');
if (p == NULL)
    {
    errAbort("unable to parse protocol (first non-'%s' directory) out of path '%s'\n",
	     cacheDir, path);
    return NULL;
    }
*p++ = '\0';
char afterProtocol[4096];
qDecode(path+1+strlen(protocol)+1, afterProtocol, sizeof(afterProtocol));
safef(buf, size, "%s://%s", protocol, afterProtocol);
return buf;
}

long long int udcSizeFromCache(char *url, char *cacheDir)
/* Look up the file size from the local cache bitmap file, or -1 if there
 * is no cache for url. */
{
long long int ret = -1;
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
struct slName *sl, *slList = udcFileCacheFiles(url, cacheDir);
for (sl = slList;  sl != NULL;  sl = sl->next)
    if (endsWith(sl->name, bitmapName))
	{
	ret = udcSizeAndModTimeFromBitmap(sl->name, NULL);
	break;
	}
slNameFreeList(&slList);
return ret;
}

time_t udcTimeFromCache(char *url, char *cacheDir)
/* Look up the file datetime from the local cache bitmap file, or 0 if there
 * is no cache for url. */
{
time_t t = 0;
long long int ret = -1;
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
struct slName *sl, *slList = udcFileCacheFiles(url, cacheDir);
for (sl = slList;  sl != NULL;  sl = sl->next)
    if (endsWith(sl->name, bitmapName))
	{
	ret = udcSizeAndModTimeFromBitmap(sl->name, &t);
	if (ret == -1)
	    t = 0;
	break;
	}
slNameFreeList(&slList);
return t;
}

#define READAHEADBUFSIZE 4096
bits64 udcRead(struct udcFile *file, void *buf, bits64 size)
/* Read a block from file.  Return amount actually read. */
{
file->ios.udc.numReads++;
// if not caching, just fetch the data
if (!sameString(file->protocol, "transparent"))
    {
    int actualSize = file->prot->fetchData(file->url, file->offset, size, buf, file);
    file->offset += actualSize;
    file->ios.udc.bytesRead += actualSize;
    return actualSize;
    }
file->ios.udc.bytesRead += size;

/* Figure out region of file we're going to read, and clip it against file size. */
bits64 start = file->offset;
if (start > file->size)
    return 0;
bits64 end = start + size;
if (end > file->size)
    end = file->size;
size = end - start;
char *cbuf = buf;

/* use read-ahead buffer if present */
bits64 bytesRead = 0;

bits64 raStart;
bits64 raEnd;
while(TRUE)
    {
    if (file->sparseReadAhead)
	{
	raStart = file->sparseRAOffset;
	raEnd = raStart+READAHEADBUFSIZE;
	if (start >= raStart && start < raEnd)
	    {
	    // copy bytes out of rabuf
	    bits64 endInBuf = min(raEnd, end);
	    bits64 sizeInBuf = endInBuf - start;
	    memcpy(cbuf, file->sparseReadAheadBuf + (start-raStart), sizeInBuf);
	    cbuf += sizeInBuf;
	    bytesRead += sizeInBuf;
	    start = raEnd;
	    size -= sizeInBuf;
	    file->offset += sizeInBuf;
	    if (size == 0)
		break;
	    }
	file->sparseReadAhead = FALSE;
	ourMustLseek(&file->ios.sparse,file->fdSparse, start, SEEK_SET);
	}

    bits64 saveEnd = end;
    if (size < READAHEADBUFSIZE)
	{
	file->sparseReadAhead = TRUE;
	if (!file->sparseReadAheadBuf)
	    file->sparseReadAheadBuf = needMem(READAHEADBUFSIZE);
	file->sparseRAOffset = start;
	size = READAHEADBUFSIZE;
	end = start + size;
	if (end > file->size)
	    {
	    end = file->size;
	    size = end - start;
	    }
	}


    /* If we're outside of the window of file we already know is good, then have to
     * consult cache on disk, and maybe even fetch data remotely! */
    if (start < file->startData || end > file->endData)
	{
	/* Currently only need fseek here.  Would be safer, but possibly
	 * slower to move fseek so it is always executed in front of read, in
	 * case other code is moving around file pointer. */

	ourMustLseek(&file->ios.sparse,file->fdSparse, start, SEEK_SET);
	}

    if (file->sparseReadAhead)
	{
	ourMustRead(&file->ios.sparse,file->fdSparse, file->sparseReadAheadBuf, size);
	end = saveEnd;
	size = end - start;
	}
    else
	{
	ourMustRead(&file->ios.sparse,file->fdSparse, cbuf, size);
	file->offset += size;
	bytesRead += size;
	break;
	}
    }

return bytesRead;
}

void udcMustRead(struct udcFile *file, void *buf, bits64 size)
/* Read a block from file.  Abort if any problem, including EOF before size is read. */
{
bits64 sizeRead = udcRead(file, buf, size);
if (sizeRead < size)
    errAbort("udc couldn't read %llu bytes from %s, did read %llu", size, file->url, sizeRead);
}

int udcGetChar(struct udcFile *file)
/* Get next character from file or die trying. */
{
UBYTE b;
udcMustRead(file, &b, 1);
return b;
}

bits64 udcReadBits64(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 64 bit entity. */
{
bits64 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap64(val);
return val;
}

bits32 udcReadBits32(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 32 bit entity. */
{
bits32 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap32(val);
return val;
}

bits16 udcReadBits16(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 16 bit entity. */
{
bits16 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap16(val);
return val;
}

float udcReadFloat(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap floating point number. */
{
float val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwapFloat(val);
return val;
}

double udcReadDouble(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap double-precision floating point number. */
{
double val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwapDouble(val);
return val;
}

char *udcReadLine(struct udcFile *file)
/* Fetch next line from udc cache or NULL. */
{
char shortBuf[2], *longBuf = NULL, *buf = shortBuf;
int i, bufSize = sizeof(shortBuf);
for (i=0; ; ++i)
    {
    /* See if need to expand buffer, which is initially on stack, but if it gets big goes into 
     * heap. */
    if (i >= bufSize)
        {
	int newBufSize = bufSize*2;
	char *newBuf = needLargeMem(newBufSize);
	memcpy(newBuf, buf, bufSize);
	freeMem(longBuf);
	buf = longBuf = newBuf;
	bufSize = newBufSize;
	}

    char c;
    bits64 sizeRead = udcRead(file, &c, 1);
    if (sizeRead == 0)
        {
        // EOF before newline: return NULL for empty string
        if (i == 0)
            return NULL;
        break;
        }
    buf[i] = c;
    if (c == '\n')
	{
	buf[i] = 0;
	break;
	}
    }
char *retString = cloneString(buf);
freeMem(longBuf);
return retString;
}

char *udcReadStringAndZero(struct udcFile *file)
/* Read in zero terminated string from file.  Do a freeMem of result when done. */
{
char shortBuf[2], *longBuf = NULL, *buf = shortBuf;
int i, bufSize = sizeof(shortBuf);
for (i=0; ; ++i)
    {
    /* See if need to expand buffer, which is initially on stack, but if it gets big goes into 
     * heap. */
    if (i >= bufSize)
        {
	int newBufSize = bufSize*2;
	char *newBuf = needLargeMem(newBufSize);
	memcpy(newBuf, buf, bufSize);
	freeMem(longBuf);
	buf = longBuf = newBuf;
	bufSize = newBufSize;
	}
    char c = udcGetChar(file);
    buf[i] = c;
    if (c == 0)
        break;
    }
char *retString = cloneString(buf);
freeMem(longBuf);
return retString;
}

char *udcFileReadAllIfExists(char *url, char *cacheDir, size_t maxSize, size_t *retSize)
/* Read a complete file via UDC. Return NULL if the file doesn't exist.  The
 * cacheDir may be null in which case udcDefaultDir() will be used.  If
 * maxSize is non-zero, check size against maxSize and abort if it's bigger.
 * Returns file data (with an extra terminal for the common case where it's
 * treated as a C string).  If retSize is non-NULL then returns size of file
 * in *retSize. Do a freeMem or freez of the returned buffer when done. */
{
struct udcFile  *file = udcFileMayOpen(url, cacheDir);
if (file == NULL)
    return NULL;
size_t size = file->size;
if (maxSize != 0 && size > maxSize)
    errAbort("%s is %lld bytes, but maxSize to udcFileReadAll is %lld",
    	url, (long long)size, (long long)maxSize);
char *buf = needLargeMem(size+1);
udcMustRead(file, buf, size);
buf[size] = 0;	// add trailing zero for string processing
udcFileClose(&file);
if (retSize != NULL)
    *retSize = size;
return buf;
}

char *udcFileReadAll(char *url, char *cacheDir, size_t maxSize, size_t *retSize)
/* Read a complete file via UDC. The cacheDir may be null in which case udcDefaultDir()
 * will be used.  If maxSize is non-zero, check size against maxSize
 * and abort if it's bigger.  Returns file data (with an extra terminal for the
 * common case where it's treated as a C string).  If retSize is non-NULL then
 * returns size of file in *retSize. Do a freeMem or freez of the returned buffer
 * when done. */
{
char *buf = udcFileReadAllIfExists(url, cacheDir, maxSize, retSize);
if (buf == NULL)
    errAbort("Couldn't open %s", url);
return buf;
}

struct lineFile *udcWrapShortLineFile(char *url, char *cacheDir, size_t maxSize)
/* Read in entire short (up to maxSize) url into memory and wrap a line file around it.
 * The cacheDir may be null in which case udcDefaultDir() will be used.  If maxSize
 * is zero then a default value (currently 256 meg) will be used. */
{
if (maxSize == 0) maxSize = 256 * 1024 * 1024;
char *buf = udcFileReadAll(url, cacheDir, maxSize, NULL);
return lineFileOnString(url, TRUE, buf);
}

void udcSeekCur(struct udcFile *file, bits64 offset)
/* Seek to a particular position in file. */
{
file->ios.udc.numSeeks++;
file->offset += offset;
}

void udcSeek(struct udcFile *file, bits64 offset)
/* Seek to a particular position in file. */
{
file->ios.udc.numSeeks++;
file->offset = offset;
}

bits64 udcTell(struct udcFile *file)
/* Return current file position. */
{
return file->offset;
}

time_t udcUpdateTime(struct udcFile *udc)
/* return udc->updateTime */
{
if (sameString("transparent", udc->protocol))
    {
    struct stat status;
    int ret = stat(udc->url, &status);
    if (ret < 0)
	return 0;
    else
	return  status.st_mtime;
    }
return udc->updateTime;
}

off_t udcFileSize(char *url)
/* fetch file size from given URL or local path 
 * returns -1 if not found. */
{
verbose(4, "getting file size for %s", url);
if (udcIsLocal(url))
    return fileSize(url);

// don't go to the network if we can avoid it
off_t cacheSize = udcSizeFromCache(url, NULL);
if (cacheSize!=-1)
    return cacheSize;

off_t ret = -1;

if (startsWith("http://",url) || startsWith("https://",url) || udcIsResolvable(url) )
    {
    errAbort("http protocol not supported");
    }
else if (startsWith("ftp://",url))
    {
    errAbort("ftp protocol not supported");
    }
else
    errAbort("udc/udcFileSize: invalid protocol for url %s, can only do http/https/ftp", url);

return ret;
}

boolean udcIsLocal(char *url) 
/* return true if file is not a http or ftp file, just a local file */
{
// copied from above
char *protocol = NULL, *afterProtocol = NULL, *colon;
udcParseUrl(url, &protocol, &afterProtocol, &colon);
freez(&protocol);
freez(&afterProtocol);
return colon==NULL;
}

boolean udcExists(char *url)
/* return true if a local or remote file exists */
{
return udcFileSize(url)!=-1;
}

