#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_LOCATION "/tmp/socket_test"
#define MAX_MESSAGE_SIZE 128

int main(){
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("Creating the socket failed. Code: %d (%s).\n", errno, strerror(errno));
    }
    /* Bind to a file */
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy((sock_addr.sun_path), SOCKET_LOCATION);
    if (connect(sockfd, (struct sockaddr*) &sock_addr, sizeof(struct sockaddr_un)) == -1){
        printf("Failed to connect to the socket. Code: %d (%s)\n", errno, strerror(errno));
        return -1;
    }
    char buffer[MAX_MESSAGE_SIZE];
    bzero(buffer, MAX_MESSAGE_SIZE);
    ssize_t readlen = 0;
    while(readlen >= 0){
        readlen = recv(sockfd, &buffer, MAX_MESSAGE_SIZE, 0);
        // Iff readlen is 0, then the pipe is closed, because it is a SOCK_STREAM
        if (readlen == 0){
            break;
        }
        if (readlen < 0){
            printf("Failed to read from the socket. Code: %d (%s)\n", errno, strerror(errno));
        } else {
            printf("Message received: %s (readlen: %zd)\n", buffer, readlen);
        }
        bzero(buffer, MAX_MESSAGE_SIZE);
    }
    if (shutdown(sockfd, SHUT_RDWR) == -1){
        printf("Shutting down the socket failed. Code: %d (%s).\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}
