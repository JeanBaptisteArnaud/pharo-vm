/* aio.c -- asynchronous file i/o
 * 
 *   Copyright (C) 1996-2006 by Ian Piumarta and other authors/contributors
 *                              listed elsewhere in this file.
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

/* Author: Ian.Piumarta@squeakland.org
 * 
 * Last edited: 2006-04-23 12:55:59 by piumarta on emilia.local
 */

#include "sqaio.h"

#ifdef HAVE_CONFIG_H

# include "config.h"

# ifdef HAVE_UNISTD_H
#   include <sys/types.h>
#   include <unistd.h>
# endif /* HAVE_UNISTD_H */
  
# ifdef NEED_GETHOSTNAME_P
    extern int gethostname();
# endif
  
# include <stdio.h>
# include <signal.h>
# include <errno.h>
# include <fcntl.h>
# include <sys/ioctl.h>
  
# ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
# else
#   include <time.h>
# endif
  
# ifdef HAS_SYS_SELECT_H
#   include <sys/select.h>
# endif
  
# ifndef FIONBIO
#   ifdef HAVE_SYS_FILIO_H
#     include <sys/filio.h>
#   endif
#   ifndef FIONBIO
#     ifdef FIOSNBIO
#       define FIONBIO FIOSNBIO
#     else
#       error: FIONBIO is not defined
#     endif
#   endif
# endif

#else /* !HAVE_CONFIG_H -- assume lowest common demoninator */

# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/select.h>
# include <sys/ioctl.h>
# include <fcntl.h>

#endif

//#define DEBUG
#undef	DEBUG

#if defined(DEBUG)
  int aioLastTick= 0;
  int aioThisTick= 0;
# define FPRINTF(X) { aioThisTick= ioMSecs();  fprintf(stderr, "%8d %8d ", aioThisTick, aioThisTick - aioLastTick);  aioLastTick= aioThisTick;  fprintf X; }
#else
# define FPRINTF(X)
#endif

#define _DO_FLAG_TYPE()	do { _DO(AIO_R, rd) _DO(AIO_W, wr) _DO(AIO_X, ex) } while (0)

#define perror(x) fprintf(3, "#x: %s\n", strerror(errno))

static int one= 1;

static aioHandler  rdHandler[FD_SETSIZE];
static aioHandler  wrHandler[FD_SETSIZE];
static aioHandler  exHandler[FD_SETSIZE];

static void       *clientData[FD_SETSIZE];

static int	maxFd;
static fd_set	fdMask;	/* handled by aio	*/
static fd_set	rdMask; /* handle read		*/
static fd_set	wrMask; /* handle write		*/
static fd_set	exMask; /* handle exception	*/
static fd_set	xdMask; /* external descriptor	*/


static void undefinedHandler(int fd, void *clientData, int flags)
{
  fprintf(stderr, "undefined handler called (fd %d, flags %x)\n", fd, flags);
}


static char *handlerName(aioHandler h)
{
  if (h == undefinedHandler) return "undefinedHandler";

 {
   extern char *socketHandlerName(aioHandler);
   return socketHandlerName(h);
 }

 return "***unknown***";
}


/* handle SIGIO here */

void handlesigio(int sig) 
{
}

/* handle SIGALRM here */

int alarmed = 0;

void handlealarm(int sig) 
{
  alarmed = 1;
}

/* initialise asynchronous i/o */

void aioInit(void)
{
  FD_ZERO(&fdMask);
  FD_ZERO(&rdMask);
  FD_ZERO(&wrMask);
  FD_ZERO(&exMask);
  FD_ZERO(&xdMask);
  maxFd= 0;
  signal(SIGPIPE, SIG_IGN);
  signal(SIGIO, handlesigio);
  signal(SIGALRM, handlealarm);
}


/* disable handlers and close all handled non-exteral descriptors */

void aioFini(void)
{
  int fd;
  for (fd= 0;  fd < maxFd;  fd++)
    if (FD_ISSET(fd, &fdMask) && !(FD_ISSET(fd, &xdMask)))
      {
	aioDisable(fd);
	close(fd);
	FD_CLR(fd, &fdMask);
	FD_CLR(fd, &rdMask);
	FD_CLR(fd, &wrMask);
	FD_CLR(fd, &exMask);
      }
  while (maxFd && !FD_ISSET(maxFd - 1, &fdMask))
    --maxFd;
  signal(SIGPIPE, SIG_DFL);
}


/* answer whether i/o becomes possible within the given number of microSeconds */
#define max(x,y) (((x)>(y))?(x):(y))

/*
 * Poll pending I/O operations but do not wait if none is ready. The microSeconds
 * argument is left for compatibility, but is not used.
 */

int aioPoll(int microSeconds)
{
  int	 fd;
  fd_set rd, wr, ex;

  if (maxFd == 0)
    return 0;

  rd= rdMask;
  wr= wrMask;
  ex= exMask;

  struct timeval tv;
  int n;
  tv.tv_sec=  0;
  tv.tv_usec= 0;
  n= select(maxFd, &rd, &wr, &ex, &tv);
  if (n == 0) return 0;

  for (fd= 0; fd < maxFd; ++fd)
    {
#     define _DO(FLAG, TYPE)				\
      {							\
	if (FD_ISSET(fd, &TYPE))			\
	  {						\
	    aioHandler handler= TYPE##Handler[fd];	\
	    FD_CLR(fd, &TYPE##Mask);			\
	    TYPE##Handler[fd]= undefinedHandler;	\
	    handler(fd, clientData[fd], FLAG);		\
	  }						\
      }
      _DO_FLAG_TYPE();
#     undef _DO
    }
  return 1;
}


/* sleep for microSeconds or until i/o becomes possible, avoiding
   sleeping in select() if timeout too small */

int aioSleepForUsecs(int microSeconds)
{
#if defined(HAVE_NANOSLEEP)
    if (microSeconds < (1000000/60)) 
    {
        if (!aioPoll(0)) 
        {
            struct timespec rqtp= { 0, microSeconds * 1000 };
            struct timespec rmtp;
            nanosleep(&rqtp, &rmtp);
            microSeconds= 0;			/* poll but don't block */
        }
    }
#endif
    
    return aioPoll(microSeconds);
}


/* enable asynchronous notification for a descriptor */

void aioEnable(int fd, void *data, int flags)
{
  dprintf(9, "aioEnable(%d)\n", fd);
  if (fd < 0)
    {
      dprintf(9, "aioEnable(%d): IGNORED\n", fd);
      return;
    }
  if (FD_ISSET(fd, &fdMask))
    {
      dprintf(4, "aioEnable: descriptor %d already enabled\n", fd);
      return;
    }
  clientData[fd]= data;
  rdHandler[fd]= wrHandler[fd]= exHandler[fd]= undefinedHandler;
  FD_SET(fd, &fdMask);
  FD_CLR(fd, &rdMask);
  FD_CLR(fd, &wrMask);
  FD_CLR(fd, &exMask);
  if (fd >= maxFd)
    maxFd= fd + 1;
  if (flags & AIO_EXT)
    {
      FD_SET(fd, &xdMask);
      /* we should not set NBIO ourselves on external descriptors! */
    }
  else
    {
      /* enable non-blocking asynchronous i/o and delivery of SIGIO to the active process */
      int arg;
      FD_CLR(fd, &xdMask);

#    if defined(O_ASYNC)
      dprintf(9, "using O_ASYNC for aio\n");
      if (      fcntl(fd, F_SETOWN, getpid()                  )  < 0)
		perror("fcntl(F_SETOWN, getpid())");
      if ((arg= fcntl(fd, F_GETFL,  0                         )) < 0)
		perror("fcntl(F_GETFL)");
      if (      fcntl(fd, F_SETFL,  arg | O_NONBLOCK | O_ASYNC)  < 0)
		perror("fcntl(F_SETFL, O_ASYNC)");

#    elif defined(FASYNC)
      dprintf(9, "using FASYNC for aio\n");
      if (      fcntl(fd, F_SETOWN, getpid()                  )  < 0)
		perror("fcntl(F_SETOWN, getpid())");
      if ((arg= fcntl(fd, F_GETFL,  0                         )) < 0)
		perror("fcntl(F_GETFL)");
      if (      fcntl(fd, F_SETFL,  arg | O_NONBLOCK | FASYNC )  < 0)
		perror("fcntl(F_SETFL, FASYNC)");

#    elif defined(FIOASYNC)
      dprintf(9, "using FIOASYNC for aio\n");
      arg= getpid();
	  if (ioctl(fd, SIOCSPGRP, &arg) < 0)
		perror("ioctl(SIOCSPGRP, getpid())");
      arg= 1;
	  if (ioctl(fd, FIOASYNC,  &arg) < 0)
		perror("ioctl(FIOASYNC, 1)");
#    else
      dprintf(2, "AIO not available\n");
#    endif
    }
}


/* install/change the handler for a descriptor */

void aioHandle(int fd, aioHandler handlerFn, int mask)
{
  dprintf(9, "aioHandle(%d, %s, %d)\n", fd, handlerName(handlerFn), mask);
  if (fd < 0)
    {
      dprintf(9, "aioHandle(%d): IGNORED\n", fd);
      return;
    }
# define _DO(FLAG, TYPE)			\
    if (mask & FLAG) {				\
      FD_SET(fd, &TYPE##Mask);			\
      TYPE##Handler[fd]= handlerFn;		\
    }
  _DO_FLAG_TYPE();
# undef _DO
}


/* temporarily suspend asynchronous notification for a descriptor */

void aioSuspend(int fd, int mask)
{
  if (fd < 0)
    {
      FPRINTF((stderr, "aioSuspend(%d): IGNORED\n", fd));
      return;
    }
  FPRINTF((stderr, "aioSuspend(%d)\n", fd));
# define _DO(FLAG, TYPE)			\
  {						\
    if (mask & FLAG)				\
      {						\
	FD_CLR(fd, &TYPE##Mask);		\
	TYPE##Handler[fd]= undefinedHandler;	\
      }						\
  }
  _DO_FLAG_TYPE();
# undef _DO
}


/* definitively disable asynchronous notification for a descriptor */

void aioDisable(int fd)
{
  if (fd < 0)
    {
      FPRINTF((stderr, "aioDisable(%d): IGNORED\n", fd));
      return;
    }
  FPRINTF((stderr, "aioDisable(%d)\n", fd));
  aioSuspend(fd, AIO_RWX);
  FD_CLR(fd, &xdMask);
  FD_CLR(fd, &fdMask);
  rdHandler[fd]= wrHandler[fd]= exHandler[fd]= 0;
  clientData[fd]= 0;
  /* keep maxFd accurate (drops to zero if no more sockets) */
  while (maxFd && !FD_ISSET(maxFd - 1, &fdMask))
    --maxFd;
}
