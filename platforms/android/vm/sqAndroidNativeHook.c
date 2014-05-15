/*
 * Initialize the VM here. In order to do this, call interp_init (formerly main) with
 * the zeroth argument pointing to the image plus some fake executable name. This will
 * give the VM an idea where the image is.
 */
#include <jni.h>

#define MAXPATHLEN 256
#define NULL  (void*)0

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
  int rc = libMain(argl - 1, argc, envp);
  (*env)->ReleaseStringUTFChars(env, imageName_, imgpath);
  (*env)->ReleaseStringUTFChars(env, cmd_, cmd);
  jclass cls = (*env)->GetObjectClass(env, self);
  //sqInvalidate = (*env)->GetMethodID(env, cls, "invalidate", "(IIII)V");
  return rc;
}

int
Java_org_pharo_stack_StackVM_setLogLevel(JNIEnv *env, jobject self, 
					  int logLevel) {

}


void
Java_org_pharo_stack_StackVM_surelyExit(JNIEnv *env, jobject self) {
  exit(0);
}

int
Java_org_pharo_stack_StackVM_setScreenSize(JNIEnv *env, jobject self,
					       int w, int h) {

}

int 
Java_org_pharo_stack_StackVM_interpret(JNIEnv *env, jobject jsqueak) {
//  JNIEnv *oldEnv = CogEnv;
 // jobject *oldCog = CogVM;
  
  
  //CogEnv = env;
  //CogVM = jsqueak;
  //int rc = interp_run();
  interpret();
  //CogEnv = oldEnv;
  //CogVM = oldCog;
  return 1;
}

int 
Java_org_pharo_stack_StackVM_updateDisplay(JNIEnv *env, jobject self,
					       jintArray bits, int w, int h,
					       int d, int left, int top, int right, int bottom) {
}
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