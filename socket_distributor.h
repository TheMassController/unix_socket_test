/**
 * @brief Initializes and starts the socket distributor. Run this function from a pthread.
 * @param maxThreadCount The maximum amount of threads that will be served by this socket
 */
void socket_distributor_main(unsigned maxThreadCount);

/**
 * @brief signals the distributor to stop.
 *
 * This will stop the thread and return once it is done.
 */
void stop_distributor(void);

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
