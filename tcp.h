#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "http.h"

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

void executeCommand(char *command, char *output_buffer, int len)
{
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        return;
    }

    while (fgets(output_buffer, len, fp) != NULL)
    {
        printf("line: %s", output_buffer);
        int slen = strlen(output_buffer);
        len -= slen;
        output_buffer += slen;
    }

    pclose(fp);
}

void processClientMessage(int client_fd, char *buffer, int len)
{
    struct HttpCheckResult http_check_result;
    checkHttpRequest(buffer, len, &http_check_result);

    char response_buffer[65536];
    memset(response_buffer, 0, sizeof(response_buffer));

    if (http_check_result.code == 400)
    {
        snprintf(response_buffer, sizeof(response_buffer), "HTTP/1.1 400 Bad Request\r\n");
        send(client_fd, response_buffer, strlen(response_buffer), 0);
    }
    else if (http_check_result.code == 404)
    {
        snprintf(response_buffer, sizeof(response_buffer), "HTTP/1.1 404 Not Found\r\n");
        send(client_fd, response_buffer, strlen(response_buffer), 0);
    }
    else if (http_check_result.code == 200)
    {
        int written_len = snprintf(response_buffer, sizeof(response_buffer), "HTTP/1.1 200 OK\r\n\r\n");
        char *command = http_check_result.url + strlen("/exec/");

        // Execute the command
        executeCommand(command, response_buffer + written_len, sizeof(response_buffer) - written_len * sizeof(char));
        send(client_fd, response_buffer, strlen(response_buffer), 0);
    }
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
                closeClient(server_state, idx); // close the client - should actually check for keep-alive?
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

int serverTick(struct ServerState *server_state)
{
    // Poll for timeout
    int poll_result = poll(server_state->pollfds, server_state->num_clients + 1, 100);
    if (poll_result > 0)
    {
        if (serveNewConnections(server_state) < 0)
        {
            return -1;
        }

        // Serve clients
        serveClients(server_state);
    }

    return 0;
}