// This file behaves as the main, creating the socket listener and distributing messages every once in a while
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "socket_distributor.h"
#include "server_listener.h"

static volatile int interrupted = 0;
static struct SocketParamSet params;
// Semi-hard defined, for now
static size_t bufSize = 128;
static size_t maxClientCount = 3;
static char* socketFileName = "/tmp/socket_test";

static void interruptHandler(int signum){
    (void) (signum);
    interrupted = 1;
}

int main(){
    // Setup signal handling
    // We want to catch sigint and sigterm (control+c and kill) to gracefully shutdown (ie destroy sockets).
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
    params.maxConnCount = maxClientCount;
    params.buflen = bufSize;
    params.socketLocation = socketFileName;
    // Create the semaphores
    sem_init(&params.initCompleteNotifier, 0, 0);
    sem_init(&params.errorNotifier, 0, 0);
    // Create the socket distributor first
    pthread_attr_t attr;
    pthread_t distributorThread, listenThread;
    pthread_attr_init(&attr);
    pthread_create(&distributorThread, &attr, socketDistributorMain, (void*)&params);
    sem_wait(&params.initCompleteNotifier);
    int retVal = sem_trywait(&params.errorNotifier);
    if (retVal == -1 && errno != EAGAIN){
        printf("Error while reading errorNotifier: %d (%s). Cannot continue\n", errno, strerror(errno));
        return 1;
    }
    // If retval is 0 there was an error
    if (retVal == 0) return 1;
    // Create the listener thread
    pthread_create(&listenThread, &attr, listenSocketServer, (void*)&params);
    sem_wait(&params.initCompleteNotifier);
    retVal = sem_trywait(&params.errorNotifier);
    if (retVal == -1 && errno != EAGAIN){
        printf("Error while reading errorNotifier: %d (%s). Cannot continue\n", errno, strerror(errno));
        return 1;
    }
    char buffer[bufSize];
    // If retval is 0 there was an error
    if (retVal == 0) return 1;
    for (int i = 0; !interrupted; ++i){
        snprintf(buffer, bufSize, "This is message number %d", i);
        distributeMessageToSockets(buffer);
        retVal = sleep(2);
        if (retVal != 0){
            printf("Sleep was interrupted: %d seconds left\n", retVal);
        }
    }
    int* retValPtr = &retVal;
    stopSocketListener();
    int faultCode = pthread_kill(listenThread, SIGINT);
    if (faultCode != 0){
        printf("Error: %d (%s)\n", faultCode, strerror(faultCode));
    }
    faultCode = pthread_join(listenThread, (void**) &(retValPtr));
    if (faultCode != 0){
        printf("Error: %d (%s)\n", faultCode, strerror(faultCode));
    }
    stopSocketDistributor();
    faultCode = pthread_kill(distributorThread, SIGINT);
    if (faultCode != 0){
        printf("Error: %d (%s)\n", faultCode, strerror(faultCode));
    }
    faultCode = pthread_join(distributorThread, (void**) &(retValPtr));
    if (faultCode != 0){
        printf("Error: %d (%s)\n", faultCode, strerror(faultCode));
    }
}
