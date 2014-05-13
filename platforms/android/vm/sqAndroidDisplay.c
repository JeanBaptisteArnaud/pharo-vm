/* sqUnixCustomWindow.c -- support for display via your custom window system.
 * 
 * Last edited: 2006-04-17 16:57:12 by piumarta on margaux.local
 * 
 * This is a template for creating your own window drivers for Cog:
 * 
 *   - copy the entire contents of this directory to some other name
 *   - rename this file to be something more appropriate
 *   - modify acinclude.m4, Makefile.in, and ../vm/sqUnixMain accordingly
 *   - implement all the stubs in this file that currently do nothing
 * 
 */

#include "sq.h"
#include "sqMemoryAccess.h"

#include "sqAndroidMain.h"
#include "sqAndroidGlobals.h"
#include "sqAndroidCharConv.h"		/* not required, but probably useful */

#include "SqDisplay.h"
#include "sqaio.h"

#include <stdio.h>

int dprintf(int logLvl, const char *fmt, ...);

#include "sqAndroidEvents.c"		/* see X11 and/or Quartz drivers for examples */

#include <jni.h>
#include <android/log.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define trace() fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__)

#define trace()

extern struct VirtualMachine *interpreterProxy;

/* Static references to callback instances during interpret() */
JNIEnv *CogEnv = NULL;
jobject CogVM = NULL;
static jmethodID sqInvalidate = NULL;
static jmethodID sqSpeak = NULL;
static jmethodID sqSetPitch = NULL;
static jmethodID sqSetSpeechRate = NULL;
static jmethodID sqStop = NULL;

static int scrw = 0, scrh = 0;

static unsigned char *sqMemory = NULL;
static int sqHeaderSize = 0;

#define LOG_FILE "/sdcard/jni.log"

void jnilog(char *str) {
	int fd = open(LOG_FILE, O_RDWR | O_APPEND | O_CREAT, 0666);
	if(fd > -1) {
		int ms = ioMSecs();
		char msstr[50];
		snprintf(msstr, 49, "[%d] ", ms);
		write(fd, msstr, strlen(msstr));
		write(fd, str, strlen(str));
		close(fd);
	}
}

#define LOGLEN 2048

static char logbuf[LOGLEN];
static int loglen = 0;


/*
 * jniputstr: put a string, buffered. Once buffer is full, jnilog the buffer.
 */

void jprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char str[10000];
  vsnprintf(str, 9999, fmt, args);
  va_end(args);
  jniputstr(str);
}

/* Higher values introduce more logging:
   1 - CRITICAL failures (out of memory etc)
   3 - ERRORS (opening resources etc)
   4 - WARNINGS (only one-time warnings)
   5 - NOTIFICATIONS (one-time)
   7 - REPEAT NOTIFICATIONS
   9 - TRACING
   10 - ADDED FOR printOop and friends
 */
static int vmLogLevel = 5;
int dprintf(int logLvl, const char *fmt, ...) {
  int result;
  va_list args;
  va_start(args, fmt);
  if(logLvl <= vmLogLevel) {
	result = __android_log_vprint(ANDROID_LOG_INFO, "PharoVM", fmt, args);
	char str[10000];
	vsnprintf(str, 9999, fmt, args);
	jnilog(str);
  }
  va_end(args);
  return result;
}

int sdprintf(int logLvl, const char *fmt, ...) {
  int result;
  va_list args;
  va_start(args, fmt);
  if(logLvl <= vmLogLevel) {
	result = __android_log_vprint(ANDROID_LOG_INFO, "Smalltalk", fmt, args);
	char str[10000];
	vsnprintf(str, 9999, fmt, args);
	jnilog(str);
  }
  va_end(args);
  return result;
}


void jniputstr(char *s) {
  	sdprintf(9, "%s", s);
}

void jniputlong(long i) {
  	sdprintf(9, "%ld", (long)i);
}

void jniputchar(char s) {
  	sdprintf(9, "here is a Char: %c", s);
}


/*
 * Find a method with given name, signature, class.
 * Uses the saved environment.
 */

jmethodID getMethodwithSiginClass(char *mtdname, char *mtdsig, jclass cls)
{
    jmethodID meth;
    if(CogEnv == NULL) return NULL;
    meth = (*CogEnv)->GetMethodID(CogEnv, cls, mtdname, mtdsig);
    return meth;
}

/*
 * Get Java class of the virtual machine instance.
 * Uses the saved environment.
 */

jclass getVMClass(void)
{
    jclass cls;
    if(CogEnv == NULL) return NULL;
    cls = (*CogEnv)->GetObjectClass(CogEnv, CogVM);
    return cls;
}

/*
 * Get Java object of the virtual machine instance.
 * Uses the saved environment.
 */

jobject getVMObject(void)
{
    return CogVM;
}

/*
 * Allocate a new Java string object given a C string.
 * Uses the saved environment.
 */

jstring asJavaString(char *bytes)
{
    jstring jstr;
    if(CogEnv == NULL) return NULL;
    jstr = (*CogEnv)->NewStringUTF(CogEnv, bytes);
}

/*
 * Allocate a new Java byte array object given the pointer and the size.
 * Uses the saved environment.
 */

jbyteArray asJavaByteArray(void *arr, jsize length)
{
    jbyteArray jarr;
    if(CogEnv == NULL) return NULL;
    jarr = (*CogEnv)->NewByteArray(CogEnv, length);
    if(jarr == NULL) return NULL;
    (*CogEnv)->SetByteArrayRegion(CogEnv, jarr, 0, length, arr);
    return jarr;
}

/*
 * Invoke a void method on an object with an argument.
 * Uses saved environment.
 */

void callVoidMethodOnwith(jmethodID meth, jobject obj, ...)
{
    va_list args;
    va_start(args, meth);
    if(CogEnv == NULL) return;
    (*CogEnv)->CallVoidMethodV(CogEnv, obj, meth, args);
    va_end(args);
}

/*
 * Invoke a int method on an object with an argument.
 * Uses saved environment.
 */

int callIntMethodOnwith(jmethodID meth, jobject obj, ...)
{
    va_list args;
    va_start(args, meth);
    int res;
    if(CogEnv == NULL) return -1;
    res = (*CogEnv)->CallIntMethodV(CogEnv, obj, meth, args);
    va_end(args);
    return res;
}

/*
 * Invoke a method returning a String on an object with an argument.
 * In fact, this calls a method returning any object, but only String
 * is expected. The returned Java string will be strdup'd so it has to
 * be freed afterwards. 
 * See http://toastedtoothpaste.blogspot.com/2010/10/jni-wheres-env-callstingmethod.html
 * Uses saved environment.
 */

char *callStringMethodOnwith(jmethodID meth, jobject obj, ...)
{
    va_list(args);
    va_start(args, meth);
    if(CogEnv == NULL) return -1;
    jstring jstr = (jstring) (*CogEnv)->CallObjectMethodV(CogEnv, obj, meth, args);
    char *res = (*CogEnv)->GetStringUTFChars(CogEnv, jstr, 0);
    char *ress = strdup(res);
    (*CogEnv)->ReleaseStringUTFChars(CogEnv, jstr, res);
    return ress;
}

/****************************************************************************/
/* JNI entry points                                                         */
/****************************************************************************/

/* force exit at the process level to make sure that the native library unloads */

void
Java_org_pharo_stack_StackVM_surelyExit(JNIEnv *env, jobject self) {
  dprintf(9, "exiting for sure\n");
  exit(0);
}

int
Java_org_pharo_stack_StackVM_setScreenSize(JNIEnv *env, jobject self,
					       int w, int h) {
  scrw = w;
  scrh = h;
  dprintf(9, "setScreenSize w: %d, h: %d\n", scrw, scrh);
  return 0;
}

int 
Java_org_pharo_stack_StackVM_interpret(JNIEnv *env, jobject jsqueak) {
  JNIEnv *oldEnv = CogEnv;
  jobject *oldCog = CogVM;
  CogEnv = env;
  CogVM = jsqueak;
  int rc = interp_run();
  CogEnv = oldEnv;
  CogVM = oldCog;
  return rc;
}

int 
Java_org_pharo_stack_StackVM_updateDisplay(JNIEnv *env, jobject self,
					       jintArray bits, int w, int h,
					       int d, int left, int top, int right, int bottom) {
  int row;
  sqInt formObj = interpreterProxy->displayObject();
  sqInt formBits = interpreterProxy->fetchPointerofObject(0, formObj);
  sqInt width = interpreterProxy->fetchIntegerofObject(1, formObj);
  sqInt height = interpreterProxy->fetchIntegerofObject(2, formObj);
  sqInt depth = interpreterProxy->fetchIntegerofObject(3, formObj);
  int *dispBits = interpreterProxy->firstIndexableField(formBits);

  if (width == 777 && height == 777) return 1;

  if(depth != 32) {
    dprintf(4, "updateDisplay: Display depth %d\n", depth);
    return 0;
  }
  if(width != w) {
    dprintf(4, "updateDisplay: Display width is %d (expected %d)\n", width, w);
  }
  if(height != h) {
    dprintf(4, "updateDisplay: Display height is %d (expected %d)\n", height, h);
  }
  for(row = top; row < bottom; row++) {
  	int ofs = width*row+left;
  	(*env)->SetIntArrayRegion(env, bits, ofs, right-left, dispBits+ofs);
  }
  return 1;
}

/*
 * For type 2, record a key press event, for type 1, record a mouse event.
 * Return 1 if input queue was empty prior to buffering this event.
 */

int 
Java_org_pharo_stack_StackVM_sendEvent(JNIEnv *env, jobject self, int 
					   type, int stamp,
					   int arg3, int arg4, int arg5,
					   int arg6, int arg7, int arg8) {

	dprintf(7, "sendEvent type=%d\n", type);
    int empty = iebEmptyP();
    switch(type) {
	case 1:				/* mouse/touch event, arg3=x, arg4=y, arg5=buttons */
	    mousePosition.x = arg3;
	    mousePosition.y = arg4;
            buttonState = arg5;
	    recordMouseEvent();
	dprintf(7, "mouse x=%d y=%d %d\n", arg3, arg4, arg5);
	    break;
	case 2:				/* keyboard input, arg3=charCode, arg5=mods, arg6=ucs4 */
	    recordKeyboardEvent(arg3, arg4, arg5, arg6);
	    break;
	default:
	    break;
    }
    return empty;
}

/*
 * Split the given command line string by whitespace unless quoted into tokens,
 * filling the array provided. Return the number of actual arguments extracted.
 */

static int splitcmd(char *cmd, int maxargc, char **argv) {
	char *argbuf = alloca(strlen(cmd) + 1);
	memset(argbuf, 0, strlen(cmd) + 1);
	int argc = 0;
	int inquote = 0, inarg = 0, inesc = 0;
	int argidx = 0;
	char *cptr;
	for(cptr = cmd; ; cptr++) {
		char c = *cptr;
		if(!c) break;
		if(argc >= maxargc) return argc;
		if(inesc) {
			argbuf[argidx++] = c;
			inesc = 0;
			continue;
		}
		if(c == '\\' && !inesc) {
			inesc = 1;
			continue;
		}
		if(c == '\"') {
			inquote = ~inquote;
			continue;
		}
                if(inquote) {
			argbuf[argidx++] = c;
			continue;
		}
		if((c == ' ' || c == '\t') && !inquote) {
			if(!strlen(argbuf)) continue;
			else {
				argv[argc] = strdup(argbuf);
				argc++;
				memset(argbuf, 0, strlen(cmd) + 1);
				argidx = 0;
			}
			continue;
		}
		argbuf[argidx++] = c;
	}
	if(strlen(argbuf)) {
		argv[argc] = strdup(argbuf);
		argc++;
	}
	return argc;
}

/*
 * Initialize the VM here. In order to do this, call interp_init (formerly main) with
 * the zeroth argument pointing to the image plus some fake executable name. This will
 * give the VM an idea where the image is.
 */

int 
Java_org_pharo_stack_StackVM_setImagePath(JNIEnv *env, jobject self,
					      jstring imageName_, jstring cmd_) {
  const char *imgpath = (*env)->GetStringUTFChars(env, imageName_, 0);
  const char *cmd = (*env)->GetStringUTFChars(env, cmd_, 0);
  char *imageName = alloca(MAXPATHLEN + 1);
  char *cmdd = strdup(cmd);
  char *fakeExe = alloca(MAXPATHLEN + 1);
  if(strlen(imgpath) <= MAXPATHLEN)
    strcpy(imageName, imgpath);
  else
    return -1;
  char *dir = dirname(imgpath);
  chdir(dir);
  strcpy(fakeExe, dir);
  strcat(fakeExe, "/");
  strcat(fakeExe, "pharos");
  int maximgarg = 128;
  char *imgargv[maximgarg];
  int imgargc = splitcmd(cmdd, maximgarg, imgargv);
int z;
//for(z = 0; z < imgargc; z++)
//	dprintf(9, "split [%d]: %s\n", z, imgargv[z]);
  char *baseargs[] = {fakeExe, imageName};
  int argl = 2 + imgargc + 1;
  char **argc = alloca(sizeof(char *) * argl);
  int i, j;
  for(i = 0; i < 2; i ++) argc[i] = baseargs[i];
  for(j = 0; j < imgargc; j++, i++) argc[i] = imgargv[j];
  argc[i] = NULL;
  char *envp[] = {NULL};
//for(z = 0; z < argl; z++)
//	dprintf(9, "argc [%d]: %s\n", z, argc[z]);
  int rc = interp_init(argl - 1, argc, envp);
  (*env)->ReleaseStringUTFChars(env, imageName_, imgpath);
  (*env)->ReleaseStringUTFChars(env, cmd_, cmd);
  jclass cls = (*env)->GetObjectClass(env, self);
  sqInvalidate = (*env)->GetMethodID(env, cls, "invalidate", "(IIII)V");
  return rc;
}

int
Java_org_pharo_stack_StackVM_setLogLevel(JNIEnv *env, jobject self, 
					  int logLevel) {
  unlink(LOG_FILE);
  vmLogLevel = logLevel;
  memset(logbuf, 0, LOGLEN);
  loglen = 0;
  return vmLogLevel;
}



/****************************************************************************/
/* Display control                                                          */   
/****************************************************************************/


static int handleEvents(void)
{
  printf("handle custom events here...\n");
  return 0;	/* 1 if events processed */
}

static sqInt display_clipboardSize(void)
{
  trace();
  return 0;
}

static sqInt display_clipboardWriteFromAt(sqInt count, sqInt byteArrayIndex, sqInt startIndex)
{
  trace();
  return 0;
}

static sqInt display_clipboardReadIntoAt(sqInt count, sqInt byteArrayIndex, sqInt startIndex)
{
  trace();
  return 0;
}


static sqInt display_ioFormPrint(sqInt bitsIndex, sqInt width, sqInt height, sqInt depth, double hScale, double vScale, sqInt landscapeFlag)
{
  trace();
  return false;
}

static sqInt display_ioBeep(void)
{
  trace();
  return 0;
}

static sqInt display_ioRelinquishProcessorForMicroseconds(sqInt microSeconds)
{
  return 0;
}

/* Poll asynchronous IO requests. Return 1 if the GUI events buffer is not empty */

static sqInt display_ioProcessEvents(void)
{
  aioPoll(0);
  return !(iebEmptyP());
}

static sqInt display_ioScreenDepth(void)
{
  trace();
  return 32;
}

static sqInt display_ioScreenSize(void)
{
  trace();
  int actw = scrw?scrw:777;
  int acth = scrh?scrh:777;
  return (actw << 16) | (acth);
}

static sqInt display_ioSetCursorWithMask(sqInt cursorBitsIndex, sqInt cursorMaskIndex, sqInt offsetX, sqInt offsetY)
{
  trace();
  return 0;
}

static sqInt display_ioSetFullScreen(sqInt fullScreen)
{
  trace();
  return 0;
}

static sqInt display_ioForceDisplayUpdate(void)
{
  trace();
  return 0;
}

static sqInt display_ioShowDisplay(sqInt dispBitsIndex, sqInt width, sqInt height, sqInt depth,
				   sqInt affectedL, sqInt affectedR, sqInt affectedT, sqInt affectedB)
{
  if(sqInvalidate && CogEnv && CogVM) {
  	(*CogEnv)->CallVoidMethod(CogEnv, CogVM, sqInvalidate, 
			affectedL, affectedT, affectedR, affectedB);
  }
  return 1;
}

static sqInt display_ioHasDisplayDepth(sqInt i)
{
  trace();
  return 32 == i;
}

static sqInt display_ioSetDisplayMode(sqInt width, sqInt height, sqInt depth, sqInt fullscreenFlag)
{
  trace();
  return 0;
}

static void display_winSetName(char *imageName)
{
  trace();
}

static void *display_ioGetDisplay(void)	{ return 0; }
static void *display_ioGetWindow(void)	{ return 0; }

static sqInt display_ioGLinitialise(void) { trace();  return 0; }
static sqInt display_ioGLcreateRenderer(glRenderer *r, sqInt x, sqInt y, sqInt w, sqInt h, sqInt flags) { trace();  return 0; }
static void  display_ioGLdestroyRenderer(glRenderer *r) { trace(); }
static void  display_ioGLswapBuffers(glRenderer *r) { trace(); }
static sqInt display_ioGLmakeCurrentRenderer(glRenderer *r) { 
	dprintf(5, "In makeCurrent\n");
	trace();  return 0; }
static void  display_ioGLsetBufferRect(glRenderer *r, sqInt x, sqInt y, sqInt w, sqInt h) { trace(); }

static char *display_winSystemName(void)
{
  trace();
  return "android";
}

static void display_winInit(void)
{
  trace();
  printf("Initialise your Custom Window system here\n");
}

static void display_winOpen(void)
{
  trace();
  printf("map your Custom Window here\n");
}

/* Call the VM to exit current activity */

static void display_winExit(void)
{
    jmethodID meth;
    jclass cls;
    if (CogEnv == NULL || CogVM == NULL) return;
    cls = (*CogEnv)->GetObjectClass(CogEnv, CogVM);
    if (cls == NULL) return;
    meth = (*CogEnv)->GetMethodID(CogEnv, cls, "finish", "()V");
    if (meth == NULL) return;
    (*CogEnv)->CallVoidMethod(CogEnv, CogVM, meth);
    dprintf(9, "called VM finish method\n");
}

static int  display_winImageFind(char *buf, int len)		{ trace();  return 0; }
static void display_winImageNotFound(void)			{ trace(); }

static sqInt display_primitivePluginBrowserReady(void)		{ return primitiveFail(); }
static sqInt display_primitivePluginRequestURLStream(void)	{ return primitiveFail(); }
static sqInt display_primitivePluginRequestURL(void)		{ return primitiveFail(); }
static sqInt display_primitivePluginPostURL(void)		{ return primitiveFail(); }
static sqInt display_primitivePluginRequestFileHandle(void)	{ return primitiveFail(); }
static sqInt display_primitivePluginDestroyRequest(void)	{ return primitiveFail(); }
static sqInt display_primitivePluginRequestState(void)		{ return primitiveFail(); }

/*
 * Functions that need to be defined but not necessarily implemented
 */

static sqInt display_ioSetCursorARGB(sqInt cursorBitsIndex, 
		sqInt extentX, sqInt extentY, 
		sqInt offsetX, sqInt offsetY) { return 0; }

static char **display_clipboardGetTypeNames(void) { return NULL; }

static sqInt display_clipboardSizeWithType(char *typeName, int nTypeName) { return 0; }

static void display_clipboardWriteWithType(char *data, size_t ndata, 
		char *typeName, size_t nTypeName, 
		int isDnd, int isClaiming) { return; }

static int display_hostWindowCreate(int w, int h, int x, 
		int y, char *list, int attributeListLength) { return 0; }

static int display_hostWindowClose(int index) { return 0; }

static int display_hostWindowCloseAll(void) { return 0; }

static int display_hostWindowShowDisplay(unsigned *dispBitsIndex, int width, 
		int height, int depth, int affectedL, int affectedR, 
		int affectedT, int affectedB, int windowIndex) { return 0; }

static int display_hostWindowGetSize(int windowIndex) { return display_ioScreenSize; }

static int display_hostWindowSetSize(int windowIndex, int w, int h) { return -1; }

static int display_hostWindowGetPosition(int windowIndex) { return -1; }

static int display_hostWindowSetPosition(int windowIndex, int x, int y) { return -1; }

static int display_ioSizeOfNativeWindow(void *windowHandle) { return -1; }

static int display_ioPositionOfNativeWindow(void *windowHandle) { return -1; }

static int display_hostWindowSetTitle(int windowIndex, 
		char *newTitle, int sizeOfTitle) { return -1; }

static int display_ioPositionOfScreenWorkArea(int windowIndex) { return 0; }

static int display_ioSizeOfScreenWorkArea(int windowIndex) { return 0; }

void *display_ioGetWindowHandle() { return NULL; }

static sqInt display_ioSetCursorPositionXY(sqInt x, sqInt y) { return 0; }

static int display_ioPositionOfNativeDisplay(void *windowHandle) { return -1; }

static int display_ioSizeOfNativeDisplay(void *windowHandle) { return -1; }

static sqInt display_dndOutStart(char *types, int ntypes)	{ return 0; }

static void  display_dndOutSend (char *bytes, int nbytes)	{ return  ; }

static sqInt display_dndOutAcceptedType(char *buf, int nbuf)	{ return 0; }

static sqInt display_dndReceived(char *fileName)	{ return 0; }


SqDisplayDefine(android);	/* name must match that in makeInterface() below */


/*** module ***/


static void display_printUsage(void)
{
  printf("\nCustom Window <option>s: (none)\n");
  /* otherwise... */
}

static void display_printUsageNotes(void)
{
  trace();
}

static void display_parseEnvironment(void)
{
  trace();
}

static int display_parseArgument(int argc, char **argv)
{
  return 0;	/* arg not recognised */
}

static void *display_makeInterface(void)
{
  return &display_android_itf;		/* name must match that in SqDisplayDefine() above */
}

#include "SqModule.h"

SqModuleDefine(display, android);	/* name must match that in sqUnixMain.c's moduleDescriptions */