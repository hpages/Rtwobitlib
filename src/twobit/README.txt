The files in this folder were taken from:

  https://github.com/ucscGenomeBrowser/kent-core/archive/refs/tags/v463.tar.gz

Only the following subset was copied to the Rtwobitlib/src/twobit/ folder:

  - from kent-core-463/src/inc/: common.h verbose.h portable.h portimpl.h
    dlist.h memalloc.h errAbort.h hash.h dnaseq.h sig.h localmem.h bits.h
    dystring.h pipeline.h cheapcgi.h linefile.h obscure.h bPlusTree.h hex.h
    udc.h dnautil.h twoBit.h

  - from kent-core-463/src/lib/: common.c verbose.c portimpl.c osunix.c
    dlist.c memalloc.c errAbort.c hash.c localmem.c bits.c
    dystring.c pipeline.c cheapcgi.c linefile.c obscure.c bPlusTree.c hex.c
    udc.c dnautil.c twoBit.c

Then the following heavy edits were performed:

  (a) in common.c/common.h:

      * in common.h:
        - remove #include <time.h>
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
          verboseTime*

      * replace 'char *fileName' with 'const char *fileName' in the
        prototype/definition of function mustOpen

      * replace 'char *sep' with 'const char *sep' in the prototype/definition
        of function chopString

  (b) in verbose.c/verbose.h:

      * remove functions verboseTime*

      * remove global variable lastTime

  (b) in osunix.c, portable.h, and portimpl.h:

      * remove #include <sys/utsname.h>, #include <sys/statvfs.h>,
        #include <pwd.h>, <sys/wait.h>, #include <termios.h>, and
        #include <sys/resource.h>

      * remove functions: rTempName, maybeTouchFile, mysqlHost, listDir*,
        pathsInDirAndSubdirs, semiUniqName, getHost, getUser, dumpStack,
        vaDumpStack, freeSpaceOnFileSystem, execPStack, childExecFailedExit,
        rawKeyIn, getTimesInSeconds, setMemLimit, timevalToSeconds,
        makeSymLink, mustReadSymlink*, mustFork, sleep1000, clock1000, clock1,
        uglyfBreak

      * replace 'char *fileName' with 'const char *fileName' in
        prototype/definition of function isRegularFile

  (c) in portimpl.c:

      * remove #include "htmshell.h"

      * remove functions: setupWss, makeTempName, cgiDir, trashDir,
        machineSpeed, mkdirTrashDirectory, rPathsInDirAndSubdirs,
        pathsInDirAndSubdirs, envUpdate

      * remove variables: wss

  (d) in memalloc.c/memalloc.h:

      * remove functions: carefulTotalAllocated, pushCarefulMemHandler,
        carefulCheckHeap, carefulCountBlocksAllocated, carefulRealloc,
        carefulFree, carefulAlloc, carefulMemInit,

      * remove #include <pthread.h>

      * remove global variables: carefulMemHandler, carefulAlloced,
        carefulParent, carefulMutex, carefulMaxToAlloc, carefulAlignMask,
        carefulAlignAdd, carefulAlignSize

  (e) in errAbort.c/errAbort.h:

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

  (f) in dystring.c/dystring.h:

      * remove function dyStringPrintf

      * remove function checkNOSQLINJ and any call to it

  (g) in pipeline.c/pipeline.h:

      * remove #include "sqlNum.h"

      * remove function pipelineOpenMem1

      * replace 'char *otherEndFile' with 'const char *otherEndFile' in
        prototypes/definitions of functions pipelineOpen and pipelineOpen1

      * replace 'char *fname' with 'const char *fname' in
        prototypes/definitions of functions openRead and openWrite

  (h) in localmem.h:

      * remove functions: lmCloneString, lmCloneStringZ

  (i) in obscure.c/obscure.h:

      * remove #include "sqlNum.h"

      * remove functions: hashTwoColumnFile, currentVmPeak, readAllWords

  (j) in udc.c/udc.h:

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

      * remove functions: connInfoGetSocket, udcDataViaHttpOrFtp, udcInfoViaFtp,
        udcSetResolver, resolveUrl, udcInfoViaHttp, udcLoadCachedResolvedUrl,
        rCleanup, udcCleanup, bitRealDataSize, resolveUrlExec, makeUdcTmp,
        udcReadAndIgnore, ourRead, udc*ViaSlow, udcCache*, udcSetCacheTimeout,
        udcFetchMissing, udcTestAndSetRedirect, setInitialCachedDataBounds,
        rangeIntersectOrTouch64, fetchMissingBits, udcNewCreateBitmapAndSparse,
        fetchMissingBlocks, allBitsSetInFile, udcCheckCacheBits

      * remove this line in udcFileSize function:
          struct udcRemoteFileInfo info;

      * remove global variable cacheTimeout

  (k) in cheapcgi.c/cheapcgi.h: we only need the cgiDecode() function
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

  (l) in linefile.c/linefile.h:

      * remove any reference to the udc stuff and htslib/tabix stuff

      * reimplement noTabixSupport function by replacing its 2-line body
          if (lf->tabix != NULL)
              lineFileAbort(lf, "%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);
        with
          if (lf->tabix != NULL)
              Rf_error("%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);

      * remove functions: lineFileDecompressMem, lineFileAbort, lineFileVaAbort

      * replace 'char *fileName' with 'const char *fileName' in
        prototypes/definitions of functions lineFileMayOpen, lineFileOpen,
        lineFileAttach, getDecompressor, headerBytes, lineFileDecompress

  (m) in twoBit.c/twoBit.h:

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

