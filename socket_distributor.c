#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "socket_distributor.h"

static pthread_mutex_t socketMutex;
static pthread_mutex_t strBufMutex;
static sem_t signaller;
static sem_t socketListSemaphore;
static int* socketList;
static char* buffer;
static size_t strBufLen;
static size_t socketListCursor = 0;
static volatile int interrupted = 0;

static int init(size_t maxSocketCount, size_t buflen){
    // Setup the internal variables
    strBufLen = buflen;
    socketList = malloc(maxSocketCount * sizeof(int));
    buffer = malloc(buflen*sizeof(char));
    if (buffer == NULL || socketList == NULL){
        printf("One or both of the buffers could not be created. Process cannot continue\n");
        return 1;
    }
    int retCode;
    pthread_mutexattr_t attr;
    retCode = pthread_mutexattr_init(&attr);
    if (retCode != 0){
        printf("Failed to initialize mutex atrributes. Errorcode: %d (%s)\n", retCode, strerror(retCode));
        return 1;
    }
    retCode = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    if (retCode != 0){
        printf("Failed to change mutex attributes. Errorcode: %d (%s)\n", retCode, strerror(retCode));
        return 1;
    }
    retCode = pthread_mutex_init(&socketMutex, &attr);
    if (retCode != 0){
        printf("Failed to initialize mutex. Errorcode: %d (%s)\n", retCode, strerror(retCode));
        return 1;
    }
    retCode = pthread_mutex_init(&strBufMutex, &attr);
    if (retCode != 0){
        printf("Failed to initialize mutex. Errorcode: %d (%s)\n", retCode, strerror(retCode));
        return 1;
    }
    if (sem_init(&signaller, 0, 0) == -1){
        printf("Failed to initialize semaphore. Errorcode: %d (%s)\n", errno, strerror(errno));
        return 1;
    }
    if (sem_init(&socketListSemaphore, 0, maxSocketCount) == -1){
        printf("Failed to initialize semaphore. Errorcode: %d (%s)\n", errno, strerror(errno));
        return 1;
    }
    return 0;
}

void* socketDistributorMain(void* socketParamSet){
    struct SocketParamSet* param = (struct SocketParamSet*) socketParamSet;
    if (init(param->maxConnCount, param->buflen) != 0){
        // Something went wrong
        sem_post(&param->errorNotifier);
        sem_post(&param->initCompleteNotifier);
        pthread_exit((void*)1);
    }
    sem_post(&param->initCompleteNotifier);
    while(!interrupted){
        if (sem_wait(&signaller) == -1 && errno == EINTR) continue;
        pthread_mutex_lock(&strBufMutex);
        pthread_mutex_lock(&socketMutex);
        for (size_t i = 0; i < socketListCursor; ++i){
            int acc_con = socketList[i];
            ssize_t writelen = write(acc_con, buffer, strlen(buffer));
            if (writelen == -1){
                if (errno == EPIPE){
                    // The socket is dead, remove it from the list
                    // Move all other elements one step forward
                    memmove(&socketList[i], &socketList[i+1], (socketListCursor - i - 1)*sizeof(int));
                    // There is now one more free space
                    socketListCursor--;
                    sem_post(&socketListSemaphore);
                    i--;
                } else {
                    printf("Error while processing pipes: %d (%s)\n", errno, strerror(errno));
                }
            }
        }
        pthread_mutex_unlock(&socketMutex);
        pthread_mutex_unlock(&strBufMutex);
    }
    return NULL;
}

int addSocket(int acc_con){
    if (sem_trywait(&socketListSemaphore) == -1){
        if (errno != EAGAIN){
            return -1;
        }
        return 0;
    }
    int retcode = pthread_mutex_lock(&socketMutex);
    if (retcode){
        printf("Failed to set mutex: %d (%s)", retcode, strerror(retcode));
        return -1;
    }
    socketList[socketListCursor++] = acc_con;
    retcode = pthread_mutex_unlock(&socketMutex);
    if (retcode){
        printf("Failed to set mutex: %d (%s)", retcode, strerror(retcode));
        return -1;
    }
    if (sem_getvalue(&socketListSemaphore, &retcode)){
        return -1;
    }
    return retcode;
}

void waitForSocketSpace(void){
    sem_wait(&socketListSemaphore);
    sem_post(&socketListSemaphore);
}

int distributeMessageToSockets(char* mes){
    int retcode = pthread_mutex_lock(&strBufMutex);
    if (retcode != 0){
        return retcode;
    }
    size_t stringLength = strlen(mes);
    if (stringLength >= strBufLen){
        memcpy(buffer, mes, strBufLen*sizeof(char)-1);
        buffer[strBufLen - 1] = 0;
    } else {
        memcpy(buffer, mes, stringLength);
    }
    sem_post(&signaller);
    retcode = pthread_mutex_unlock(&strBufMutex);
    if (retcode != 0){
        return retcode;
    }
    return 0;
}

void stopSocketDistributor(void){
    interrupted = 1;
}
