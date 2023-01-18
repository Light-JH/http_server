#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tcp.h"

static volatile int receivedSigInt = 0;

void intHandler(int signal)
{
    receivedSigInt = 1;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }
    const int port = atoi(argv[1]);

    printf("Starting server on port %d...\n", port);
    const int server_fd = setupServer(port);
    if (server_fd < 0)
    {
        return -1;
    }

    // Regist SIGINT handling
    signal(SIGINT, intHandler);

    // Loop until SIGINT is received
    while (!receivedSigInt)
    {
    }

    // SIGINT received: clean up and close
    printf("Exiting...\n");
    close(server_fd);

    return 0;
}