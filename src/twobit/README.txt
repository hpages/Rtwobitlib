The files in this folder were taken from:

  https://github.com/ucscGenomeBrowser/kent-core/archive/refs/tags/v463.tar.gz

Only the following subset was copied to the Rtwobitlib/src/twobit/ folder:

  - from kent-core-463/src/inc/: common.h verbose.h portable.h portimpl.h
    dlist.h memalloc.h errAbort.h hash.h dnaseq.h sig.h localmem.h bits.h
    dystring.h cheapcgi.h linefile.h obscure.h bPlusTree.h hex.h
    udc.h dnautil.h twoBit.h

  - from kent-core-463/src/lib/: common.c verbose.c osunix.c
    dlist.c memalloc.c errAbort.c hash.c localmem.c bits.c
    dystring.c cheapcgi.c linefile.c obscure.c bPlusTree.c hex.c
    udc.c dnautil.c twoBit.c

Then the following heavy edits were performed:

  (a) in common.c/common.h:

      * in common.h:
        - remove #include <time.h>, #include <sys/stat.h>,
          #include <sys/types.h>, and #include <sys/wait.h>
        - add the following lines at the top of common.h (right below
          #define COMMON_H):
            #include <R_ext/Error.h>
            #define errAbort Rf_error
            #define warn Rf_warning
        - remove function protoypes for errAbort, warn, childExecFailedExit
        - remove the following lines:
            #if defined(__APPLE__)
            #if defined(__i686__)
            /* The i686 apple math library defines warn. */
            #define warn jkWarn
            #endif
            #endif

      * in common.c:
        - remove #include "sqlNum.h"
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
          verboseTime*, makeDir, makeDirs

      * replace 'char *fileName' with 'const char *fileName' in the
        prototype/definition of function mustOpen

      * replace 'char *sep' with 'const char *sep' in the prototype/definition
        of function chopString

  (b) in verbose.c/verbose.h:

      * remove functions: verboseTime*, verboseCgi

      * remove global variable lastTime

      * remove global variable doHtml and all 'if (doHtml)' statements

  (b) in osunix.c, portimpl.h, and portable.h:

      * remove #include <sys/utsname.h>, #include <sys/statvfs.h>,
        #include <pwd.h>, <sys/wait.h>, #include <termios.h>,
        #include <sys/resource.h>, #include "htmshell.h", and
        #include <sys/types.h>

      * remove functions: rTempName, maybeTouchFile, mysqlHost, listDir*,
        pathsInDirAndSubdirs, semiUniqName, getHost, getUser, dumpStack,
        vaDumpStack, freeSpaceOnFileSystem, execPStack, childExecFailedExit,
        rawKeyIn, getTimesInSeconds, setMemLimit, timevalToSeconds,
        makeSymLink, mustReadSymlink*, mustFork, sleep1000, clock1000, clock1,
        uglyfBreak, setupWss, makeTempName, cgiDir, trashDir,
        machineSpeed, mkdirTrashDirectory, rPathsInDirAndSubdirs,
        pathsInDirAndSubdirs, envUpdate, makeDirsOnPath, makeDir

      * remove variable wss

      * replace 'char *fileName' with 'const char *fileName' in
        prototype/definition of function isRegularFile

  (c) in memalloc.c/memalloc.h:

      * remove functions: carefulTotalAllocated, pushCarefulMemHandler,
        carefulCheckHeap, carefulCountBlocksAllocated, carefulRealloc,
        carefulFree, carefulAlloc, carefulMemInit,

      * remove #include <pthread.h>

      * remove global variables: carefulMemHandler, carefulAlloced,
        carefulParent, carefulMutex, carefulMaxToAlloc, carefulAlignMask,
        carefulAlignAdd, carefulAlignSize

  (d) in errAbort.c/errAbort.h:

      * remove functions: errAbort, warn, warnWithBackTrace,
        isErrAbortInProgress, errAbortDebugnPushPopErr, push*, pop*,
        warnAbortHandler, debugAbort, errnoWarn, vaErrAbort, vaWarn,
        getThreadVars, defaultVaWarn, silentVaWarn, defaultAbort,
        errAbortSetDoContentType

      * remove #include <pthread.h>

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

      * remove variable doContentType

  (e) in dystring.c/dystring.h:

      * remove function dyStringPrintf

      * remove function checkNOSQLINJ and any call to it

  (f) in localmem.h:

      * remove functions: lmCloneString, lmCloneStringZ

  (g) in obscure.c/obscure.h:

      * remove includes "sqlNum.h", <sys/syscall.h>, and <unistd.h>

      * remove functions: hashTwoColumnFile, currentVmPeak, readAllWords,
        get_thread_id, readAndIgnore, printVmPeak, nameInCommaList,
        ensureNamesCaseUnique, spaceToUnderbar, endsWithWordComma,
        slListRandom*, shuffle*, *printWith*, rangeRoundUp,
        rangeFromMinMaxMeanStd

      * global variable _dotForUserMod

  (h) in udc.c/udc.h:

      * remove #include <sys/wait.h> and #include <sys/mman.h>

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

      * remove functions: connInfoGetSocket, udcDataViaHttpOrFtp, udcInfoViaFtp,
        udcSetResolver, resolveUrl, udcInfoViaHttp, udcLoadCachedResolvedUrl,
        rCleanup, udcCleanup, bitRealDataSize, resolveUrlExec, makeUdcTmp,
        udcReadAndIgnore, ourRead, udc*ViaSlow, udcCache*, udcSetCacheTimeout,
        udcFetchMissing, udcTestAndSetRedirect, setInitialCachedDataBounds,
        rangeIntersectOrTouch64, fetchMissingBits, udcNewCreateBitmapAndSparse,
        fetchMissingBlocks, allBitsSetInFile, udcCheckCacheBits,
        readBitsIntoBuf, ourMustWrite, udcMMap*

      * remove this line in udcFileSize function:
          struct udcRemoteFileInfo info;

      * remove global variable cacheTimeout

  (i) in cheapcgi.c/cheapcgi.h: we only need the cgiDecode() function
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
        cgiIsOnWeb, cgiRequest*, _cgiFindInput, _cgiRawInput, 

      * remove variable: doUseTempFile, dumpStackOnSignal, inputSize,
        inputString, inputHash, inputList, haveCookiesHash, cookieHash,
        cookieList

      * replace 'char *in' with 'const char *in' in prototype/definition of
        cgiDecode function (do NOT do this for 'char *out')

  (j) in linefile.c/linefile.h:

      * remove #include "pipeline.h" in linefile.c

      * remove any reference to the udc stuff and htslib/tabix stuff

      * reimplement noTabixSupport function by replacing its 2-line body
          if (lf->tabix != NULL)
              lineFileAbort(lf, "%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);
        with
          if (lf->tabix != NULL)
              Rf_error("%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);

      * remove functions: getDecompressor, lineFileDecompressMem,
        lineFileDecompressFd, lineFileDecompress, lineFileAbort,
        lineFileVaAbort, getFileNameFromHdrSig

      * remove 'if (getDecompressor(fileName) != NULL)' statement in
        lineFileAttach function

      * remove 'pl' member from struct lineFile definition (in linefile.h)

      * remove 'if (lf->pl != NULL)' statement in lineFileSeek function
        and 'if (pl != NULL)' statement in lineFileClose function

      * replace 'char *fileName' with 'const char *fileName' in
        prototypes/definitions of functions lineFileMayOpen, lineFileOpen,
        lineFileAttach

  (k) in twoBit.c/twoBit.h:

      * add #include "common.h" in twoBit.h (right below #define TWOBIT_H)

      * remove #include "net.h" from twoBit.c

      * remove all
          if (hasProtocol(fileName))
              useUdc = TRUE;
        statements, as well as any reference to the udc stuff

      * replace 'char' with 'const char' in prototypes/definitions of
        functions twoBitOpen, twoBitSeqNames, twoBitFromFile,
        twoBitIsFile, twoBitIsRange, twoBitIsFileOrRange, twoBitSpecNew,
        twoBitSpecNewFile, twoBitChromHash, twoBitOpenReadHeader, getTbfAndOpen,
        parseSeqSpec

