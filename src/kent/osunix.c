/* Some wrappers around operating-system specific stuff. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <dirent.h>
#include <sys/time.h>
#include "portable.h"
#include "portimpl.h"
#include <regex.h>
#include <utime.h>


off_t fileSize(char *pathname)
/* get file size for pathname. return -1 if not found */
{
struct stat mystat;
ZeroVar(&mystat);
if (stat(pathname,&mystat)==-1)
    {
    return -1;
    }
return mystat.st_size;
}

char *getCurrentDir()
/* Return current directory.  Abort if it fails. */
{
static char dir[PATH_LEN];

if (getcwd( dir, sizeof(dir) ) == NULL )
    errnoAbort("getCurrentDir: can't get current directory");
return dir;
}

void setCurrentDir(char *newDir)
/* Set current directory.  Abort if it fails. */
{
if (chdir(newDir) != 0)
    errnoAbort("setCurrentDir: can't to set current directory: %s", newDir);
}

boolean maybeSetCurrentDir(char *newDir)
/* Change directory, return FALSE (and set errno) if fail. */
{
return chdir(newDir) == 0;
}

struct fileInfo *newFileInfo(char *name, off_t size, bool isDir, int statErrno, 
	time_t lastAccess, time_t creationTime)
/* Return a new fileInfo. */
{
int len = strlen(name);
struct fileInfo *fi = needMem(sizeof(*fi) + len);
fi->size = size;
fi->isDir = isDir;
fi->statErrno = statErrno;
fi->lastAccess = lastAccess;
fi->creationTime = creationTime;
strcpy(fi->name, name);
return fi;
}

int cmpFileInfo(const void *va, const void *vb)
/* Compare two fileInfo. */
{
const struct fileInfo *a = *((struct fileInfo **)va);
const struct fileInfo *b = *((struct fileInfo **)vb);
return strcmp(a->name, b->name);
}

time_t fileModTime(char *pathName)
/* Return file last modification time.  The units of
 * these may vary from OS to OS, but you can depend on
 * later files having a larger time. */
{
struct stat st;
if (stat(pathName, &st) < 0)
    errnoAbort("stat failed in fileModTime: %s", pathName);
return st.st_mtime;
}


char *getTempDir(void)
/* get temporary directory to use for programs.  This first checks TMPDIR environment
 * variable, then /data/tmp, /scratch/tmp, /var/tmp, /tmp.  Return is static and
 * only set of first call */
{
static char *checkTmpDirs[] = {"/data/tmp", "/scratch/tmp", "/var/tmp", "/tmp", NULL};

static char* tmpDir = NULL;
if (tmpDir == NULL)
    {
    tmpDir = getenv("TMPDIR");
    if (tmpDir != NULL)
        tmpDir = cloneString(tmpDir);  // make sure it's stable
    else
        {
        int i;
        for (i = 0; (checkTmpDirs[i] != NULL) && (tmpDir == NULL); i++)
            {
            if (fileSize(checkTmpDirs[i]) >= 0)
                tmpDir = checkTmpDirs[i];
            }
        }
    }
if (tmpDir == NULL)
    errAbort("BUG: can't find a tmp directory");
return tmpDir;
}

void mustRename(char *oldName, char *newName)
/* Rename file or die trying. */
{
int err = rename(oldName, newName);
if (err < 0)
    errnoAbort("Couldn't rename %s to %s", oldName, newName);
}

void mustRemove(char *path)
/* Remove file or die trying */
{
int err = remove(path);
if (err < 0)
    errnoAbort("Couldn't remove %s", path);
}

static void eatSlashSlashInPath(char *path)
/* Convert multiple // to single // */
{
char *s, *d;
s = d = path;
char c, lastC = 0;
while ((c = *s++) != 0)
    {
    if (c == '/' && lastC == c)
        continue;
    *d++ = c;
    lastC = c;
    }
*d = 0;
}

static void eatExcessDotDotInPath(char *path)
/* If there's a /.. in path take it out.  Turns 
 *      'this/long/../dir/file' to 'this/dir/file
 * and
 *      'this/../file' to 'file'  
 *
 * and
 *      'this/long/..' to 'this'
 * and
 *      'this/..' to  ''   
 * and
 *       /this/..' to '/' */
{
/* Take out each /../ individually */
for (;;)
    {
    /* Find first bit that needs to be taken out. */
    char *excess= strstr(path, "/../");
    char *excessEnd = excess+4;
    if (excess == NULL || excess == path)
        break;

    /* Look for a '/' before this */
    char *excessStart = matchingCharBeforeInLimits(path, excess, '/');
    if (excessStart == NULL) /* Preceding '/' not found */
         excessStart = path;
    else 
         excessStart += 1;
    strcpy(excessStart, excessEnd);
    }

/* Take out final /.. if any */
if (endsWith(path, "/.."))
    {
    if (!sameString(path, "/.."))  /* We don't want to turn this to blank. */
	{
	int len = strlen(path);
	char *excessStart = matchingCharBeforeInLimits(path, path+len-3, '/');
	if (excessStart == NULL) /* Preceding '/' not found */
	     excessStart = path;
	else 
	     excessStart += 1;
	*excessStart = 0;
	}
    }
}

char *simplifyPathToDir(char *path)
/* Return path with ~ and .. taken out.  Also any // or trailing /.   
 * freeMem result when done. */
{
/* Expand ~ if any with result in newPath */
char newPath[PATH_LEN];
int newLen = 0;
char *s = path;
if (*s == '~')
    {
    char *homeDir = getenv("HOME");
    if (homeDir == NULL)
        errAbort("No HOME environment var defined after ~ in simplifyPathToDir");
    ++s;
    if (*s == '/')  /*    ~/something      */
        {
	++s;
	safef(newPath, sizeof(newPath), "%s/", homeDir);
	}
    else            /*   ~something        */
	{
	safef(newPath, sizeof(newPath), "%s/../", homeDir);
	}
    newLen = strlen(newPath);
    }
int remainingLen  = strlen(s);
if (newLen + remainingLen >= sizeof(newPath))
    errAbort("path too big in simplifyPathToDir");
strcpy(newPath+newLen, s);

/* Remove //, .. and trailing / */
eatSlashSlashInPath(newPath);
eatExcessDotDotInPath(newPath);
int lastPos = strlen(newPath)-1;
if (lastPos > 0 && newPath[lastPos] == '/')
    newPath[lastPos] = 0;

return cloneString(newPath);
}

#ifdef DEBUG
void simplifyPathToDirSelfTest()
{
/* First test some cases which should remain the same. */
assert(sameString(simplifyPathToDir(""),""));
assert(sameString(simplifyPathToDir("a"),"a"));
assert(sameString(simplifyPathToDir("a/b"),"a/b"));
assert(sameString(simplifyPathToDir("/"),"/"));
assert(sameString(simplifyPathToDir("/.."),"/.."));
assert(sameString(simplifyPathToDir("/../a"),"/../a"));

/* Now test removing trailing slash. */
assert(sameString(simplifyPathToDir("a/"),"a"));
assert(sameString(simplifyPathToDir("a/b/"),"a/b"));

/* Test .. removal. */
assert(sameString(simplifyPathToDir("a/.."),""));
assert(sameString(simplifyPathToDir("a/../"),""));
assert(sameString(simplifyPathToDir("a/../b"),"b"));
assert(sameString(simplifyPathToDir("/a/.."),"/"));
assert(sameString(simplifyPathToDir("/a/../"),"/"));
assert(sameString(simplifyPathToDir("/a/../b"),"/b"));
assert(sameString(simplifyPathToDir("a/b/.."),"a"));
assert(sameString(simplifyPathToDir("a/b/../"),"a"));
assert(sameString(simplifyPathToDir("a/b/../c"),"a/c"));
assert(sameString(simplifyPathToDir("a/../b/../c"),"c"));
assert(sameString(simplifyPathToDir("a/../b/../c/.."),""));
assert(sameString(simplifyPathToDir("/a/../b/../c/.."),"/"));

/* Test // removal */
assert(sameString(simplifyPathToDir("//"),"/"));
assert(sameString(simplifyPathToDir("//../"),"/.."));
assert(sameString(simplifyPathToDir("a//b///c"),"a/b/c"));
assert(sameString(simplifyPathToDir("a/b///"),"a/b"));
}
#endif /* DEBUG */

boolean isPipe(int fd)
/* determine in an open file is a pipe  */
{
struct stat buf;
if (fstat(fd, &buf) < 0)
    errnoAbort("isPipe: fstat failed");
return S_ISFIFO(buf.st_mode);
}

void touchFileFromFile(const char *oldFile, const char *newFile)
/* Set access and mod time of newFile from oldFile. */
{
    struct stat buf;
    if (stat(oldFile, &buf) != 0)
        errnoAbort("stat failed on %s", oldFile);
    struct utimbuf puttime;
    puttime.modtime = buf.st_mtime;
    puttime.actime = buf.st_atime;
    if (utime(newFile, &puttime) != 0)
	errnoAbort("utime failed on %s", newFile);
}

boolean isDirectory(char *pathName)
/* Return TRUE if pathName is a directory. */
{
struct stat st;

if (stat(pathName, &st) < 0)
    return FALSE;
if (S_ISDIR(st.st_mode))
    return TRUE;
return FALSE;
}

boolean isRegularFile(const char *fileName)
/* Return TRUE if fileName is a regular file. */
{
struct stat st;

if (stat(fileName, &st) < 0)
    return FALSE;
if (S_ISREG(st.st_mode))
    return TRUE;
return FALSE;
}

void mustBeReadableAndRegularFile(char *fileName)
/* errAbort if fileName is a regular file and readable. */
{
// check if file exists and is readable, including the
// magic "stdin" name.
FILE *fh = mustOpen(fileName, "r");
struct stat st;
if (fstat(fileno(fh), &st) < 0)
    errnoAbort("stat failed on: %s", fileName);  // should never happen
carefulClose(&fh);

if (!S_ISREG(st.st_mode))
    errAbort("input file (%s) must be a regular file.  Pipes or other special devices can't be used here.", fileName);
}

