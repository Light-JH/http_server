#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
    struct ServerState server_state;
    if (setupServer(port, &server_state) < 0)
    {
        return -1;
    }

    // Register SIGINT handling
    signal(SIGINT, intHandler);

    // Loop until SIGINT is received
    while (!receivedSigInt)
    {
        if (serverTick(&server_state) < 0)
        {
            break;
        }
    }

    shutdownServer(&server_state);

    return 0;
}