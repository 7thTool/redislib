#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>

#include <redis.h>
#include <ae.h>
#include <anet.h>

/* Put event loop in the global scope, so it can be explicitly stopped */
static aeEventLoop *loop;
static int connected = 0;

int timerCallback(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    aeDeleteTimeEvent(eventLoop, id);
    //aeStop(eventLoop);
    return 0;
}

void TcpCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    if(mask & AE_READABLE) {
        char buf[1024] = {0};
        int nread = read(fd, buf, 1024);
        if (nread == -1) {
            if (errno == EAGAIN) {
                return;
            } else {
                printf("Error from server: %s\n",strerror(errno));
                close(fd);
                return;
            }
        } else if (nread == 0) {
            printf("Server closed connection\n");
            close(fd);
            return;
        } else {
            printf("Reading from server: %s\n", buf);
        }
    }
    if(mask & AE_WRITABLE) {
        if(!connected) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
                printf("getsockopt(SO_ERROR)\n");
                return;
            }
            if (err) {
                errno = err;
                printf("connect failed:%s\n", strerror(errno));
                return;
            } else {
                connected = 1;
                printf("connect success.\n");
            }
        }
        int nwrite = anetWrite(fd, "hello", 5);

        printf("close connection\n");
        close(fd);
        connected = 0;
    }
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
        printf("Can't create event loop timers.");
        exit(1);
    }

    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */
    int fd = anetTcpNonBlockConnect(neterr, "127.0.0.1", 6677);
    anetNonBlock(NULL,fd);
    anetEnableTcpNoDelay(NULL,fd);
    aeCreateFileEvent(loop, fd, AE_ALL_EVENTS, TcpCallback, NULL);

    aeMain(loop);
    aeDeleteEventLoop(loop);
    return 0;
}

