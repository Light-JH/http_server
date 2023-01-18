#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tcp.h"

#define MAX_CLIENTS 1
static volatile int receivedSigInt = 0;

void intHandler(int signal)
{
    receivedSigInt = 1;
}

struct ServerState
{
    int server_fd;
    int num_clients;
    struct pollfd pollfds[MAX_CLIENTS + 1];
};

void processClientMessage(int client_fd, char *buffer, int len)
{
    // TODO: Check for valid HTTP structure, and respond accordingly.
    // For now just echo back
    send(client_fd, buffer, len, 0);
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

    // Register SIGINT handling
    signal(SIGINT, intHandler);

    // Initialize ServerState struct
    struct ServerState server_state;
    memset(&server_state, 0, sizeof(server_state));
    server_state.server_fd = server_fd;
    server_state.pollfds[0].fd = server_fd;
    server_state.pollfds[0].events = POLLIN | POLLPRI;

    // Buffer for accepting messages from client
    char buffer[1024];

    // Loop until SIGINT is received
    while (!receivedSigInt)
    {
        // Poll for timeout
        int poll_result = poll(server_state.pollfds, server_state.num_clients + 1, 100);
        if (poll_result > 0)
        {
            // Client connect event
            if (server_state.pollfds[0].revents & POLLIN)
            {
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
                if (client_fd < 0)
                {
                    printf("Error accepting client...\n");
                    break;
                }

                // Search for a pollfd to assign this client into
                for (int idx = 1; idx <= MAX_CLIENTS; idx++)
                {
                    if (server_state.pollfds[idx].fd == 0)
                    {
                        server_state.pollfds[idx].fd = client_fd;
                        server_state.pollfds[idx].events = POLLIN | POLLPRI;
                        server_state.num_clients++;
                        printf("Client %d connected: %s:%u\n", idx, inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
                        break;
                    }
                }
            }

            // Serve clients
            for (int idx = 1; idx <= MAX_CLIENTS; idx++)
            {
                if (server_state.pollfds[idx].fd > 0 && server_state.pollfds[idx].revents & POLLIN)
                {
                    int len = read(server_state.pollfds[idx].fd, buffer, sizeof(buffer) - 1);
                    if (len <= 0) // Error (-1) or Disconnect (0)
                    {
                        printf("Client %d disconnected\n", idx);
                        close(server_state.pollfds[idx].fd);
                        memset(&server_state.pollfds[idx], 0, sizeof(struct pollfd));
                        server_state.num_clients--;
                    }
                    else
                    {
                        processClientMessage(server_state.pollfds[idx].fd, buffer, len);
                    }
                }
            }
        }
    }

    // Error or SIGINT received: clean up and close
    printf("Exiting...\n");

    // Close open clients
    for (int idx = 1; idx <= MAX_CLIENTS; idx++)
    {
        if (server_state.pollfds[idx].fd > 0)
        {
            printf("Closing client %d...\n", idx - 1);
            close(server_state.pollfds[idx].fd);
            memset(&server_state.pollfds[idx], 0, sizeof(struct pollfd));
            server_state.num_clients--;
        }
    }
    printf("Shutting down server...\n");
    close(server_state.server_fd);

    return 0;
}