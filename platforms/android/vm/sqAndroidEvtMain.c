/* sqAndroidEvtMain.c -- support for Android event-driven VM.
 * 
 *   Copyright (C) 1996-2007 by Ian Piumarta and other authors/contributors
 *                              listed elsewhere in this file.
 *   Copyright (C) 2011 by Dmitry Golubovsky for the code added to support
 *   the event-driven VM.
 *	 
 *	 Copyright (C) 2 by Dmitry Golubovsky for the code added to support
 *
 *   All rights reserved.
 *   
 *   This file is part of Unix Squeak.
 * 
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the "Software"),
 *   to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *   and/or sell copies of the Software, and to permit persons to whom the
 *   Software is furnished to do so, subject to the following conditions:
 * 
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *   DEALINGS IN THE SOFTWARE.
 */

/* Author: Ian Piumarta <ian.piumarta@squeakland.org>
 * Merged with
 *	http://squeakvm.org/svn/squeak/trunk/platforms/unix/vm/sqUnixMain.c
 *	Revision: 2148
 *	Last Changed Rev: 2132
 * by eliot Wed Jan 20 10:57:26 PST 2010
 */

#include "sq.h"
#include "sqMemoryAccess.h"
#include "sqAndroidEvtBeat.h"
#include "sqaio.h"
#include "sqAndroidCharConv.h"
#include "debug.h"
#include "setjmp.h"

#ifdef ioMSecs
# undef ioMSecs
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#if !defined(NOEXECINFO)
//# include <execinfo.h>
# define BACKTRACE_DEPTH 64
#endif
#if __FreeBSD__
# include <sys/ucontext.h>
#endif

#if defined(__alpha__) && defined(__osf__)
# include <sys/sysinfo.h>
# include <sys/proc.h>
#endif

#define DEBUG_IMAGE

#undef	DEBUG_MODULES

#undef	IMAGE_DUMP				/* define to enable SIGHUP and SIGQUIT handling */

#define IMAGE_NAME_SIZE MAXPATHLEN

#define DefaultHeapSize		  20	     	/* megabytes BEYOND actual image size */
#define DefaultMmapSize		1024     	/* megabytes of virtual memory */

       char  *documentName= 0;			/* name if launced from document */
       char   shortImageName[MAXPATHLEN+1];	/* image name */
       char   imageName[MAXPATHLEN+1];		/* full path to image */
static char   vmName[MAXPATHLEN+1];		/* full path to vm */
       char   vmPath[MAXPATHLEN+1];		/* full path to image directory */

       int    argCnt=		0;	/* global copies for access from plugins */
       char **argVec=		0;
       char **envVec=		0;

static int    vmArgCnt=		0;	/* for getAttributeIntoLength() */
static char **vmArgVec=		0;
static int    squeakArgCnt=	0;
static char **squeakArgVec=	0;

static int    extraMemory=	0;
       int    useMmap=		DefaultMmapSize * 1024 * 1024;

static int    useItimer=	1;	/* 0 to disable itimer-based clock */
static int    installHandlers=	1;	/* 0 to disable sigusr1 & sigsegv handlers */
       int    noEvents=		0;	/* 1 to disable new event handling */
       int    noSoundMixer=	0;	/* 1 to disable writing sound mixer levels */
       char  *squeakPlugins=	0;	/* plugin path */
       int    runAsSingleInstance=0;
#if !STACKVM && !COGVM
       int    useJit=		0;	/* use default */
       int    jitProcs=		0;	/* use default */
       int    jitMaxPIC=	0;	/* use default */
#else
# define useJit 0
#endif
       int    withSpy=		0;

       int    uxDropFileCount=	0;	/* number of dropped items	*/
       char **uxDropFileNames=	0;	/* dropped filenames		*/

       int    textEncodingUTF8= 0;	/* 1 if copy from external selection uses UTF8 */

#if defined(IMAGE_DUMP)
static int    dumpImageFile=	0;	/* 1 after SIGHUP received */
#endif

#if defined(DARWIN)
int inModalLoop= 0;
#endif

int sqIgnorePluginErrors	= 0;
int runInterpreter		= 1;

#include "SqModule.h"
#include "SqDisplay.h"
#include "SqSound.h"

struct SqDisplay *dpy= 0;
struct SqSound   *snd= 0;

extern struct SqModule sound_null, display_android;
jmp_buf jmpBufExit;

extern void dumpPrimTraceLog(void);

int bigEndian;

/*
 * In the Cog VMs time management is in platforms/unix/vm/sqUnixHeartbeat.c.
 */
#if !STACKVM
/*** timer support ***/

#define	LOW_RES_TICK_MSECS	20	/* 1/50 second resolution */

static unsigned int   lowResMSecs= 0;
static struct timeval startUpTime;

static void sigalrm(int signum)
{
  lowResMSecs+= LOW_RES_TICK_MSECS;
  forceInterruptCheck();
}

void
ioInitTime(void)
{
  /* set up the micro/millisecond clock */
  gettimeofday(&startUpTime, 0);
  if (useItimer)
    {
      /* set up the low-res (50th second) millisecond clock */
      /* WARNING: all system calls must check for EINTR!!! */
      {
	struct sigaction sa;
	sigset_t ss1, ss2;
	sigemptyset(&ss1);
	sigprocmask(SIG_BLOCK, &ss1, &ss2);
	sa.sa_handler= sigalrm;
	sa.sa_mask= ss2;
#      ifdef SA_RESTART	/* we're probably on Linux */
	sa.sa_flags= SA_RESTART;
#      else
	sa.sa_flags= 0;	/* assume we already have BSD behaviour */
#      endif
#      if defined(__linux__) && !defined(__ia64) && !defined(__alpha__)
	sa.sa_restorer= 0;
#      endif
	sigaction(SIGALRM, &sa, 0);
      }
      {
	struct itimerval iv;
	iv.it_interval.tv_sec= 0;
	iv.it_interval.tv_usec= LOW_RES_TICK_MSECS * 1000;
	iv.it_value= iv.it_interval;
	setitimer(ITIMER_REAL, &iv, 0);
      }
    }
}

sqInt ioLowResMSecs(void)
{
  return (useItimer)
    ? lowResMSecs
    : ioMSecs();
}

sqInt ioMSecs(void)
{
  struct timeval now;
  gettimeofday(&now, 0);
  if ((now.tv_usec-= startUpTime.tv_usec) < 0)
    {
      now.tv_usec+= 1000000;
      now.tv_sec-= 1;
    }
  now.tv_sec-= startUpTime.tv_sec;
  return lowResMSecs= (now.tv_usec / 1000 + now.tv_sec * 1000);
}

sqInt ioMicroMSecs(void)
{
  /* return the highest available resolution of the millisecond clock */
  return ioMSecs();	/* this already to the nearest millisecond */
}

time_t convertToSqueakTime(time_t unixTime);

/* returns the local wall clock time */
sqInt ioSeconds(void)
{
  return convertToSqueakTime(time(0));
}

#define SecondsFrom1901To1970      2177452800ULL
#define MicrosecondsFrom1901To1970 2177452800000000ULL

#define MicrosecondsPerSecond 1000000ULL
#define MillisecondsPerSecond 1000ULL

#define MicrosecondsPerMillisecond 1000ULL
/* Compute the current VM time basis, the number of microseconds from 1901. */

static unsigned long long
currentUTCMicroseconds()
{
	struct timeval utcNow;

	gettimeofday(&utcNow,0);
	return ((utcNow.tv_sec * MicrosecondsPerSecond) + utcNow.tv_usec)
			+ MicrosecondsFrom1901To1970;
}

usqLong
ioUTCMicroseconds() { return currentUTCMicroseconds(); }

/* This is an expensive interface for use by profiling code that wants the time
 * now rather than as of the last heartbeat.
 */
usqLong
ioUTCMicrosecondsNow() { return currentUTCMicroseconds(); }
#endif /* STACKVM */

time_t convertToSqueakTime(time_t unixTime)
{
#ifdef HAVE_TM_GMTOFF
  unixTime+= localtime(&unixTime)->tm_gmtoff;
#else
# ifdef HAVE_TIMEZONE
  unixTime+= ((daylight) * 60*60) - timezone;
# else
#  error: cannot determine timezone correction
# endif
#endif
  /* Squeak epoch is Jan 1, 1901.  Unix epoch is Jan 1, 1970: 17 leap years
     and 52 non-leap years later than Squeak. */
  return unixTime + ((52*365UL + 17*366UL) * 24*60*60UL);
}


/*** VM & Image File Naming ***/


/* copy src filename to target, if src is not an absolute filename,
 * prepend the cwd to make target absolute
  */
static void pathCopyAbs(char *target, const char *src, size_t targetSize)
{
  if (src[0] == '/')
    strcpy(target, src);
  else
    {
      0 == getcwd(target, targetSize);
      strcat(target, "/");
      strcat(target, src);
    }
}


static void recordFullPathForVmName(const char *localVmName)
{
#if defined(__linux__)
  char	 name[MAXPATHLEN+1];
  int    len;

  if ((len= readlink("/proc/self/exe", name, sizeof(name))) > 0)
    {
      struct stat st;
      name[len]= '\0';
      if (!stat(name, &st))
	localVmName= name;
    }
#endif

  /* get canonical path to vm */
  if (realpath(localVmName, vmPath) == 0)
    pathCopyAbs(vmPath, localVmName, sizeof(vmPath));

  /* truncate vmPath to dirname */
  {
    int i= 0;
    for (i= strlen(vmPath); i >= 0; i--)
      if ('/' == vmPath[i])
	{
	  vmPath[i+1]= '\0';
	  break;
	}
  }
}

static void recordFullPathForImageName(const char *localImageName)
{
  struct stat s;
  /* get canonical path to image */
  if ((stat(localImageName, &s) == -1) || (realpath(localImageName, imageName) == 0))
    pathCopyAbs(imageName, localImageName, sizeof(imageName));
}

/* vm access */

sqInt imageNameSize(void)
{
  return strlen(imageName);
}

sqInt imageNameGetLength(sqInt sqImageNameIndex, sqInt length)
{
  char *sqImageName= pointerForOop(sqImageNameIndex);
  int count, i;

  count= strlen(imageName);
  count= (length < count) ? length : count;

  /* copy the file name into the Squeak string */
  for (i= 0; i < count; i++)
    sqImageName[i]= imageName[i];

  return count;
}


sqInt imageNamePutLength(sqInt sqImageNameIndex, sqInt length)
{
  char *sqImageName= pointerForOop(sqImageNameIndex);
  int count, i;

  count= (IMAGE_NAME_SIZE < length) ? IMAGE_NAME_SIZE : length;

  /* copy the file name into a null-terminated C string */
  for (i= 0; i < count; i++)
    imageName[i]= sqImageName[i];
  imageName[count]= 0;

  dpy->winSetName(imageName);

  return count;
}


char *getImageName(void)
{
  return imageName;
}


/*** VM Home Directory Path ***/


sqInt vmPathSize(void)
{
  return strlen(vmPath);
}

sqInt vmPathGetLength(sqInt sqVMPathIndex, sqInt length)
{
  char *stVMPath= pointerForOop(sqVMPathIndex);
  int count, i;

  count= strlen(vmPath);
  count= (length < count) ? length : count;

  /* copy the file name into the Squeak string */
  for (i= 0; i < count; i++)
    stVMPath[i]= vmPath[i];

  return count;
}

char* ioGetLogDirectory(void) { return ""; };
sqInt ioSetLogDirectoryOfSize(void* lblIndex, sqInt sz){ return 1; }


/*** power management ***/


sqInt ioDisablePowerManager(sqInt disableIfNonZero)
{
  return true;
}


/*** Access to system attributes and command-line arguments ***/


/* OS_TYPE may be set in configure.in and passed via the Makefile */

#ifndef OS_TYPE
# ifdef UNIX
#   define OS_TYPE "unix"
# else
#  define OS_TYPE "unknown"
# endif
#endif

static char *getAttribute(sqInt id)
{
  if (id < 0)	/* VM argument */
    {
      if (-id  < vmArgCnt)
	return vmArgVec[-id];
    }
  else
    switch (id)
      {
      case 0:
	return vmName[0] ? vmName : vmArgVec[0];
      case 1:
	return imageName;
      case 1001:
	/* OS type: "unix", "win32", "mac", ... */
	return OS_TYPE;
      case 1002:
	/* OS name: "solaris2.5" on unix, "win95" on win32, ... */
	return VM_HOST_OS;
      case 1003:
	/* processor architecture: "68k", "x86", "PowerPC", ...  */
	return VM_HOST_CPU;
      case 1004:
	/* Interpreter version string */
	return  (char *)interpreterVersion;
      case 1005:
	/* window system name */
	return  dpy->winSystemName();
      case 1006:
	/* vm build string */
	return VM_BUILD_STRING;
#if STACKVM
      case 1007: { /* interpreter build info */
	extern char *__interpBuildInfo;
	return __interpBuildInfo;
      }
# if COGVM
      case 1008: { /* cogit build info */
	extern char *__cogitBuildInfo;
	return __cogitBuildInfo;
      }
# endif
#endif
      default:
	if ((id - 2) < squeakArgCnt)
	  return squeakArgVec[id - 2];
      }
  success(false);
  return "";
}

sqInt attributeSize(sqInt id)
{
  return strlen(getAttribute(id));
}

sqInt getAttributeIntoLength(sqInt id, sqInt byteArrayIndex, sqInt length)
{
  if (length > 0)
    strncpy(pointerForOop(byteArrayIndex), getAttribute(id), length);
  return 0;
}


/*** event handling ***/


sqInt inputEventSemaIndex= 0;


/* set asynchronous input event semaphore  */

sqInt ioSetInputSemaphore(sqInt semaIndex)
{
  if ((semaIndex == 0) || (noEvents == 1))
    success(false);
  else
    inputEventSemaIndex= semaIndex;
  return true;
}


/*** display functions ***/

sqInt ioFormPrint(sqInt bitsAddr, sqInt width, sqInt height, sqInt depth, double hScale, double vScale, sqInt landscapeFlag)
{
  return dpy->ioFormPrint(bitsAddr, width, height, depth, hScale, vScale, landscapeFlag);
}

sqInt ioBeep(void)				 { return dpy->ioBeep(); }

#if defined(IMAGE_DUMP)

static void emergencyDump(int quit)
{
  extern sqInt preSnapshot(void);
  extern sqInt postSnapshot(void);
  extern void writeImageFile(sqInt);
  char savedName[MAXPATHLEN];
  char baseName[MAXPATHLEN];
  char *term;
  int  dataSize, i;
  strncpy(savedName, imageName, MAXPATHLEN);
  strncpy(baseName, imageName, MAXPATHLEN);
  if ((term= strrchr(baseName, '.')))
    *term= '\0';
  for (i= 0; ++i;)
    {
      struct stat sb;
      sprintf(imageName, "%s-emergency-dump-%d.image", baseName, i);
      if (stat(imageName, &sb))
	break;
    }
  dataSize= preSnapshot();
  writeImageFile(dataSize);

#if STACKVM
  printf("\nMost recent primitives\n");
  dumpPrimTraceLog();
#endif
  fprintf(stderr, "\n");
  printCallStack();
  fprintf(stderr, "\nTo recover valuable content from this image:\n");
  fprintf(stderr, "    squeak %s\n", imageName);
  fprintf(stderr, "and then evaluate\n");
  fprintf(stderr, "    Smalltalk processStartUpList: true\n");
  fprintf(stderr, "in a workspace.  DESTROY the dumped image after recovering content!");

  if (quit) abort();
  strncpy(imageName, savedName, sizeof(imageName));
}

#endif

sqInt ioProcessEvents(void)
{
	sqInt result;
	extern sqInt inIOProcessEvents;


#if defined(IMAGE_DUMP)
	if (dumpImageFile) {
		emergencyDump(0);
		dumpImageFile= 0;
	}
#endif
	/* inIOProcessEvents controls ioProcessEvents.  If negative then
	 * ioProcessEvents is disabled.  If >= 0 inIOProcessEvents is incremented
	 * to avoid reentrancy (i.e. for native GUIs).
	 */
	if (inIOProcessEvents) return;
	inIOProcessEvents += 1;

	result = dpy->ioProcessEvents();

	if (inIOProcessEvents > 0)
		inIOProcessEvents -= 1;


	return result;
}

void	ioDrainEventQueue() {}

sqInt ioScreenDepth(void)		 { return dpy->ioScreenDepth(); }
sqInt ioScreenSize(void)		 { return dpy->ioScreenSize(); }

sqInt ioSetCursorWithMask(sqInt cursorBitsIndex, sqInt cursorMaskIndex, sqInt offsetX, sqInt offsetY)
{
  return dpy->ioSetCursorWithMask(cursorBitsIndex, cursorMaskIndex, offsetX, offsetY);
}

sqInt ioSetCursorARGB(sqInt cursorBitsIndex, sqInt extentX, sqInt extentY, sqInt offsetX, sqInt offsetY)
{
  return dpy->ioSetCursorARGB(cursorBitsIndex, extentX, extentY, offsetX, offsetY);
}

sqInt ioSetCursor(sqInt cursorBitsIndex, sqInt offsetX, sqInt offsetY)
{
  return ioSetCursorWithMask(cursorBitsIndex, 0, offsetX, offsetY);
}

sqInt ioSetFullScreen(sqInt fullScreen)	{ return dpy->ioSetFullScreen(fullScreen); }
sqInt ioForceDisplayUpdate(void)	{ return dpy->ioForceDisplayUpdate(); }

sqInt ioShowDisplay(sqInt dispBitsIndex, sqInt width, sqInt height, sqInt depth, sqInt l, sqInt r, sqInt t, sqInt b)
{
  return dpy->ioShowDisplay(dispBitsIndex, width, height, depth, l, r, t, b);
}

sqInt ioHasDisplayDepth(sqInt i) { return dpy->ioHasDisplayDepth(i); }

sqInt ioSetDisplayMode(sqInt width, sqInt height, sqInt depth, sqInt fullscreenFlag)
{
  return dpy->ioSetDisplayMode(width, height, depth, fullscreenFlag);
}

sqInt clipboardSize(void)
{
  return dpy->clipboardSize();
}

sqInt clipboardWriteFromAt(sqInt count, sqInt byteArrayIndex, sqInt startIndex)
{
  return dpy->clipboardWriteFromAt(count, byteArrayIndex, startIndex);
}

sqInt clipboardReadIntoAt(sqInt count, sqInt byteArrayIndex, sqInt startIndex)
{
  return dpy->clipboardReadIntoAt(count, byteArrayIndex, startIndex);
}

char **clipboardGetTypeNames(void)
{
  return dpy->clipboardGetTypeNames();
}

sqInt clipboardSizeWithType(char *typeName, int ntypeName)
{
  return dpy->clipboardSizeWithType(typeName, ntypeName);
}

void clipboardWriteWithType(char *data, size_t nData, char *typeName, size_t nTypeNames, int isDnd, int isClaiming)
{
  dpy->clipboardWriteWithType(data, nData, typeName, nTypeNames, isDnd, isClaiming);
}

sqInt ioGetButtonState(void)		{ return dpy->ioGetButtonState(); }
sqInt ioPeekKeystroke(void)		{ return dpy->ioPeekKeystroke(); }
sqInt ioGetKeystroke(void)		{ return dpy->ioGetKeystroke(); }
sqInt ioGetNextEvent(sqInputEvent *evt)	{ return dpy->ioGetNextEvent(evt); }

sqInt ioMousePoint(void)		{ return dpy->ioMousePoint(); }

/*** Window labeling ***/
char* ioGetWindowLabel(void) {return "";}

sqInt ioSetWindowLabelOfSize(void* lbl, sqInt size)
{ return dpy->hostWindowSetTitle((long)dpy->ioGetWindowHandle(), lbl, size); }

sqInt ioIsWindowObscured(void) {return false;}

/** Misplaced Window-Size stubs, so the VM will link. **/
sqInt ioGetWindowWidth()
{ int wh = dpy->hostWindowGetSize((long)dpy->ioGetWindowHandle());
  return wh >> 16; } 

sqInt ioGetWindowHeight()
{ int wh = dpy->hostWindowGetSize((long)dpy->ioGetWindowHandle());
  return (short)wh; } 

void* ioGetWindowHandle(void) { return dpy->ioGetWindowHandle(); }

sqInt ioSetWindowWidthHeight(sqInt w, sqInt h)
{ return dpy->hostWindowSetSize((long)dpy->ioGetWindowHandle(),w,h); }

/*** Drag and Drop ***/

sqInt dndOutStart(char *types, int ntypes)	{ return dpy->dndOutStart(types, ntypes); }
sqInt dndOutAcceptedType(char *type, int ntype)	{ return dpy->dndOutAcceptedType(type, ntype); }
void  dndOutSend(char *bytes, int nbytes)	{        dpy->dndOutSend(bytes, nbytes); }
void  dndReceived(char *fileName)			{        dpy->dndReceived(fileName); }

/*** OpenGL ***/

int verboseLevel= 1;

struct SqDisplay *ioGetDisplayModule(void)	{ return dpy; }

void *ioGetDisplay(void)			{ return dpy->ioGetDisplay(); }
void *ioGetWindow(void)				{ return dpy->ioGetWindow(); }
sqInt ioGLinitialise(void)			{ return dpy->ioGLinitialise(); }

sqInt  ioGLcreateRenderer(glRenderer *r, sqInt x, sqInt y, sqInt w, sqInt h, sqInt flags)
{
  return dpy->ioGLcreateRenderer(r, x, y, w, h, flags);
}

sqInt ioGLmakeCurrentRenderer(glRenderer *r)	{ return dpy->ioGLmakeCurrentRenderer(r); }
void  ioGLdestroyRenderer(glRenderer *r)	{	 dpy->ioGLdestroyRenderer(r); }
void  ioGLswapBuffers(glRenderer *r)		{	 dpy->ioGLswapBuffers(r); }

void  ioGLsetBufferRect(glRenderer *r, sqInt x, sqInt y, sqInt w, sqInt h)
{
  dpy->ioGLsetBufferRect(r, x, y, w, h);
}


sqInt  primitivePluginBrowserReady(void)	{ return dpy->primitivePluginBrowserReady(); }
sqInt  primitivePluginRequestURLStream(void)	{ return dpy->primitivePluginRequestURLStream(); }
sqInt  primitivePluginRequestURL(void)		{ return dpy->primitivePluginRequestURL(); }
sqInt  primitivePluginPostURL(void)		{ return dpy->primitivePluginPostURL(); }
sqInt  primitivePluginRequestFileHandle(void)	{ return dpy->primitivePluginRequestFileHandle(); }
sqInt  primitivePluginDestroyRequest(void)	{ return dpy->primitivePluginDestroyRequest(); }
sqInt  primitivePluginRequestState(void)	{ return dpy->primitivePluginRequestState(); }


/*** errors ***/

static void outOfMemory(void)
{
  /* pushing stderr outputs the error report on stderr instead of stdout */
  pushOutputFile((char *)STDERR_FILENO);
  error("out of memory\n");
}

/* Print an error message, possibly a stack trace, and exit. */
/* Disable Intel compiler inlining of error which is used for breakpoints */
#pragma auto_inline off
void
error(char *msg)
{
//	reportStackState(msg,0,0,0);
	abort();
}
#pragma auto_inline on

/* construct /dir/for/image/crash.dmp if a / in imageName else crash.dmp */
static void
getCrashDumpFilenameInto(char *buf)
{
  char *slash;

  strcpy(buf,imageName);
  slash = strrchr(buf,'/');
  strcpy(slash ? slash + 1 : buf, "crash.dmp");
}


#if defined(IMAGE_DUMP)
static void
sighup(int ignore) { dumpImageFile= 1; }

static void
sigquit(int ignore) { emergencyDump(1); }
#endif


/* built-in main vm module */


static int strtobkm(const char *str)
{
  char *suffix;
  int value= strtol(str, &suffix, 10);
  switch (*suffix)
    {
    case 'k': case 'K':
      value*= 1024;
      break;
    case 'm': case 'M':
      value*= 1024*1024;
      break;
    }
  return value;
}

#if !STACKVM && !COGVM
static int jitArgs(char *str)
{
  char *endptr= str;
  int  args= 3;				/* default JIT mode = fast compiler */
  
  if (*str == '\0') return args;
  if (*str != ',')
    args= strtol(str, &endptr, 10);	/* mode */
  while (*endptr == ',')		/* [,debugFlag]* */
    args|= (1 << (strtol(endptr + 1, &endptr, 10) + 8));
  return args;
}
#endif /* !STACKVM && !COGVM */


# include <locale.h>
static void vm_parseEnvironment(void)
{
  char *ev= setlocale(LC_CTYPE, "");
  if (ev)
    setLocaleEncoding(ev);
  else
    fprintf(stderr, "setlocale() failed (check values of LC_CTYPE, LANG and LC_ALL)\n");

  if (documentName)
    strcpy(shortImageName, documentName);
  else if ((ev= getenv("SQUEAK_IMAGE")))
    strcpy(shortImageName, ev);
  else
    strcpy(shortImageName, "squeak.image");

  if ((ev= getenv("SQUEAK_MEMORY")))	{
    extraMemory= strtobkm(ev); 
    dprintf(9, "extraMemory => %d (env)\n", extraMemory);
  }
  if ((ev= getenv("SQUEAK_MMAP")))	useMmap= strtobkm(ev);
  if ((ev= getenv("SQUEAK_PLUGINS")))	squeakPlugins= strdup(ev);
  if ((ev= getenv("SQUEAK_NOEVENTS")))	noEvents= 1;
  if ((ev= getenv("SQUEAK_NOTIMER")))	useItimer= 0;
#if !STACKVM && !COGVM
  if ((ev= getenv("SQUEAK_JIT")))	useJit= jitArgs(ev);
  if ((ev= getenv("SQUEAK_PROCS")))	jitProcs= atoi(ev);
  if ((ev= getenv("SQUEAK_MAXPIC")))	jitMaxPIC= atoi(ev);
#endif /* !STACKVM && !COGVM */
  if ((ev= getenv("SQUEAK_ENCODING")))	setEncoding(&sqTextEncoding, ev);
  if ((ev= getenv("SQUEAK_PATHENC")))	setEncoding(&uxPathEncoding, ev);
  if ((ev= getenv("SQUEAK_TEXTENC")))	setEncoding(&uxTextEncoding, ev);

}


static void usage(void);
static void versionInfo(void);


static int parseModuleArgument(int argc, char **argv, struct SqModule **addr, char *type, char *name)
{
  if (*addr)
    {
      fprintf(stderr, "option '%s' conflicts with previously-loaded module '%s'\n", *argv, (*addr)->name);
      exit(1);
    }
  *addr= requireModule(type, name);
  return (*addr)->parseArgument(argc, argv);
}


static int vm_parseArgument(int argc, char **argv)
{
  /* vm arguments */

  if      (!strcmp(argv[0], "-noevents"))	{ noEvents	= 1;	return 1; }
  else if (!strcmp(argv[0], "-nomixer"))	{ noSoundMixer	= 1;	return 1; }
  else if (!strcmp(argv[0], "-notimer"))	{ useItimer	= 0;	return 1; }
  else if (!strcmp(argv[0], "-nohandlers"))	{ installHandlers= 0;	return 1; }
#if !STACKVM && !COGVM
  else if (!strncmp(argv[0],"-jit", 4))		{ useJit	= jitArgs(argv[0]+4);	return 1; }
  else if (!strcmp(argv[0], "-nojit"))		{ useJit	= 0;	return 1; }
  else if (!strcmp(argv[0], "-spy"))		{ withSpy	= 1;	return 1; }
#endif /* !STACKVM && !COGVM */
  else if (!strcmp(argv[0], "-version"))	{ versionInfo();	return 1; }
  else if (!strcmp(argv[0], "-single"))		{ runAsSingleInstance=1; return 1; }
  /* option requires an argument */
  else if (argc > 1)
    {
      if (!strcmp(argv[0], "-memory"))	{ 
	extraMemory = strtobkm(argv[1]);
	dprintf(9, "extraMemory => %d (arg)", extraMemory);
	return 2; }
#if !STACKVM && !COGVM
      else if (!strcmp(argv[0], "-procs"))	{ jitProcs=	 atoi(argv[1]);		 return 2; }
      else if (!strcmp(argv[0], "-maxpic"))	{ jitMaxPIC=	 atoi(argv[1]);		 return 2; }
#endif /* !STACKVM && !COGVM */
      else if (!strcmp(argv[0], "-mmap"))	{ useMmap=	 strtobkm(argv[1]);	 return 2; }
      else if (!strcmp(argv[0], "-plugins"))	{ squeakPlugins= strdup(argv[1]);	 return 2; }
      else if (!strcmp(argv[0], "-encoding"))	{ setEncoding(&sqTextEncoding, argv[1]); return 2; }
      else if (!strcmp(argv[0], "-pathenc"))	{ setEncoding(&uxPathEncoding, argv[1]); return 2; }
#if STACKVM && !COGVM || NewspeakVM
	  else if (!strcmp(argv[0], "-sendtrace")) { extern sqInt sendTrace; sendTrace = 1; return 1; }
#endif
#if STACKVM || NewspeakVM
      else if (!strcmp(argv[0], "-breaksel")) { 
		extern void setBreakSelector(char *);
		setBreakSelector(argv[1]);
		return 2; }
#endif
#if STACKVM
      else if (!strcmp(argv[0], "-eden")) {
		extern sqInt desiredEdenBytes;
		desiredEdenBytes = strtobkm(argv[1]);
		return 2; }
      else if (!strcmp(argv[0], "-leakcheck")) { 
		extern sqInt checkForLeaks;
		checkForLeaks = atoi(argv[1]);	 
		return 2; }
      else if (!strcmp(argv[0], "-stackpages")) {
		extern sqInt desiredNumStackPages;
		desiredNumStackPages = atoi(argv[1]);
		return 2; }
      else if (!strcmp(argv[0], "-noheartbeat")) { 
		extern sqInt suppressHeartbeatFlag;
		suppressHeartbeatFlag = 1;
		return 1; }
#endif /* STACKVM */
#if COGVM
      else if (!strcmp(argv[0], "-codesize")) { 
		extern sqInt desiredCogCodeSize;
		desiredCogCodeSize = strtobkm(argv[1]);	 
		return 2; }
# define TLSLEN (sizeof("-sendtrace")-1)
      else if (!strncmp(argv[0], "-sendtrace", TLSLEN)) { 
		extern int traceLinkedSends;
		char *equalsPos = strchr(argv[0],'=');

		if (!equalsPos) {
			traceLinkedSends = 1;
			return 1;
		}
		if (equalsPos - argv[0] != TLSLEN
		  || (equalsPos[1] != '-' && !isdigit(equalsPos[1])))
			return 0;

		traceLinkedSends = atoi(equalsPos + 1);
		return 1; }
      else if (!strcmp(argv[0], "-tracestores")) { 
		extern sqInt traceStores;
		traceStores = 1;
		return 1; }
      else if (!strcmp(argv[0], "-cogmaxlits")) { 
		extern sqInt maxLiteralCountForCompile;
		maxLiteralCountForCompile = strtobkm(argv[1]);	 
		return 2; }
      else if (!strcmp(argv[0], "-cogminjumps")) { 
		extern sqInt minBackwardJumpCountForCompile;
		minBackwardJumpCountForCompile = strtobkm(argv[1]);	 
		return 2; }
#endif /* COGVM */
      else if (!strcmp(argv[0], "-textenc"))
	{
	  char *buf= (char *)malloc(strlen(argv[1]) + 1);
	  int len, i;
	  strcpy(buf, argv[1]);
	  len= strlen(buf);
	  for (i= 0;  i < len;  ++i)
	    buf[i]= toupper(buf[i]);
	  if ((!strcmp(buf, "UTF8")) || (!strcmp(buf, "UTF-8")))
	    textEncodingUTF8= 1;
	  else
	    setEncoding(&uxTextEncoding, buf);
	  free(buf);
	  return 2;
	}
    }
  return 0;	/* option not recognised */
}


static void vm_printUsage(void)
{
  printf("\nCommon <option>s:\n");
  printf("  -encoding <enc>       set the internal character encoding (default: MacRoman)\n");
  printf("  -help                 print this help message, then exit\n");
  printf("  -memory <size>[mk]    use fixed heap size (added to image size)\n");
  printf("  -mmap <size>[mk]      limit dynamic heap size (default: %dm)\n", DefaultMmapSize);
#if STACKVM || NewspeakVM
  printf("  -breaksel selector    set breakpoint on send of selector\n");
#endif
#if STACKVM
  printf("  -eden <size>[mk]      use given eden size\n");
  printf("  -leakcheck num        check for leaks in the heap\n");
  printf("  -stackpages <num>     use given number of stack pages\n");
#endif
  printf("  -noevents             disable event-driven input support\n");
  printf("  -nohandlers           disable sigsegv & sigusr1 handlers\n");
  printf("  -pathenc <enc>        set encoding for pathnames (default: UTF-8)\n");
  printf("  -plugins <path>       specify alternative plugin location (see manpage)\n");
  printf("  -textenc <enc>        set encoding for external text (default: UTF-8)\n");
  printf("  -version              print version information, then exit\n");
  printf("  -vm-<sys>-<dev>       use the <dev> driver for <sys> (see below)\n");
#if COGVM
  printf("  -codesize <size>[mk]  set machine code memory to bytes\n");
  printf("  -sendtrace[=num]      enable send tracing (optionally to a specific value)\n");
  printf("  -tracestores          enable store tracing (assert check stores)\n");
  printf("  -cogmaxlits <n>       set max number of literals for methods compiled to machine code\n");
  printf("  -cogminjumps <n>      set min number of backward jumps for interpreted methods to be considered for compilation to machine code\n");
#endif
#if 1
  printf("Deprecated:\n");
# if !STACKVM
  printf("  -jit                  enable the dynamic compiler (if available)\n");
# endif
  printf("  -notimer              disable interval timer for low-res clock \n");
  printf("  -display <dpy>        quivalent to '-vm-display-X11 -display <dpy>'\n");
  printf("  -headless             quivalent to '-vm-display-X11 -headless'\n");
  printf("  -nodisplay            quivalent to '-vm-display-null'\n");
  printf("  -nomixer              disable modification of mixer settings\n");
  printf("  -nosound              quivalent to '-vm-sound-null'\n");
  printf("  -quartz               quivalent to '-vm-display-Quartz'\n");
#endif
}


static void vm_printUsageNotes(void)
{
  printf("  If `-memory' is not specified then the heap will grow dynamically.\n");
  printf("  <argument>s are ignored, but are processed by the Squeak image.\n");
  printf("  The first <argument> normally names a Squeak `script' to execute.\n");
  printf("  Precede <arguments> by `--' to use default image.\n");
}


static void *vm_makeInterface(void)
{
  fprintf(stderr, "this cannot happen\n");
  abort();
}


SqModuleDefine(vm, Module);


/*** options processing ***/


char *getVersionInfo(int verbose)
{
  extern int   vm_serial;
  extern char *vm_date, *cc_version, *ux_version;
  char *info= (char *)malloc(4096);
  info[0]= '\0';

  if (verbose)
    sprintf(info+strlen(info), "Squeak VM version: ");
  sprintf(info+strlen(info), "%s #%d", VM_VERSION, vm_serial);
#if defined(USE_XSHM)
  sprintf(info+strlen(info), " XShm");
#endif
  sprintf(info+strlen(info), " %s %s\n", vm_date, cc_version);
  if (verbose)
    sprintf(info+strlen(info), "Built from: ");
  sprintf(info+strlen(info), "Event-Driven %s\n", interpreterVersion);
  if (verbose)
    sprintf(info+strlen(info), "Build host: ");
  sprintf(info+strlen(info), "%s\n", ux_version);
  sprintf(info+strlen(info), "plugin path: %s [default: %s]\n", squeakPlugins, vmPath);
  return info;
}


static void versionInfo(void)
{
  printf("%s", getVersionInfo(0));
  exit(0);
}


static void parseArguments(int argc, char **argv)
{
# define skipArg()	(--argc, argv++)
# define saveArg()	(vmArgVec[vmArgCnt++]= *skipArg())

  saveArg();	/* vm name */

  while ((argc > 0) && (**argv == '-'))	/* more options to parse */
    {
      struct SqModule *m= 0;
      int n= 0;
      if (!strcmp(*argv, "--"))		/* escape from option processing */
	break;
#    ifdef DEBUG_IMAGE
      dprintf(9, "parseArgument n = %d\n", n);
#    endif
      if (n == 0)			/* option not recognised */
	{
	  fprintf(stderr, "unknown option: %s\n", argv[0]);
	}
      while (n--)
	saveArg();
    }
  if (!argc)
    return;
  if (!strcmp(*argv, "--"))
    skipArg();
  else					/* image name */
    {
      if (!documentName)
	strcpy(shortImageName, saveArg());
      if (!strstr(shortImageName, ".image"))
	strcat(shortImageName, ".image");
    }
  /* save remaining arguments as Squeak arguments */
  while (argc > 0) {
    squeakArgVec[squeakArgCnt++]= *skipArg();
    dprintf(9, "%s\n", squeakArgVec[squeakArgCnt - 1]);
  }

# undef saveArg
# undef skipArg
}


/*** main ***/


static void imageNotFound(char *imageName)
{
  /* image file is not found */
  dprintf(2,     
	  "Could not open the Squeak image file `%s'.\n"
	  "\n"
	  "There are three ways to open a Squeak image file.  You can:\n"
	  "  1. Put copies of the default image and changes files in this directory.\n"
	  "  2. Put the name of the image file on the command line when you\n"
	  "     run squeak (use the `-help' option for more information).\n"
	  "  3. Set the environment variable SQUEAK_IMAGE to the name of the image\n"
	  "     that you want to use by default.\n"
	  "\n"
	  "For more information, type: `man squeak' (without the quote characters).\n",
	  imageName);
  exit(1);
}


void imgInit(void)
{
  /* read the image file and allocate memory for Squeak heap */
  for (;;)
    {
      FILE *f= 0;
      struct stat sb;
      char imageName[MAXPATHLEN];
dprintf(5, "imgInit %s\n", shortImageName);
      sq2uxPath(shortImageName, strlen(shortImageName), imageName, 1000, 1);
dprintf(5, "%s %s\n", imageName, shortImageName);
      if ((  (-1 == stat(imageName, &sb)))
	  || ( 0 == (f= fopen(imageName, "r"))))
	{
	  if (dpy->winImageFind(shortImageName, sizeof(shortImageName)))
	    continue;
	  dpy->winImageNotFound();
	  imageNotFound(shortImageName);
	}
      {
	int fd= open(imageName, O_RDONLY);
	if (fd < 0) abort();
#      ifdef DEBUG_IMAGE
	dprintf(9, "fstat(%d) => %d\n", fd, fstat(fd, &sb));
#      endif
      }
      recordFullPathForImageName(shortImageName); /* full image path */
      dprintf(9, "extraMemory: %d\n", extraMemory);
      if (extraMemory)
	useMmap= 0;
      else
	extraMemory= DefaultHeapSize * 1024 *1024;
#    ifdef DEBUG_IMAGE
      dprintf(9, "image size %d + heap size %d (useMmap = %d)\n", 
		      (int)sb.st_size, extraMemory, useMmap);
#    endif
      extraMemory += (int)sb.st_size;
      readImageFromFileHeapSizeStartingAt(f, extraMemory, 0);
      sqImageFileClose(f);
      break;
    }
}


#if defined(__GNUC__) && ( defined(i386) || defined(__i386) || defined(__i386__)  \
			|| defined(i486) || defined(__i486) || defined (__i486__) \
			|| defined(intel) || defined(x86) || defined(i86pc) )
  static void fldcw(unsigned int cw)
  {
    __asm__("fldcw %0" :: "m"(cw));
  }
#else
# define fldcw(cw)
#endif

#if defined(__GNUC__) && ( defined(ppc) || defined(__ppc) || defined(__ppc__)  \
			|| defined(POWERPC) || defined(__POWERPC) || defined (__POWERPC__) )
  void mtfsfi(unsigned long long fpscr)
  {
    __asm__("lfd   f0, %0" :: "m"(fpscr));
    __asm__("mtfsf 0xff, f0");
  }
#else
# define mtfsfi(fpscr)
#endif

static int nohrtbit;

int interp_init(int argc, char **argv, char **envp)
{
  fldcw(0x12bf);	/* signed infinity, round to nearest, REAL8, disable intrs, disable signals */
  mtfsfi(0);		/* disable signals, IEEE mode, round to nearest */

  bigEndian = isBigEndian();
  nohrtbit = 1;
  dprintf(7, "bigEndian = %d\n", bigEndian);

  /* Make parameters global for access from plugins */

  argCnt = argc;
  argVec = argv;
  envVec = envp;


#ifdef DEBUG_IMAGE
  {
    int i= argc;
    char **p= argv;
    while (i--)
      printf("arg: %s\n", *p++);
  }
#endif

  /* Allocate arrays to store copies of pointers to command line
     arguments.  Used by getAttributeIntoLength(). */

  if ((vmArgVec= calloc(argc + 1, sizeof(char *))) == 0)
    outOfMemory();

  if ((squeakArgVec= calloc(argc + 1, sizeof(char *))) == 0)
    outOfMemory();


#if defined(HAVE_TZSET)
  tzset();	/* should _not_ be necessary! */
#endif

  recordFullPathForVmName(argv[0]); /* full vm path */
  squeakPlugins= vmPath;		/* default plugin location is VM directory */

dprintf(5, "vmPath: %s\n", vmPath);

#if !DEBUG
  sqIgnorePluginErrors= 1;
#endif
  parseArguments(argc, argv);
  int x;
  for(x = 0; x <= squeakArgCnt; x++) {
      dprintf(9, "sqarg[%d]: %s\n", x, squeakArgVec[x]);
  }
  dpy = display_android.makeInterface();
  snd = sound_null.makeInterface();
#if !DEBUG
  sqIgnorePluginErrors= 0;
#endif

#if defined(DEBUG_MODULES)
  dprintf(9, "displayModule %p %s\n", dpy, displayModule->name);
  if (soundModule)
    dprintf(9, "soundModule   %p %s\n", snd,   soundModule->name);
#endif

  if (!realpath(argv[0], vmName))
    vmName[0]= 0; /* full VM name */

#ifdef DEBUG_IMAGE
  dprintf(9, "vmName: %s -> %s\n", argv[0], vmName);
  dprintf(9, "viName: %s\n", shortImageName);
  dprintf(9, "documentName: %s\n", documentName);
#endif

dprintf(5, "about to init time\n");

  ioInitTime();
  aioInit();
  dpy->winInit();
dprintf(5, "about to init image\n");
  imgInit();
dprintf(5, "about to open window\n");
  /* If running as a single instance and there are arguments after the image
   * and any are files then try and drop these on the existing instance.
   */
  dpy->winOpen(runAsSingleInstance ? squeakArgCnt : 0, squeakArgVec);
  ioSetMaxExtSemTableSize(64);
  return 0;
}

/*
 * Gather some interpreter stats here.
 */

static int hostcnt = 0, interpcnt = 0, alarmcnt = 0, gt1dot5 = 0, gt2dot0 = 0, gt2dot5 = 0;
static long interpmsecs = 0, interpmax = 0, interpmin = 0xFFFFFFFFL, lastexit = 0;
static long hostmsecs = 0, hostmax = 0, hostmin = 0xFFFFFFFFL;

/*
 * Run the interpreter, heartbeat before. Give it ALARM_MS msec or less to run.
 * ALARM_MS is defined in sqPlatformSpecific.h
 */

int interp_run() {
    if (nohrtbit) nohrtbit = 0;
    else heartbeat();
    alarmed = 0;
    struct itimerval tval, oval;
    tval.it_interval = (struct timeval) {.tv_sec = 0, .tv_usec = 0};
    tval.it_value = (struct timeval){.tv_sec = ALARM_MS / 1000, .tv_usec = ALARM_MS * 1000};

    setitimer(ITIMER_REAL, &tval, &oval);

    long t1 = (ioUTCMicroseconds() / 1000LL);
    if(lastexit != 0) {
      long hosttm = t1 - lastexit;
      hostcnt ++;
      hostmsecs += hosttm;
      if(hosttm > hostmax) hostmax = hosttm;
      if(hosttm < hostmin) hostmin = hosttm;
    }
	
    interpcnt++;
	setjmp(jmpBufExit);
	
	interpret();
	
    long t2 = (ioUTCMicroseconds() / 1000LL);	
    long interptm = t2 - t1;
    lastexit = t2;
    tval.it_value = (struct timeval){.tv_sec = 0, .tv_usec = 0};
    tval.it_interval = (struct timeval) {.tv_sec = 0, .tv_usec = 0};
    setitimer(ITIMER_REAL, &tval, &oval);
    int rc = alarmed;
    if(rc) alarmcnt++;
    interpmsecs += interptm;
    if(interptm > interpmax) interpmax = interptm;
    if(interptm < interpmin) interpmin = interptm;
    if(100 * interptm / ALARM_MS > 150) gt1dot5++;
    if(100 * interptm / ALARM_MS > 200) gt2dot0++;
    if(100 * interptm / ALARM_MS > 250) gt2dot5++;
    alarmed = 0;
    return rc;
}

void interpStats(void)
{
  dprintf(1, "Interpreter entered: %d times\n", interpcnt);
  dprintf(1, "Alarm timer (T), msec: %d\n", ALARM_MS);
  dprintf(1, "Exited by alarm: %d times\n", alarmcnt);
  //dprintf(1, "Interpreter times, msec (min avg max): %ld, %ld, %ld\n", 
  //  interpmin, interpmsecs / interpcnt, interpmax);
  //dprintf(1, "Longer than 1.5 * T: %d times\n", gt1dot5);
  //dprintf(1, "Longer than 2.0 * T: %d times\n", gt2dot0);
  //dprintf(1, "Longer than 2.5 * T: %d times\n", gt2dot5);
  //dprintf(1, "Host times, msec (min avg max): %ld, %ld, %ld\n", 
  // hostmin, hostmsecs / hostcnt, hostmax);
}

int ioExit(void) { return ioExitWithErrorCode(0); }

sqInt
ioExitWithErrorCode(int ec)
{
  (void)sq2uxPath;
  (void)ux2sqPath;
  interpStats();
  sqDebugAnchor();
  dpy->winExit();
  return ec;
}


/* Copy aFilenameString to aCharBuffer and optionally resolveAlias (or
   symbolic link) to the real path of the target.  Answer 0 if
   successful of -1 to indicate an error.  Assume aCharBuffer is at
   least PATH_MAX bytes long.  Note that MAXSYMLINKS is a lower bound
   on the (potentially unlimited) number of symlinks allowed in a
   path, but calling sysconf() seems like overkill. */

sqInt sqGetFilenameFromString(char *aCharBuffer, char *aFilenameString, sqInt filenameLength, sqInt resolveAlias)
{
  int numLinks= 0;
  struct stat st;

  memcpy(aCharBuffer, aFilenameString, filenameLength);
  aCharBuffer[filenameLength]= 0;

  if (resolveAlias)
    for (;;)	/* aCharBuffer might refer to link or alias */
      {
	if (!lstat(aCharBuffer, &st) && S_ISLNK(st.st_mode))	/* symlink */
	  { char linkbuf[PATH_MAX+1];
	    if (++numLinks > MAXSYMLINKS)
	      return -1;	/* too many levels of indirection */

	    filenameLength= readlink(aCharBuffer, linkbuf, PATH_MAX);
	    if ((filenameLength < 0) || (filenameLength >= PATH_MAX))
	      return -1;	/* link unavailable or path too long */

	    linkbuf[filenameLength]= 0;

	    if (filenameLength > 0 && *linkbuf == '/') /* absolute */
	      strcpy(aCharBuffer, linkbuf);
	    else {
	      char *lastSeparator = strrchr(aCharBuffer,'/');
	      char *append = lastSeparator ? lastSeparator + 1 : aCharBuffer;
	      if (append - aCharBuffer + strlen(linkbuf) > PATH_MAX)
		return -1; /* path too long */
	      strcpy(append,linkbuf);
	    }

	    continue;
	  }

#    if defined(DARWIN)
	if (isMacAlias(aCharBuffer))
	  {
	    if ((++numLinks > MAXSYMLINKS) || !resolveMacAlias(aCharBuffer, aCharBuffer, PATH_MAX))
	      return -1;		/* too many levels or bad alias */
	    continue;
	  }
#    endif

	break;			/* target is no longer a symlink or alias */
      }

  return 0;
}


sqInt ioGatherEntropy(char *buffer, sqInt bufSize)
{
  int fd, count= 0;

  if ((fd= open("/dev/urandom", O_RDONLY)) < 0)
    return 0;

  while (count < bufSize)
    {
      int n;
      if ((n= read(fd, buffer + count, bufSize)) < 1)
	break;
      count += n;
    }

  close(fd);

  return count == bufSize;
}

/*
 * Memory fence: in a single-threaded program it is NOP.
 */

void sqLowLevelMFence() 
{
}

/*
 * In a single-threaded program, crashInThisOrAnotherThread is a NOP always returning 0
 */

sqInt
crashInThisOrAnotherThread(sqInt inThisThread)
{
    return 0;
}

/*
 * In a single-threaded program we are always in the VM thread.
 */

sqInt
amInVMThread()
{
    return 1;
}

#if COGVM
/*
 * Support code for Cog.
 * a) Answer whether the C frame pointer is in use, for capture of the C stack
 *    pointers.
 */
# if defined(i386) || defined(__i386) || defined(__i386__)
/*
 * Cog has already captured CStackPointer  before calling this routine.  Record
 * the original value, capture the pointers again and determine if CFramePointer
 * lies between the two stack pointers and hence is likely in use.  This is
 * necessary since optimizing C compilers for x86 may use %ebp as a general-
 * purpose register, in which case it must not be captured.
 */
int
isCFramePointerInUse()
{
	extern unsigned long CStackPointer, CFramePointer;
	extern void (*ceCaptureCStackPointers)(void);
	unsigned long currentCSP = CStackPointer;

	currentCSP = CStackPointer;
	ceCaptureCStackPointers();
	assert(CStackPointer < currentCSP);
	return CFramePointer >= CStackPointer && CFramePointer <= currentCSP;
}
# endif /* defined(i386) || defined(__i386) || defined(__i386__) */
#endif /* COGVM */

/*	Answer true (non-zero) if running on a big endian machine. */

sqInt isBigEndian(void) {
    char * cString;
    sqInt i;
    sqInt anInt;
    static sqInt endianness = -1;
    sqInt len;

	if (!(endianness == -1)) {
		return endianness;
	}
	len = sizeof(anInt);
	cString = (char *) &anInt;
	i = 0;
	while (i < len) {
		cString[i] = i;
		i += 1;
	}
	endianness = anInt & 255;
	return endianness;
}


