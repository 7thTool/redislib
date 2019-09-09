#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <redis.h>
#include <ae.h>
#include <anet.h>
#include <sds.h>
#include <adlist.h>
#include <dict.h>
#include <atomicvar.h>

#include <time.h>
#include <syslog.h>
#include <errno.h>

#define CONFIG_BINDADDR_MAX 16

/* Protocol and I/O related defines */
#define PROTO_MAX_QUERYBUF_LEN  (1024*1024*1024) /* 1GB max query buffer. */
#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */
#define PROTO_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define PROTO_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define PROTO_MBULK_BIG_ARG     (1024*32)
#define LONG_STR_SIZE      21          /* Bytes needed for long -> str + '\0' */
#define REDIS_AUTOSYNC_BYTES (1024*1024*32) /* fdatasync every 32MB */

#define LIMIT_PENDING_QUERYBUF (4*1024*1024) /* 4mb */

/* This structure is used in order to represent the output buffer of a client,
 * which is actually a linked list of blocks like that, that is: client->reply. */
typedef struct clientReplyBlock {
    size_t size, used;
    char buf[];
} clientReplyBlock;

/* Client.reply list dup and free methods. */
void *dupClientReplyValue(void *o) {
    clientReplyBlock *old = o;
    clientReplyBlock *buf = zmalloc(sizeof(clientReplyBlock) + old->size);
    memcpy(buf, o, sizeof(clientReplyBlock) + old->size);
    return buf;
}

void freeClientReplyValue(void *o) {
    zfree(o);
}

struct Server {
    /* General */
    aeEventLoop *el;
    int module_blocked_pipe[2]; /* Pipe used to awake the event loop if a
                                   client blocked on a module command needs
                                   to be processed. */
    /* Networking */
    int port;                   /* TCP listening port */
    int tcp_backlog;            /* TCP listen() backlog */
    char *bindaddr[CONFIG_BINDADDR_MAX]; /* Addresses we should bind to */
    int bindaddr_count;         /* Number of addresses in server.bindaddr[] */
    char *unixsocket;           /* UNIX socket path */
    mode_t unixsocketperm;      /* UNIX socket permission */
    int ipfd[CONFIG_BINDADDR_MAX]; /* TCP socket file descriptors */
    int ipfd_count;             /* Used slots in ipfd[] */
    int sofd;                   /* Unix socket file descriptor */
    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */
    uint64_t next_client_id;
}server;

/* With multiplexing we need to take per-client state.
 * Clients are taken in a linked list. */
typedef struct client {
    uint64_t id;            /* Client incremental unique ID. */
    int fd;                 /* Client socket. */
    //redisDb *db;            /* Pointer to currently SELECTed DB. */
    //robj *name;             /* As set by CLIENT SETNAME. */
    sds querybuf;           /* Buffer we use to accumulate client queries. */
    size_t qb_pos;          /* The position we have read in querybuf. */
    sds pending_querybuf;   /* If this client is flagged as master, this buffer
                               represents the yet not applied portion of the
                               replication stream that we are receiving from
                               the master. */
    size_t querybuf_peak;   /* Recent (100ms or more) peak of querybuf size. */
    //int argc;               /* Num of arguments of current command. */
    //robj **argv;            /* Arguments of current command. */
    //struct redisCommand *cmd, *lastcmd;  /* Last command executed. */
    int reqtype;            /* Request protocol type: PROTO_REQ_* */
    int multibulklen;       /* Number of multi bulk arguments left to read. */
    long bulklen;           /* Length of bulk argument in multi bulk request. */
    list *reply;            /* List of reply objects to send to the client. */
    unsigned long long reply_bytes; /* Tot bytes of objects in reply list. */
    size_t sentlen;         /* Amount of bytes already sent in the current
                               buffer or object being sent. */
    time_t ctime;           /* Client creation time. */
    time_t lastinteraction; /* Time of the last interaction, used for timeout */
    time_t obuf_soft_limit_reached_time;
    int flags;              /* Client flags: CLIENT_* macros. */
    int authenticated;      /* When requirepass is non-NULL. */
    int replstate;          /* Replication state if this is a slave. */
    int repl_put_online_on_ack; /* Install slave write handler on ACK. */
    int repldbfd;           /* Replication DB file descriptor. */
    off_t repldboff;        /* Replication DB file offset. */
    off_t repldbsize;       /* Replication DB file size. */
    sds replpreamble;       /* Replication DB preamble. */
    long long read_reploff; /* Read replication offset if this is a master. */
    long long reploff;      /* Applied replication offset if this is a master. */
    long long repl_ack_off; /* Replication ack offset, if this is a slave. */
    long long repl_ack_time;/* Replication ack time, if this is a slave. */
    long long psync_initial_offset; /* FULLRESYNC reply offset other slaves
                                       copying this slave output buffer
                                       should use. */
    sds peerid;             /* Cached peer ID. */
    listNode *client_list_node; /* list node in client list */

    /* Response buffer */
    int bufpos;
    char buf[PROTO_REPLY_CHUNK_BYTES];
} client;

#ifndef UNUSED
#define UNUSED
#endif

/* Error codes */
#define C_OK                    0
#define C_ERR                   -1

/* Log levels */
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_RAW (1<<10) /* Modifier to log without timestamp */

/* Like serverLogRaw() but with printf-alike support. This is the function that
 * is used across the code. The raw version is only used in order to dump
 * the INFO output on crash. */
void serverLog(int level, const char *fmt, ...) {
    va_list ap;
    char msg[256];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    printf("%d %s\n", level, msg);
}

// void _serverPanic(const char *file, int line, const char *msg, ...) {
//     va_list ap;
//     va_start(ap,msg);
//     char fmtmsg[256];
//     vsnprintf(fmtmsg,sizeof(fmtmsg),msg,ap);
//     va_end(ap);

//     //bugReportStart();
//     serverLog(LL_WARNING,"------------------------------------------------");
//     serverLog(LL_WARNING,"!!! Software Failure. Press left mouse button to continue");
//     serverLog(LL_WARNING,"Guru Meditation: %s #%s:%d",fmtmsg,file,line);
// #ifdef HAVE_BACKTRACE
//     serverLog(LL_WARNING,"(forcing SIGSEGV in order to print the stack trace)");
// #endif
//     serverLog(LL_WARNING,"------------------------------------------------");
//     *((char*)-1) = 'x';
// }

#define serverPanic(...) _serverPanic(__FILE__,__LINE__,__VA_ARGS__),_exit(1)

int timerCallback(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    return 0;
}

#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define NET_PEER_ID_LEN (NET_IP_STR_LEN+32) /* Must be enough for ip:port */

#define MAX_ACCEPTS_PER_CALL 1000

/* Initialize a set of file descriptors to listen to the specified 'port'
 * binding the addresses specified in the Redis server configuration.
 *
 * The listening file descriptors are stored in the integer array 'fds'
 * and their number is set in '*count'.
 *
 * The addresses to bind are specified in the global server.bindaddr array
 * and their number is server.bindaddr_count. If the server configuration
 * contains no specific addresses to bind, this function will try to
 * bind * (all addresses) for both the IPv4 and IPv6 protocols.
 *
 * On success the function returns C_OK.
 *
 * On error the function returns C_ERR. For the function to be on
 * error, at least one of the server.bindaddr addresses was
 * impossible to bind, or no bind addresses were specified in the server
 * configuration but the function is not able to bind * for at least
 * one of the IPv4 or IPv6 protocols. */
int listenToPort(int port, int *fds, int *count) {
    int j;

    /* Force binding of 0.0.0.0 if no bind address is specified, always
     * entering the loop if j == 0. */
    if (server.bindaddr_count == 0) server.bindaddr[0] = NULL;
    for (j = 0; j < server.bindaddr_count || j == 0; j++) {
        if (server.bindaddr[j] == NULL) {
            int unsupported = 0;
            /* Bind * for both IPv6 and IPv4, we enter here only if
             * server.bindaddr_count == 0. */
            fds[*count] = anetTcp6Server(server.neterr,port,NULL,
                server.tcp_backlog);
            if (fds[*count] != ANET_ERR) {
                anetNonBlock(NULL,fds[*count]);
                (*count)++;
            } else if (errno == EAFNOSUPPORT) {
                unsupported++;
                serverLog(LL_WARNING,"Not listening to IPv6: unsupproted");
            }

            if (*count == 1 || unsupported) {
                /* Bind the IPv4 address as well. */
                fds[*count] = anetTcpServer(server.neterr,port,NULL,
                    server.tcp_backlog);
                if (fds[*count] != ANET_ERR) {
                    anetNonBlock(NULL,fds[*count]);
                    (*count)++;
                } else if (errno == EAFNOSUPPORT) {
                    unsupported++;
                    serverLog(LL_WARNING,"Not listening to IPv4: unsupproted");
                }
            }
            /* Exit the loop if we were able to bind * on IPv4 and IPv6,
             * otherwise fds[*count] will be ANET_ERR and we'll print an
             * error and return to the caller with an error. */
            if (*count + unsupported == 2) break;
        } else if (strchr(server.bindaddr[j],':')) {
            /* Bind IPv6 address. */
            fds[*count] = anetTcp6Server(server.neterr,port,server.bindaddr[j],
                server.tcp_backlog);
        } else {
            /* Bind IPv4 address. */
            fds[*count] = anetTcpServer(server.neterr,port,server.bindaddr[j],
                server.tcp_backlog);
        }
        if (fds[*count] == ANET_ERR) {
            serverLog(LL_WARNING,
                "Could not create server TCP listening socket %s:%d: %s",
                server.bindaddr[j] ? server.bindaddr[j] : "*",
                port, server.neterr);
                if (errno == ENOPROTOOPT     || errno == EPROTONOSUPPORT ||
                    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
                    errno == EAFNOSUPPORT    || errno == EADDRNOTAVAIL)
                    continue;
            return C_ERR;
        }
        anetNonBlock(NULL,fds[*count]);
        (*count)++;
    }
    return C_OK;
}

void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    char buf[1024] = {0};
    int nread = read(fd, buf, 1024);
    if (nread == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            serverLog(LL_VERBOSE, "Error from client: %s",strerror(errno));
            close(fd);
            return;
        }
    } else if (nread == 0) {
        serverLog(LL_VERBOSE, "Client closed connection");
        close(fd);
        return;
    } else {
        serverLog(LL_VERBOSE,"Reading from client: %s", buf);
        write(fd, buf, nread);
        //close(fd);
    }
}

client *createClient(int fd) {
    client *c = zmalloc(sizeof(client));

    /* passing -1 as fd it is possible to create a non connected client.
     * This is useful since all the commands needs to be executed
     * in the context of a client. When commands are executed in other
     * contexts (for instance a Lua script) we need a non connected client. */
    if (fd != -1) {
        anetNonBlock(NULL,fd);
        anetEnableTcpNoDelay(NULL,fd);
        //if (server.tcpkeepalive)
        //    anetKeepAlive(NULL,fd,server.tcpkeepalive);
        if (aeCreateFileEvent(server.el,fd,AE_READABLE,
            readQueryFromClient, c) == AE_ERR)
        {
            close(fd);
            zfree(c);
            return NULL;
        }
    }

    //selectDb(c,0);
    uint64_t client_id;
    atomicGetIncr(server.next_client_id,client_id,1);
    c->id = client_id;
    c->fd = fd;
    //c->name = NULL;
    c->bufpos = 0;
    c->qb_pos = 0;
    c->querybuf = sdsempty();
    c->pending_querybuf = sdsempty();
    c->querybuf_peak = 0;
    c->reqtype = 0;
    //c->argc = 0;
    //c->argv = NULL;
    //c->cmd = c->lastcmd = NULL;
    c->multibulklen = 0;
    c->bulklen = -1;
    c->sentlen = 0;
    c->flags = 0;
    //c->ctime = c->lastinteraction = server.unixtime;
    c->authenticated = 0;
    //c->replstate = REPL_STATE_NONE;
    c->repl_put_online_on_ack = 0;
    c->reploff = 0;
    c->read_reploff = 0;
    c->repl_ack_off = 0;
    c->repl_ack_time = 0;
    c->reply = listCreate();
    c->reply_bytes = 0;
    c->obuf_soft_limit_reached_time = 0;
    listSetFreeMethod(c->reply,freeClientReplyValue);
    listSetDupMethod(c->reply,dupClientReplyValue);
    // c->btype = BLOCKED_NONE;
    // c->bpop.timeout = 0;
    // c->bpop.keys = dictCreate(&objectKeyHeapPointerValueDictType,NULL);
    // c->bpop.target = NULL;
    // c->bpop.xread_group = NULL;
    // c->bpop.xread_consumer = NULL;
    // c->bpop.xread_group_noack = 0;
    // c->bpop.numreplicas = 0;
    // c->bpop.reploffset = 0;
    // c->woff = 0;
    // c->watched_keys = listCreate();
    // c->pubsub_channels = dictCreate(&objectKeyPointerValueDictType,NULL);
    // c->pubsub_patterns = listCreate();
    c->peerid = NULL;
    c->client_list_node = NULL;
    // listSetFreeMethod(c->pubsub_patterns,decrRefCountVoid);
    // listSetMatchMethod(c->pubsub_patterns,listMatchObjects);
    // if (fd != -1) linkClient(c);
    // initClientMultiState(c);
    return c;
}

#define MAX_ACCEPTS_PER_CALL 1000
static void acceptCommonHandler(int fd, int flags, char *ip) {
    client *c;
    if ((c = createClient(fd)) == NULL) {
        serverLog(LL_WARNING,
            "Error registering fd event for the new client: %s (fd=%d)",
            strerror(errno),fd);
        close(fd); /* May be already closed, just ignore errors */
        return;
    }

    c->flags |= flags;
}

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetTcpAccept(server.neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                serverLog(LL_WARNING,
                    "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_VERBOSE,"Accepted %s:%d", cip, cport);
        acceptCommonHandler(cfd,0,cip);
    }
}

#define CLIENT_UNIX_SOCKET (1<<11) /* Client connected via Unix domain socket */
void acceptUnixHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cfd, max = MAX_ACCEPTS_PER_CALL;
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetUnixAccept(server.neterr, fd);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                serverLog(LL_WARNING,
                    "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_VERBOSE,"Accepted connection to %s", server.unixsocket);
        acceptCommonHandler(cfd,CLIENT_UNIX_SOCKET,NULL);
    }
}

void prog_exit(int signo)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	serverLog(LL_WARNING, "send stop signal...");
    aeStop(server.el);
}

int main (int argc, char **argv) {
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, prog_exit);
	signal(SIGKILL, prog_exit);
	signal(SIGTERM, prog_exit);

    int j;
    server.el = aeCreateEventLoop(64);
    
    if (aeCreateTimeEvent(server.el, 1000, timerCallback, NULL, NULL) == AE_ERR) {
        serverLog(LL_WARNING, "Can't create event loop timers.");
        exit(1);
    }

    /* Open the TCP listening socket for the user commands. */
    if (listenToPort(6677,server.ipfd,&server.ipfd_count) == C_ERR)
        exit(1);

    /* Open the listening Unix domain socket. */
    if (server.unixsocket != NULL) {
        unlink(server.unixsocket); /* don't care if this fails */
        server.sofd = anetUnixServer(server.neterr,server.unixsocket,
            server.unixsocketperm, server.tcp_backlog);
        if (server.sofd == ANET_ERR) {
            serverLog(LL_WARNING, "Opening Unix socket: %s", server.neterr);
            exit(1);
        }
        anetNonBlock(NULL,server.sofd);
    }

    /* Abort if there are no listening sockets at all. */
    if (server.ipfd_count == 0 && server.sofd < 0) {
        serverLog(LL_WARNING, "Configured to not listen anywhere, exiting.");
        exit(1);
    }

    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
    for (j = 0; j < server.ipfd_count; j++) {
        if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE,
            acceptTcpHandler,NULL) == AE_ERR)
            {
                serverPanic(
                    "Unrecoverable error creating server.ipfd file event.");
            }
    }
    if (server.sofd > 0 && aeCreateFileEvent(server.el,server.sofd,AE_READABLE,
        acceptUnixHandler,NULL) == AE_ERR) serverPanic("Unrecoverable error creating server.sofd file event.");


    aeMain(server.el);
    return 0;
}
