#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "redis.h"

/* _serverAssert is needed by dict */
void _serverAssert(const char *estr, const char *file, int line) {
    fprintf(stderr, "=== ASSERTION FAILED ===");
    fprintf(stderr, "==> %s:%d '%s' is not true",file,line,estr);
    *((char*)-1) = 'x';
}

void _serverPanic(const char *file, int line, const char *msg, ...) {
    /*va_list ap;
    va_start(ap,msg);
    char fmtmsg[256];
    vsnprintf(fmtmsg,sizeof(fmtmsg),msg,ap);
    va_end(ap);

    //bugReportStart();
    serverLog(LL_WARNING,"------------------------------------------------");
    serverLog(LL_WARNING,"!!! Software Failure. Press left mouse button to continue");
    serverLog(LL_WARNING,"Guru Meditation: %s #%s:%d",fmtmsg,file,line);
#ifdef HAVE_BACKTRACE
    serverLog(LL_WARNING,"(forcing SIGSEGV in order to print the stack trace)");
#endif
    serverLog(LL_WARNING,"------------------------------------------------");
    *((char*)-1) = 'x';*/
}