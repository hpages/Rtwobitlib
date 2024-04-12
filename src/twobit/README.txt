The files in this folder were taken from:

  https://github.com/ucscGenomeBrowser/kent-core/archive/refs/tags/v463.tar.gz

Only the following subset was copied to the Rtwobitlib/src/twobit/ folder:

  - src/inc/verbose.h
  - src/inc/portable.h
  - src/inc/portimpl.h
  - src/inc/dlist.h
  - src/inc/memalloc.h
  - src/inc/errAbort.h
  - src/inc/common.h
  - src/inc/hash.h
  - src/inc/dnaseq.h
  - src/inc/sig.h
  - src/inc/localmem.h
  - src/inc/bits.h
  - src/inc/dystring.h
  - src/inc/pipeline.h
  - src/inc/cheapcgi.h
  - src/inc/linefile.h
  - src/inc/obscure.h
  - src/inc/bPlusTree.h
  - src/inc/hex.h
  - src/inc/udc.h
  - src/inc/dnautil.h
  - src/inc/twoBit.h

  - src/lib/verbose.c
  - src/lib/portimpl.c
  - src/lib/osunix.c
  - src/lib/dlist.c
  - src/lib/memalloc.c
  - src/lib/errAbort.c
  - src/lib/common.c
  - src/lib/hash.c
  - src/lib/localmem.c
  - src/lib/bits.c
  - src/lib/dystring.c
  - src/lib/pipeline.c
  - src/lib/cheapcgi.c
  - src/lib/linefile.c
  - src/lib/obscure.c
  - src/lib/bPlusTree.c
  - src/inc/hex.c
  - src/inc/udc.c
  - src/lib/dnautil.c
  - src/lib/twoBit.c

In addition:

  (a) in osunix.c, portable.h, and portimpl.h:
      - remove functions: rTempName, maybeTouchFile, mysqlHost, listDir*,
        pathsInDirAndSubdirs

  (b) in portimpl.c:
      - remove #include "htmshell.h"
      - remove functions: setupWss, makeTempName, cgiDir, trashDir,
        machineSpeed, mkdirTrashDirectory, rPathsInDirAndSubdirs,
        pathsInDirAndSubdirs, envUpdate
      - remove variables: wss

  (c) in memalloc.c/memalloc.h:
      - remove functions: carefulTotalAllocated, pushCarefulMemHandler,
        carefulCheckHeap, carefulCountBlocksAllocated, carefulRealloc,
        carefulFree, carefulAlloc, carefulMemInit,
      - remove global variables: carefulMemHandler, carefulAlloced,
        carefulParent, carefulMutex, carefulMaxToAlloc, carefulAlignMask,
        carefulAlignAdd, carefulAlignSize

  (d) in errAbort.c/errAbort.h:
      - remove functions: warnWithBackTrace, isErrAbortInProgress,
        errAbortDebugnPushPopErr

        errnoWarn, vaWarn,
        pushWarnHandler, popWarnHandler, noWarnAbort, vaErrAbort,
        pushAbortHandler, popAbortHandler,

  (e) #include "sqlNum.h" was removed from common.c, pipeline.c, and obscure.c;

  (f) in common.c/common.h:
      - remove functions: wildMatch, loadSizes, sqlMatchLike, truncatef,
        vatruncatef, warnWithBackTrace

  (g) localmem.h was edited to remove the lmCloneString and lmCloneStringZ
      functions;

  (h) obscure.c and obscure.h were edited to remove the hashTwoColumnFile,
      currentVmPeak and readAllWords functions;

  (i) udc.c was edited to remove:
      - the net.h and htmlPage.h includes
      - add #include "obscure.h" (between linefile.h and portable.h includes)
      - remove functions: connInfoGetSocket, udcDataViaHttpOrFtp, udcInfoViaFtp,
        resolveUrl, udcInfoViaHttp, udcLoadCachedResolvedUrl, rCleanup,
        udcCleanup, bitRealDataSize, resolveUrlExec, makeUdcTmp,
        udcReadAndIgnore, ourRead
      - remove variables: info

  (j) in cheapcgi.c and cheapcgi.h: we only need the cgiDecode() function
      from this file so we remove everything except that:
      - remove functions: useTempFile, cgiRemoteAddr, cgiUserAgent,
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
      - remove variable: doUseTempFile, dumpStackOnSignal, inputSize,
        inputString, inputHash, inputList, haveCookiesHash, cookieHash,
        cookieList

  (k) in linefile.c and linefile.h:
      - remove any reference to the udc stuff and htslib/tabix stuff.

  (l) twoBit.c was edited to remove
        #include "net.h"
      and all
        if (hasProtocol(fileName))
            useUdc = TRUE;
      statements, as well as any reference to the udc stuff;

