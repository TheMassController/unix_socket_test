#include <semaphore.h>
#include <pthread.h>

#include "socket_distributor.h"

struct SocketHolder{
    int fd;
    struct SocketHolder* next;
};

static sem_t semaphore;
static pthread_mutex_t mutex;
static struct SocketHolder* socketList;

void socket_distributor_main(unsigned maxThreadCount){
    int success = sem_init(&semaphore, 0, 1);
    if (succes == -1){

    }
}
