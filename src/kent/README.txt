The files in this folder were taken from v463 of the kent-core tree:

  https://github.com/ucscGenomeBrowser/kent-core/archive/refs/tags/v463.tar.gz

Only the following files were copied from the kent-core tree to the
Rtwobitlib/src/kent/ folder:

  - from kent-core-463/src/inc/: common.h localmem.h portable.h
    portimpl.h errAbort.h hash.h dnaseq.h sig.h bits.h dystring.h
    cheapcgi.h linefile.h obscure.h hex.h dnautil.h dnaseq.h twoBit.h

  - from kent-core-463/src/lib/: common.c localmem.c osunix.c
    errAbort.c hash.c bits.c dystring.c cheapcgi.c linefile.c obscure.c
    hex.c dnautil.c dnaseq.c twoBit.c

Note that the 2bit API is defined in twoBit.h. In order to keep the library
as small as possible, we removed twoBitOpenExternalBptIndex() from the API.
Supporting this function would require to bring the following additional files
from the kent-core tree:
  - udc.c/udc/h
  - bPlusTree.c/bPlusTree.h
It would also require to link the package to libcrypto because the code in
udc.c calls SHA1() from the crypto library in openssl.

Then the following heavy edits were performed:

  (a) in common.h:

      * add the following lines at the top of common.h (right below
        #define COMMON_H):
          #include <R_ext/Error.h>
          #define errAbort Rf_error
          #define warn Rf_warning

      * remove function protoypes for: errAbort, warn, childExecFailedExit

      * remove the following lines:
          #if defined(__APPLE__)
          #if defined(__i686__)
          /* The i686 apple math library defines warn. */
          #define warn jkWarn
          #endif
          #endif

      * remove the following includes:
          #include <time.h>
          #include <sys/stat.h>
          #include <sys/types.h>
          #include <sys/wait.h>

      * replace needMem function prototype with
          INLINE void *needMem(size_t size)
          /* Need mem calls abort if the memory allocation fails. The memory
           * is initialized to zero. */
          {
          void *pt;
          if (size == 0)
              errAbort("needMem: trying to allocate 0 bytes");
          if ((pt = malloc(size)) == NULL)
              errAbort("needMem: Out of memory - request size %llu bytes, errno: %d\n",
                       (unsigned long long)size, errno);
          memset(pt, 0, size);
          return pt;
          }

      * replace needLargeMem function prototype with
          INLINE void *needLargeMem(size_t size)
          /* This calls abort if the memory allocation fails. The memory is
           * not initialized to zero. */
          {
          void *pt;
          if (size == 0)
              errAbort("needLargeMem: trying to allocate 0 bytes");
          if ((pt = malloc(size)) == NULL)
              errAbort("needLargeMem: Out of memory - request size %llu bytes, errno: %d\n",
                       (unsigned long long)size, errno);
          return pt;
          }

      * replace needHugeZeroedMem function prototype with
          INLINE void *needLargeZeroedMem(size_t size)
          /* Request a large block of memory and zero it. */
          {
          void *pt;
          pt = needLargeMem(size);
          memset(pt, 0, size);
          return pt;
          }

      * replace needMoreMem function prototype with
          INLINE void *needMoreMem(void *old, size_t oldSize, size_t newSize)
          /* Adjust memory size on a block, possibly relocating it.  If old
           * is NULL, a new memory block is allocated.  No checking on size.
           * If block is grown, new memory is zeroed. */
          {
          void *pt;
          if (newSize == 0)
              errAbort("needMoreMem: trying to allocate 0 bytes");
          if ((pt = realloc(old, newSize)) == NULL)
             errAbort("needMoreMem: Out of memory - request size %llu bytes, errno: %d\n",
                       (unsigned long long)newSize, errno);
          if (newSize > oldSize)
              memset(((char*)pt)+oldSize, 0, newSize-oldSize);
          return pt;
          }

      * replace freeMem function prototype with
          INLINE void freeMem(void *pt)
          /* Free memory will check for null before freeing. */
          {
          if (pt != NULL)
              free(pt);
          }

      * replace freez function prototype with
          INLINE void freez(void *vpt)
          /* Pass address of pointer.  Will free pointer and set it
           * to NULL. */
          {
          void **ppt = (void **)vpt;
          void *pt = *ppt;
          *ppt = NULL;
          freeMem(pt);
          }

  (b) in common.c/common.h:

      * in common.c:

        - remove includes: "sqlNum.h", "hash.h"

        - replace this line (in carefulCloseWarn function)
            errnoWarn("fclose failed");
          with
            warn("%s\n%s", strerror(errno), "fclose failed");

      * in common.c and common.h:

        - remove functions: wildMatch, loadSizes, sqlMatchLike, truncatef,
          vatruncatef, warnWithBackTrace, chopByWhiteRespectDoubleQuotes,
          chopByCharRespectDoubleQuotes, mktimeFromUtc, dateToSeconds,
          dateIsOld, dateIsOlderBy, dayOfYear, dateAddTo, dateAdd, daysOfMonth,
          dumpStack, vaDumpStack, getTimesInSeconds, uglyTime, uglyt,
          verbose*, makeDir, makeDirs, slNameIntersection, memCheckPoint,
          slSortMergeUniq, slUniqify, slFreeListWithFunc,
          slPairFreeVals*, containsStringNoCase, strstrNoCase,
          needHugeZeroedMem, needHugeZeroedMemResize, needHugeMem,
          doubleBoxWhiskerCalc, slDoubleBoxWhiskerCalc

      * replace 'char *fileName' with 'const char *fileName' in the
        prototype/definition of function mustOpen

      * replace 'char *sep' with 'const char *sep' in the prototype/definition
        of function chopString

      * replace 'char *str' with 'const char *str' in prototype/definition of
        function slPairListFromString

  (c) in localmem.c/localmem.h:

      * remove functions: lmBlockHeaderSize, lmCloneString, lmCloneStringZ,
        lmAllocMoreMem

  (d) in osunix.c, portimpl.h, and portable.h:

      * remove #include <sys/utsname.h>, #include <sys/statvfs.h>,
        #include <pwd.h>, #include "portimpl.h", <sys/wait.h>,
        #include <termios.h>, #include <sys/resource.h>,
        #include "htmshell.h", and #include <sys/types.h>

      * remove functions: rTempName, maybeTouchFile, mysqlHost, listDir*,
        pathsInDirAndSubdirs, semiUniqName, getHost, getUser, dumpStack,
        vaDumpStack, freeSpaceOnFileSystem, execPStack, childExecFailedExit,
        rawKeyIn, getTimesInSeconds, setMemLimit, timevalToSeconds,
        makeSymLink, mustReadSymlink*, mustFork, sleep1000, clock1000, clock1,
        uglyfBreak, setupWss, makeTempName, cgiDir, trashDir,
        machineSpeed, mkdirTrashDirectory, rPathsInDirAndSubdirs,
        pathsInDirAndSubdirs, envUpdate, makeDirsOnPath, makeDir,
        getCurrentDir, getCurrentDir, speed

      * remove variable wss

      * replace 'char *fileName' with 'const char *fileName' in
        prototype/definition of function isRegularFile

  (e) in errAbort.c/errAbort.h:

      * remove includes: <pthread.h>, "hash.h"

      * remove functions: errAbort, warn, warnWithBackTrace,
        isErrAbortInProgress, errAbortDebugnPushPopErr, push*, pop*,
        warnAbortHandler, debugAbort, errnoWarn, vaErrAbort, vaWarn,
        getThreadVars, defaultVaWarn, silentVaWarn, defaultAbort,
        errAbortSetDoContentType

      * remove struct perThreadAbortVars definition

      * add #include <R_ext/Print.h> to errAbort.c right below
        #include "errAbort.h"

      * reimplement errnoAbort function by replacing its 6-line body
          char fbuf[512];
          va_list args;
          va_start(args, format);
          sprintf(fbuf, "%s\n%s", strerror(errno), format);
          vaErrAbort(fbuf, args);
          va_end(args);
        with
          char fbuf[1024];
          va_list args;
          Rprintf("%s\n", strerror(errno));
          vsnprintf(fbuf, sizeof(fbuf), format, args);
          va_end(args);
          Rf_error("%s", fbuf);

      * reimplement noWarnAbort function by replacing its 3-line body
          struct perThreadAbortVars *ptav = getThreadVars();
          ptav->abortArray[ptav->abortIx]();
          exit(-1);
        with
          Rf_error("%s", "unexpected error in Rtwobitlib");

      * fix function prototypes in errAbort.h:
          typedef void (*AbortHandler)(); --> typedef void (*AbortHandler)(void);
          void noWarnAbort(); --> void noWarnAbort(void);

      * remove variable doContentType

  (f) in hash.c/hash.h:

      * add #include "common.h" in hash.h (right below #define HASH_H)

      * remove function hashElCmpIntValDesc

      * replace 'char *' with 'const char *' in prototypes/definitions of
        functions: hashString, hashCrc, hashLookupUpperCase, hashAdd,
        hashAddN, hashRemove, hashAddUnique, hashAddSaveName, hashStore,
        hashStoreName, hashMustFindName, hashMustFindVal, hashFindVal,
        hashOptionalVal, hashFindValUpperCase, hashAddInt, hashIncInt,
        hashIntVal, hashIntValDefault, hashElFindVal, hashHisto,
        hashPrintStats, hashReplace, hashMayRemove, hashMustRemove,
        hashFromString, hashFreeWithVals

      * add 'const' to declaration of variable keyStr in function hashString

  (g) bits.c/bits.h:

      * remove functions: lmBitRealloc, bitCountRange, bitRealloc, bitClone,
        bitsIn, bitAlloc

      * fix function prototypes in bits.h:
          void bitsInByteInit(); --> void bitsInByteInit(void);

  (h) in dystring.c/dystring.h:

      * remove functions: dyStringPrintf, dyStringCreate, dyStringVaPrintf,
        dyStringBumpBufSize

      * remove function checkNOSQLINJ and any call to it

  (i) in obscure.c/obscure.h:

      * remove includes: "sqlNum.h", <sys/syscall.h>, <unistd.h>, "hash.h"

      * remove functions: hashTwoColumnFile, currentVmPeak, readAllWords,
        get_thread_id, readAndIgnore, printVmPeak, nameInCommaList,
        ensureNamesCaseUnique, spaceToUnderbar, endsWithWordComma,
        slNameListOfUniqueWords, slPairTwoColumnFile, slListRandom*,
        stringToSlNames, writeTsvRow, shuffle*, *printWith*, rangeRoundUp,
        rangeFromMinMaxMeanStd, hashVarLine, hashThisEqThatLine,
        stripCommas, *Quoted*, *printLongWithCommas, hashNameIntFile,
        hashWordsInFile, hashTsvBy, stripCommas

      * global variable _dotForUserMod

  (j) in cheapcgi.c/cheapcgi.h: we only need the cgiDecode() function
      from these files so we remove everything except that:

      * remove functions: useTempFile, cgiRemoteAddr, cgiUserAgent,
        cgiParseMultipart, cgiVar*, cgiBoolean, cgiBooleanDefined,
        cgiChangeVar, cgiStringList, cgiFromFile, cgiScriptName,
        cgiScriptDirUrl, _cgiFindInput, getPostInput, cgiSpoof, initCgiInput,
        cgiIntExp, cgiOneChoice, cgiContinue*, cgiString, cgiInt,
        cgiDouble, initSigHandlers, catchSignal, logCgiToStderr,
        cgiOptionalDouble, cgiOptionalInt, cgiUsualString, cgiUsualDouble,
        cgiOptionalString, mustFindVarData, findVarData, cgiFromCommandLine,
        cgiMake*, cgiDictionary*, cgiSimpleTable*, cgiTable*,
        cgiParagraph, cgiBadVar, js*, findJsEvent, commonCssStyles,
        checkValidEvent, cgi*ShadowPrefix, cgiUrlString, cgiAppendSForHttps,
        cgiServer*, cgiClientBrowser, cgiBrowser, cgiDown,
        cgiStringNewValForVar, turnCgiVarsToVals,
        cgiParse*, cgiDropDownWithTextValsAndExtra, cgiEncode*, cgiDecodeFull,
        parseCookies, findCookieData, dumpCookieList, javaScriptLiteralEncode,
        cgiInputSource, getPostInput, getQueryInput*, cgiResetState,
        cgiIsOnWeb, cgiRequest*, _cgiFindInput, _cgiRawInput

      * remove variable: doUseTempFile, dumpStackOnSignal, inputSize,
        inputString, inputHash, inputList, haveCookiesHash, cookieHash,
        cookieList

      * replace 'char *in' with 'const char *in' in prototype/definition of
        cgiDecode function (do NOT do this for 'char *out')

      * change type of variable 'code' from 'int' to 'unsigned int'

  (k) in linefile.c/linefile.h:

      * remove includes: "pipeline.h", "hash.h"

      * remove any reference to the udc stuff and htslib/tabix stuff

      * reimplement noTabixSupport function by replacing its 2-line body
          if (lf->tabix != NULL)
              lineFileAbort(lf, "%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);
        with
          if (lf->tabix != NULL)
              Rf_error("%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);

      * remove functions: getDecompressor, lineFileDecompressMem,
        lineFileDecompressFd, lineFileDecompress, lineFileAbort,
        lineFileVaAbort, getFileNameFromHdrSig,
        lineFileRemoveInitialCustomTrackLines

      * remove 'if (getDecompressor(fileName) != NULL)' statement in
        lineFileAttach function

      * remove 'pl' member from struct lineFile definition (in linefile.h)

      * remove 'if (lf->pl != NULL)' statement in lineFileSeek function
        and 'if (pl != NULL)' statement in lineFileClose function

      * replace 'char *fileName' with 'const char *fileName' in
        prototypes/definitions of functions lineFileMayOpen, lineFileOpen,
        lineFileAttach

  (l) in dnautil.c/dnautil.h:

      * add #include "common.h" in dnautil.h (right below #define DNAUTIL_H)

      * replace all occurrences of 'uint' with 'unsigned int'

      * remove functions: headPolyTSizeLoose, tailPolyASizeLoose,
        maskHeadPolyT, maskTailPolyA, findHeadPolyTMaybeMask,
        findTailPolyAMaybeMask

      * replace 'DNA *in' with 'const DNA *in' in prototypes/definitions of
        functions packDna16, packDna8, packDna4

      * fix function prototypes in dnautil.h:
          void dnaUtilOpen(); --> void dnaUtilOpen(void);
      * fix function prototypes in dnautil.c:
          static void initNtVal() --> static void initNtVal(void)
          static void initNtChars() --> static void initNtChars(void)
          static void initNtMixedCaseChars() --> static void initNtMixedCaseChars(void)
          static void initNtCompTable() --> static void initNtCompTable(void)
          static void checkSizeTypes() --> static void checkSizeTypes(void)
          static void initAaVal() --> static void initAaVal(void)

  (m) in dnaseq.c/dnaseq.h:

      * remove functions: maskFromUpperCaseSeq, dnaSeqHash, cloneDnaSeq

  (n) in twoBit.c/twoBit.h:

      * add #include "common.h" in twoBit.h (right below #define TWOBIT_H)

      * remove includes: "net.h", "localmem.h", "dnautil.h"

      * comment out:
        - twoBitOpenExternalBptIndex function
        - include "bPlusTree.h"
        - 'bpt' member (Alternative index) from struct twoBitFile definition
        - call to bptFileClose in twoBitClose function
        - 'if (tbf->bpt)' statements and if part of 'if (tbf->bpt) else'
          statements

      * remove all
          if (hasProtocol(fileName))
              useUdc = TRUE;
        statements, as well as any reference to the udc stuff

      * replace 'char *' with 'const char *' in prototypes/definitions of
        functions twoBitOpen, twoBitSeqNames, twoBitFromFile,
        twoBitIsFile, twoBitIsRange, twoBitIsFileOrRange, twoBitSpecNew,
        twoBitSpecNewFile, twoBitChromHash, twoBitOpenReadHeader, getTbfAndOpen,
        parseSeqSpec, countBlocksOfN, countBlocksOfLower, storeBlocksOfN,
        storeBlocksOfLower

      * add 'const' to declaration of variable DNA in function twoBitFromDnaSeq

      * modify function twoBitWriteHeaderExt as follow:
        - change return type from 'void' to 'int'
        - add 'const char **msg' argument
        - add the 2 following lines at the very beginning of the function
          body:
            static char msg_buf[280];
            *msg = msg_buf;
        - replace this line
            errAbort("name %s too long", twoBit->name);
          with
            {
            snprintf(msg_buf, sizeof(msg_buf),
                     "sequence name too long: %s", twoBit->name);
            return -1;
            }
        - replace these lines
            errAbort("Error in faToTwoBit, blah blah ..."
                    "does not support blah blah ...\n"
                    "please split up blah blah ...",
                    twoBit->name, UINT_MAX/1000000000);
          with
            {
            snprintf(msg_buf, sizeof(msg_buf),
                     "index overflow at sequence %s", twoBit->name);
            return -2;
            }

      * modify function twoBitWriteHeader as follow:
        - change return type from 'void' to 'int'
        - add 'const char **msg' argument
        - replace line
            twoBitWriteHeaderExt(twoBitList, f, FALSE);
          with
            return twoBitWriteHeaderExt(twoBitList, f, FALSE, msg);


-------------------------------------------------------------------------------

This section documents what it would take to bring back the
twoBitOpenExternalBptIndex functionality

  - Copy the bPlusTree.c/bPlusTree.h files from the kent-core tree

  - Copy the udc.c/udc.h files from the kent-core tree and modify them as
    follow:

      * remove includes: <sys/wait.h>, <sys/mman.h>, "hash.h"

      * remove net.h and htmlPage.h includes

      * add #include "obscure.h" (between linefile.h and portable.h includes)

      * disable protocols "slow", "http", and "ftp" in udcProtocolNew by
        removing the corresponding 'else if' blocks (only "local"
        and "transparent" will be left)

      * remove any 'if (udcCacheEnabled())' statement

      * in udcRead function remove
          if (!udcCachePreload(file, start, size))
              {
              verbose(4, "udcCachePreload failed");
              bytesRead = 0;
              break;
              }

      * in udcFileClose function remove
          if (file->mmapBase != NULL)
              {
              if (munmap(file->mmapBase, file->size) < 0)
                  errnoAbort("munmap() failed on %s", file->url);
              }

      * remove 'mmapBase' member from struct udcFile definition

      * in longDirHash function replace
          int maxLen = pathconf(cacheDir, _PC_NAME_MAX);
          if (maxLen < 0)
              maxLen = 255;
        with
          int maxLen = 255;

      * remove 'if (udcIsResolvable(..) && ..)' statement in udcFileMayOpen
        function

      * remove functions: connInfoGetSocket, udcDataViaHttpOrFtp, udcInfoViaFtp,
        udcSetResolver, resolveUrl, udcInfoViaHttp, udcLoadCachedResolvedUrl,
        rCleanup, udcCleanup, bitRealDataSize, resolveUrlExec, makeUdcTmp,
        udcReadAndIgnore, ourRead, udc*ViaSlow, udcCache*, udcSetCacheTimeout,
        udcFetchMissing, udcTestAndSetRedirect, setInitialCachedDataBounds,
        rangeIntersectOrTouch64, fetchMissingBits, udcNewCreateBitmapAndSparse,
        fetchMissingBlocks, allBitsSetInFile, udcCheckCacheBits,
        readBitsIntoBuf, ourMustWrite, udcMMap*, udcExists, udcFileSize,
        udcIsLocal, udcUpdateTime, udcSetLog, udcFastReadString,
        msbFirstWriteBits64, udc*FromCache, udcIsResolvable, udcDisableCache,
        udcFileCacheFiles, udcSizeAndModTimeFromBitmap, udcBitmapOpen

      * remove global variables: cacheTimeout, resolvCmd

  - See NOTES.txt in Rtwobitlib's top-level folder for additional things to do.

