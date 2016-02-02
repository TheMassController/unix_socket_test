#include <semaphore.h>
/**
 * @file socket_distributor.h
 * @author Jacko Dirks
 *
 * The idea of this module is that is has a number of sockets and it forwards every message you give it to every socket.
 */

struct SocketParamSet {
    size_t maxConnCount;
    size_t buflen;
    char* socketLocation;
    sem_t initCompleteNotifier;
    sem_t errorNotifier;
};

/**
 * @brief Initializes and starts the socket distributor. Run this function from a pthread.
 * @param maxThreadCount The maximum amount of threads that will be served by this socket
 * @param buflen The length of the internal string buffer.
 * @param initCompleteNotifier This semaphore MUST be initialized, it will be set once this thread has completed initialization
 */
void socketDistributorMain(void* socketDistributorMainParam);

/**
 * @brief signals the distributor to stop.
 *
 * This will stop the thread and return once it is done.
 */
void stopDistributor(void);

/**
 * @brief add a socket to the socket distributor
 * @param acc_con The filedescriptor of the new-to-be-added socket
 * @return >0 if there is space afterwards, 0 if not, -1 if there is an error (errno will be set)
 */
int addSocket(int acc_con);

/**
 * @brief Returns if there is space to allow a new socket into the message distributor
 *
 * Returns immediately if there was space anyway, makes the callee thread sleep if there was no space, while waiting for space.
 */
void waitForSocketSpace(void);

/**
 * @brief Forwards string to all connected clients
 * @param mes This message is copied to he internal buffer before returning. It is truncated if it passed given buflen (passed to the main)
 * @return 0 if erverything was ok. See errno for non-0 return code
 */
int distributeMessageToSockets(char* mes);
