// This file behaves as the main, creating the socket listener and distributing messages every once in a while
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#include "socket_distributor.h"

static volatile int interrupted = 0;
static struct SocketParamSet params;

static size_t bufSize = 128;
static size_t maxClientCount = 32;
static char* socketFileName = "/tmp/socket_test";

static void interruptHandler(int signum){
    (void) (signum);
    interrupted = 1;
}

int main(){
    // Setup signal handling
    // We want to catch sigint and sigterm (control+c and kill) togracefully shutdown (ie destroy sockets).
    struct sigaction intAction;
    intAction.sa_handler = interruptHandler;
    // See man sigemptyset
    sigemptyset(&intAction.sa_mask);
    intAction.sa_flags = 0;
    sigaction(SIGINT, &intAction, NULL);
    sigaction(SIGTERM, &intAction, NULL);
    // Ignore the sigpipe, let read handle that one when the time comes
    intAction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &intAction, NULL);

    // Now it needs to setup threads. Two of them, the first one listens for clients that want to connect,
    // The second one distributes messages to clients.
    // But first, we need some parameters. For now, we will define them here
    params.maxConnCount = maxClientCount;
    params.buflen = bufSize;
    params.socketLocation = socketFileName;


    // Create the subthread
    sem_t initSem;
    sem_init(&initSem, 0, 0);
}
