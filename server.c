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

#include "socket_server.h"
#include "socket_distributor.h"
/* MAXCONNCOUNT only talks about the amount of backlog: the amount of connections that can be waiting to be accepted at the same time.
 * Multiple can still be accepted, but only if they start waiting after the last one was accepted
 * Its only a hint, by the way. So an implimentation can still allow a bigger or a smaller buffer then specified here.
 */
#define MAXCONNCOUNT 1

volatile int interrupted = 0;

// The param of this function is the send signal.
void interruptHandler(int signum){
    (void) (signum);
    interrupted = 1;
}

/* The server side creates the socket */
int main(){
    // Setup sighandling
    struct sigaction intAction;
    intAction.sa_handler = interruptHandler;
    // See man sigemptyset
    sigemptyset(&intAction.sa_mask);
    intAction.sa_flags = 0;
    sigaction(SIGINT, &intAction, NULL);
    // This piece of code tries if the socket already exists and removes it if it does. ENOENT means that the socket did not exist, we can ignore that error.
    if (unlink(SOCKET_LOCATION) == -1){
        if (errno != ENOENT){
            printf("Unlinking the socket failed. Code: %d (%s).\n", errno, strerror(errno));
            return -1;
        }
    }
    // Create the socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("Creating the socket failed. Code: %d (%s).\n", errno, strerror(errno));
    }
    /* Bind the socket to a file */
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy((sock_addr.sun_path), SOCKET_LOCATION);
    if (bind(sockfd,(struct sockaddr*) &sock_addr, sizeof(struct sockaddr_un)) == -1){
        printf("Binding the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    /* Mark it as a listen socket.*/
    if (listen(sockfd, MAXCONNCOUNT) == -1){
        printf("Marking the socket as a listen socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    /* At this point we seem to have a working socket. Accept a single connection on it. */
    struct sockaddr_un acc_sockaddr;
    socklen_t acc_socklen = sizeof(struct sockaddr_un);
    int acc_con = accept(sockfd, (struct sockaddr*)&acc_sockaddr, &acc_socklen);
    if (acc_con == -1){
        printf("Accepting a connection failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    /* Write to the client */
    char buffer[MAX_MESSAGE_SIZE];
    ssize_t writelen;
    for (int i = 0; !interrupted ; ++i){
        snprintf(buffer, MAX_MESSAGE_SIZE, "This is message number %d.", i);
        writelen = write(acc_con, buffer, strlen(buffer));
        if (writelen == -1){
            printf("Failed to write to socket. Code: %d (%s).\n", errno, strerror(errno));
        }
        printf("Sending message %d\n", i);
        sleep(1);
    }
    if (close(acc_con) == -1){
        printf("Shutting down the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    if (close(sockfd) == -1){
        printf("Shutting down the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    if (unlink(SOCKET_LOCATION) == -1){
        printf("Unlinking the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}
