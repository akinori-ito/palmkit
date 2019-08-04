/*
 * functions to remove temporary files
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>

#include <remove_tmp.h>

/* lists to hold file names to be removed */
typedef struct files_to_be_removed {
  char *filename;
  struct files_to_be_removed *next;
} Files2BRemoved;

/* A global list of files to be removed */
static Files2BRemoved *ToBeRemoved = NULL;

/* put filename into the list */
void
RemoveAfter(char *file)
{
  Files2BRemoved *l;
  l = malloc(sizeof(Files2BRemoved));
  l->next = ToBeRemoved;
  l->filename = strdup(file);
  ToBeRemoved = l;
}

/* remove listed files */
void
DoCleanup()
{
  Files2BRemoved *l;
  for (l = ToBeRemoved; l != NULL; l = l->next)
    unlink(l->filename);
}

/* callback routine for signalling */
RETSIGTYPE
scavenger_cb(int sig)
{
  DoCleanup();
  signal(sig,SIG_DFL);
#ifdef HAS_KILL
  kill(getpid(),sig);
#endif
}

void TrapSignalsToScavenge(int sig1, ...)
{
  va_list ap;
  int sig;

  va_start(ap,sig1);
  signal(sig1,scavenger_cb);
  for (;;) {
    sig = va_arg(ap,int);
    if (sig == 0)
      break;
    signal(sig,scavenger_cb);
  }
  va_end(ap);
}

void TrapAllSignals()
{
    TrapSignalsToScavenge(
#ifdef SIGHUP
			  SIGHUP,
#endif
			  SIGINT,
#ifdef SIGQUIT
			  SIGQUIT,
#endif
			  SIGILL,
			  SIGABRT,
#ifdef SIGBUS
			  SIGBUS,
#endif
			  SIGFPE,
			  SIGSEGV,
#ifdef SIGPIPE
			  SIGPIPE,
#endif
			  SIGTERM,
#ifdef SIGSTKFLT
			  SIGSTKFLT,
#endif
#ifdef SIGXFSZ
			  SIGXFSZ,
#endif
#ifdef SIGIO
			  SIGIO,
#endif
			  0);
}
