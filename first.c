#include <stdio.h>                          // Include the standard input/output library
#include <stdlib.h>                         // Include the standard library
#include <unistd.h>                         // Include the POSIX operating system API library
#include <string.h>                         // Include the string manipulation library
#include <sys/socket.h>                     // Include the socket programming library
#include <netinet/in.h>                     // Include the internet protocol library

typedef struct s_client                     // Define a structure for a client
{
    int     id;                             // Client ID
    char    msg[100000];                    // Message buffer for each client
}   t_client;

t_client    clients[1024];                  // Array of clients
fd_set      read_set, write_set, current;   // File descriptor sets for reading, writing, and current state
int         maxfd = 0, gid = 0;              // Maximum file descriptor and global ID counter
char        send_buffer[120000], recv_buffer[120000];   // Buffers for sending and receiving data

void    err(char  *msg)                     // Function to handle errors
{
    if (msg)
        write(2, msg, strlen(msg));         // Write error message to stderr
    else
        write(2, "Fatal error", 11);         // Write default error message to stderr
    write(2, "\n", 1);                       // Write newline character to stderr
    exit(1);                                // Exit the program with error status
}

void    send_to_all(int except)              // Function to send data to all clients except one
{
    for (int fd = 0; fd <= maxfd; fd++)      // Iterate through all file descriptors
    {
        if  (FD_ISSET(fd, &write_set) && fd != except)   // Check if the file descriptor is ready for writing and not the exception
            if (send(fd, send_buffer, strlen(send_buffer), 0) == -1)   // Send data to the client
                err(NULL);                   // Handle send error
    }
}

int     main(int ac, char **av)              // Main function
{
    if (ac != 2)
        err("Wrong number of arguments");   // Check if the number of arguments is correct

    struct sockaddr_in  serveraddr;          // Structure for server address
    socklen_t           len;                 

    int serverfd = socket(AF_INET, SOCK_STREAM, 0);   // Create a socket
    if (serverfd == -1) err(NULL);                     // Check for socket creation error
    maxfd = serverfd;

    FD_ZERO(&current);                       // Clear the current file descriptor set
    FD_SET(serverfd, &current);               // Add the server socket to the current set
    bzero(clients, sizeof(clients));         // Clear the clients array
    bzero(&serveraddr, sizeof(serveraddr));   // Clear the server address structure

    serveraddr.sin_family = AF_INET;                    // Set the address family
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);     // Set the IP address
    serveraddr.sin_port = htons(atoi(av[1]));           // Set the port number

    if (bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1)
        err(NULL);                                      // Bind and listen on the server socket

    while (1)                                   // Main server loop
    {
        read_set = write_set = current;         // Update the read and write sets
        if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) continue;   // Wait for activity on the file descriptors

        for (int fd = 0; fd <= maxfd; fd++)     // Iterate through all file descriptors
        {
            if (FD_ISSET(fd, &read_set))        // Check if the file descriptor is ready for reading
            {
                if (fd == serverfd)              // Check if it is the server socket
                {
                    int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);   // Accept a new client connection
                    if (clientfd == -1) continue;       // Check for accept error
                    if (clientfd > maxfd) maxfd = clientfd;   // Update the maximum file descriptor
                    clients[clientfd].id = gid++;       // Assign a unique ID to the client
                    FD_SET(clientfd, &current);         // Add the client socket to the current set
                    sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd);              // Send a message to all clients
                    break;
                }
                else
                {
                    int ret = recv(fd, recv_buffer, sizeof(recv_buffer), 0);   // Receive data from a client
                    if (ret <= 0)
                    {
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd);                // Send a message to all clients
                        FD_CLR(fd, &current);           // Remove the client socket from the current set
                        close(fd);                      // Close the client socket
                        break;
                    }
                    else
                    {
                        for (int i = 0, j = strlen(clients[fd].msg); i < ret; i++, j++)
                        {
                            clients[fd].msg[j] = recv_buffer[i];   // Store the received data in the client's message buffer
                            if (clients[fd].msg[j] == '\n')
                            {
                                clients[fd].msg[j] = '\0';   // Null-terminate the message
                                sprintf(send_buffer, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                                send_to_all(fd);            // Send a message to all clients
                                bzero(clients[fd].msg, strlen(clients[fd].msg));   // Clear the client's message buffer
                                j = -1;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    return (0);                                 // Return 0 to indicate successful execution
}