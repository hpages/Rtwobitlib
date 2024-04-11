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
        pathsInDirAndSubdirs

  (c) #include "sqlNum.h" was removed from common.c, pipeline.c, and obscure.c;

  (d) common.c and common.h were edited to remove the wildMatch, loadSizes,
      and sqlMatchLike functions;

  (e) localmem.h was edited to remove the lmCloneString and lmCloneStringZ
      functions;

  (f) obscure.c and obscure.h were edited to remove the hashTwoColumnFile,
      currentVmPeak and readAllWords functions;

  (g) udc.c was edited to remove:
      - the net.h and htmlPage.h includes
      - add #include "obscure.h" (between linefile.h and portable.h includes)
      - functions connInfoGetSocket, udcDataViaHttpOrFtp, udcInfoViaFtp,
        resolveUrl, udcInfoViaHttp, udcLoadCachedResolvedUrl, rCleanup,
        and udcCleanup

  (h) in cheapcgi.c and cheapcgi.h:
      - remove functions: jsInlineFinish, cgiParseMultipart, cgiVarExists,
        cgiBoolean, cgiBooleanDefined, cgiChangeVar, cgiVarExcludeExcept,
        cgiVarExclude, cgiVarSet, cgiStringList, cgiFromFile, cgiScriptName,
        cgiScriptDirUrl, _cgiFindInput, getPostInput, cgiSpoof, initCgiInput,
        cgiIntExp, cgiOneChoice, cgiContinueHiddenVar, cgiString, cgiInt,
        cgiDouble, initSigHandlers, catchSignal, logCgiToStderr, cgiVarList,
        cgiOptionalDouble, cgiOptionalInt, cgiUsualString, cgiUsualDouble,
        cgiOptionalString, mustFindVarData, findVarData, cgiFromCommandLine,
        cgiMakeTextArea, cgiMakeTextAreaDisableable, cgiMakeTextVar,
        cgiMakeTextVarWithJs, cgiMakeOnKeypressTextVar, cgiMakeIntVar*

  (i) in linefile.c and linefile.h:
      - remove any reference to the udc stuff and htslib/tabix stuff.

  (j) twoBit.c was edited to remove
        #include "net.h"
      and all
        if (hasProtocol(fileName))
            useUdc = TRUE;
      statements, as well as any reference to the udc stuff;

