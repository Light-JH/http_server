#pragma once

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Set up TCP server on given port. Return the FD.
int setupServer(int port)
{
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

    return server_fd;
}