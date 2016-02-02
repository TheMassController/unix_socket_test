/* General idea:
 * A socket normally only exists as a piece of memory mapped to a process.
 * You can bind a socket to a file and, depending on the properties of the file, the socket is now publicly available.
 * By marking the socket as a listen socket. This makes it a passive socket, processes can connect to this socket and if the connection is accepted a new socket is spawned
 * Using this new socket the client and server can now talk. Talking happens trough ports, the same ports as TCP and UDP use.
 * When binding a socket to a file, it is still just a port. Its just that the port is random from the perspective of client and server, while a file name does not have to be.
 */
#include <stdio.h>                  // Printf
#include <errno.h>                  // Error code translation, errno
#include <string.h>                 // strerr
#include <unistd.h>                 // Sleep, unlink
#include <sys/types.h>              // Defines types like
#include <sys/socket.h>             // Socket functions like socket
#include <sys/un.h>                 // UNIX domain sockets defenitions
#include <signal.h>                 // To catch signals passed to this process
#include <pthread.h>                // Pthreads
#include <semaphore.h>              // Semaphores

#include "socket_distributor.h"

static volatile int interrupted = 0;
// The param of this function is the send signal.
static void interruptHandler(int signum){
    (void) (signum);
    interrupted = 1;
}

static void errorExitProcedure(sem_t initCompleteNotifier, sem_t errorNotifier){
    sem_post(&errorNotifier);
    sem_post(&initCompleteNotifier);
    pthread_exit((void*)1);
}

static int openSocket(char* socket_file, size_t maxConnCount){
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("Creating the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    /* Bind the socket to a file */
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy((sock_addr.sun_path), socket_file);
    if (bind(sockfd,(struct sockaddr*) &sock_addr, sizeof(struct sockaddr_un)) == -1){
        printf("Binding the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    /* Mark it as a listen socket.*/
    if (listen(sockfd, maxConnCount) == -1){
        printf("Marking the socket as a listen socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    return sockfd;
}

static int closeSocket(char* socket_file, int sockfd){
    if (close(sockfd) == -1){
        printf("Shutting down the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    if (unlink(socket_file) == -1){
        printf("Unlinking the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

/* The server side creates the socket */
void listenSocketServer(void* socketParamSet){
    struct SocketParamSet* param = (struct SocketParamSet*) socketParamSet;
    // Create a seperate handler for sigint, this signal will be send to all threads when the process wants to quit
    struct sigaction intAction;
    intAction.sa_handler = interruptHandler;
    // See man sigemptyset
    sigemptyset(&intAction.sa_mask);
    intAction.sa_flags = 0;
    sigaction(SIGINT, &intAction, NULL);
    sigaction(SIGTERM, &intAction, NULL);

    // This piece of code tries if the socket already exists and removes it if it does. ENOENT means that the socket did not exist, we can ignore that error.
    if (unlink(param->socketLocation) == -1){
        if (errno != ENOENT){
            printf("Unlinking the socket failed. Code: %d (%s).\n", errno, strerror(errno));
            errorExitProcedure(param->initCompleteNotifier, param->errorNotifier);
        }
    }

    // Create the socket
    int sockfd = openSocket(param->socketLocation, param->maxConnCount);
    if (sockfd == -1){
        errorExitProcedure(param->initCompleteNotifier, param->errorNotifier);
    }
    // Notify the main process that the startup was successfull
    sem_post(&param->initCompleteNotifier);
    struct sockaddr_un acc_sockaddr;
    socklen_t acc_socklen = sizeof(struct sockaddr_un);
    // At this point we seem to have a working socket. Accept connections on it
    while(!interrupted){
        int acc_con = accept(sockfd, (struct sockaddr*)&acc_sockaddr, &acc_socklen);
        if (acc_con == -1){
            if (errno == EINTR){
                // A signal was received, this usually means that the socket needs to shutdown, so interrupted will be high.
                // Just continue and let the rest of the code run
                continue;
            }else {
                printf("Accepting a connection failed. Code: %d (%s).\n", errno, strerror(errno));
                errorExitProcedure(param->initCompleteNotifier, param->errorNotifier);
            }
        }
        int retCode = addSocket(acc_con);
        if (retCode < 0){
            printf("Adding the socket to the socketlist failed. Code: %d (%s).\n", errno, strerror(errno));
            errorExitProcedure(param->initCompleteNotifier, param->errorNotifier);
        }
        if (retCode == 0){
            //Stop the listener, wait for space and the reopen the listener
            retCode = closeSocket(param->socketLocation, sockfd);
            if (retCode == -1){
                errorExitProcedure(param->initCompleteNotifier, param->errorNotifier);
            }
            waitForSocketSpace();
            addSocket(acc_con);
            sockfd = openSocket(param->socketLocation, param->maxConnCount);
        }
    }
    closeSocket(param->socketLocation, sockfd);
}
