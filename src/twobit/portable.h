/* portable.h - wrappers around things that vary from server
 * to server and operating system to operating system. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef PORTABLE_H
#define PORTABLE_H

#include <sys/types.h>
#include <sys/stat.h>

struct fileInfo 
/* Info about a file. */
    {
    struct fileInfo  *next;	/* Next in list. */
    off_t size;		/* Size in bytes. */
    bool isDir;		/* True if file is a directory. */
    int statErrno;	/* Result of stat (e.g. bad symlink). */
    time_t lastAccess;  /* Last access time. */
    time_t creationTime; /* Creation time. */
    char name[1];	/* Allocated at run time. */
    };

struct fileInfo *newFileInfo(char *name, off_t size, bool isDir, int statErrno, 
	time_t lastAccess, time_t creationTime);
/* Return a new fileInfo. */

char *getCurrentDir();
/* Return current directory.  Abort if it fails. */

void setCurrentDir(char *newDir);
/* Set current directory.  Abort if it fails. */

boolean maybeSetCurrentDir(char *newDir);
/* Change directory, return FALSE (and set errno) if fail. */

boolean makeDir(char *dirName);
/* Make dir.  Returns TRUE on success.  Returns FALSE
 * if failed because directory exists.  Prints error
 * message and aborts on other error. */

void makeDirsOnPath(char *pathName);
/* Create directory specified by pathName.  If pathName contains
 * slashes, create directory at each level of path if it doesn't
 * already exist.  Abort with error message if there's a problem.
 * (It's not considered a problem for the directory to already
 * exist. ) */

char *simplifyPathToDir(char *path);
/* Return path with ~ (for home) and .. taken out.   freeMem result when done. */

char *getTempDir(void);
/* get temporary directory to use for programs.  This first checks TMPDIR environment
 * variable, then /data/tmp, /scratch/tmp, /var/tmp, /tmp.  Return is static and
 * only set of first call */

struct tempName
	{
	char forCgi[4096];
	char forHtml[4096];
	};

void mustRename(char *oldName, char *newName);
/* Rename file or die trying. */

void mustRemove(char *path);
/* Remove file or die trying */

time_t fileModTime(char *pathName);
/* Return file last modification time.  The units of
 * these may vary from OS to OS, but you can depend on
 * later files having a larger time. */


boolean isPipe(int fd);
/* determine in an open file is a pipe  */

void touchFileFromFile(const char *oldFile, const char *newFile);
/* Set access and mod time of newFile from oldFile. */

boolean isDirectory(char *pathName);
/* Return TRUE if pathName is a directory. */

void mustBeReadableAndRegularFile(char *fileName);
/* errAbort if fileName is a regular file and readable. */

boolean isRegularFile(const char *fileName);
/* Return TRUE if fileName is a regular file. */

#endif /* PORTABLE_H */

