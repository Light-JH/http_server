#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 1

struct ServerState
{
    int server_fd;
    int num_clients;
    struct pollfd pollfds[MAX_CLIENTS + 1];
    char buffer[65536];
};

// Set up TCP server on given port. Return the FD.
int setupServer(int port, struct ServerState *server_state)
{
    // Zero out the server state
    memset(server_state, 0, sizeof(*server_state));

    // Open socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        printf("Error opening socket for TCP server...\n");
        return -1;
    }

    // Bind the address and port to the socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Error binding socket for TCP server...\n");
        return -1;
    }

    // Mark server fd as ready to accept connections
    if (listen(server_fd, 1) < 0)
    {
        printf("Error listening for clients\n");
        return -1;
    }

    // Initialize polling fields for server
    server_state->server_fd = server_fd;
    server_state->pollfds[0].fd = server_fd;
    server_state->pollfds[0].events = POLLIN | POLLPRI;

    return server_fd;
}

int onClientConnect(struct ServerState *server_state)
{
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_fd = accept(server_state->server_fd, (struct sockaddr *)&client_addr, &addr_size);
    if (client_fd < 0)
    {
        printf("Error accepting client...\n");
        return -1;
    }

    // Search for a pollfd to assign this client into
    for (int idx = 1; idx <= MAX_CLIENTS; idx++)
    {
        if (server_state->pollfds[idx].fd == 0)
        {
            server_state->pollfds[idx].fd = client_fd;
            server_state->pollfds[idx].events = POLLIN | POLLPRI;
            server_state->num_clients++;
            printf("Client %d connected: %s:%u\n", idx, inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            break;
        }
    }

    return 0;
}

void closeClient(struct ServerState *server_state, int idx)
{
    close(server_state->pollfds[idx].fd);
    memset(&server_state->pollfds[idx], 0, sizeof(struct pollfd));
    server_state->num_clients--;
}

void onClientDisconnect(struct ServerState *server_state, int idx)
{
    printf("Client %d disconnected\n", idx);
    closeClient(server_state, idx);
}

int serveNewConnections(struct ServerState *server_state)
{
    if (server_state->pollfds[0].revents & POLLIN)
    {
        if (onClientConnect(server_state) < 0)
        {
            return -1;
        }
    }

    return 0;
}

void processClientMessage(int client_fd, char *buffer, int len)
{
    // TODO: Check for valid HTTP structure, and respond accordingly.
    // For now just echo back
    send(client_fd, buffer, len, 0);
}

void serveClients(struct ServerState *server_state)
{
    for (int idx = 1; idx <= MAX_CLIENTS; idx++)
    {
        if (server_state->pollfds[idx].fd > 0 && server_state->pollfds[idx].revents & POLLIN)
        {
            int len = read(server_state->pollfds[idx].fd, server_state->buffer, sizeof(server_state->buffer) - 1);
            if (len <= 0) // Error (-1) or Disconnect (0)
            {
                onClientDisconnect(server_state, idx);
            }
            else
            {
                processClientMessage(server_state->pollfds[idx].fd, server_state->buffer, len);
            }
        }
    }
}

void shutdownServer(struct ServerState *server_state)
{
    // Error or SIGINT received: clean up and close
    printf("Exiting...\n");

    // Close open clients
    for (int idx = 1; idx <= MAX_CLIENTS; idx++)
    {
        if (server_state->pollfds[idx].fd > 0)
        {
            printf("Closing client %d...\n", idx - 1);
            closeClient(server_state, idx);
        }
    }
    printf("Shutting down server...\n");
    close(server_state->server_fd);
}