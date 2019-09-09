#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <redis.h>
#include <ae.h>

/* Put event loop in the global scope, so it can be explicitly stopped */
static aeEventLoop *loop;

int timerCallback(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    aeDeleteTimeEvent(eventLoop, id);
    //aeStop(eventLoop);
    return 0;
}

void prog_exit(int signo)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	printf("send stop signal...\n");
    aeStop(loop);
}

int main (int argc, char **argv) {
    signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, prog_exit);
	signal(SIGKILL, prog_exit);
	signal(SIGTERM, prog_exit);

    loop = aeCreateEventLoop(64);
    
    if (aeCreateTimeEvent(loop, 1000, timerCallback, NULL, NULL) == AE_ERR) {
        printf("Can't create event loop timers.\n");
        exit(1);
    }

    aeMain(loop);
    aeDeleteEventLoop(loop);
    return 0;
}

