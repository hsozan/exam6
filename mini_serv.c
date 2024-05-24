#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

int client[4096] = {-1}; // Array to store client IDs, initialized with -1
char *messages[4096]; // Array to store client messages
fd_set current, write_set, read_set; // File descriptor sets for active, write, and read file descriptors
int serverfd; // File descriptor for the server socket

void ft_error(char *str)
{
    write(2, str, strlen(str)); // Write the error message to the standard error stream
    if (serverfd > 0)
        close(serverfd); // Close the server socket if it is open
    exit(1); // Exit the program with a non-zero status code
}

void ft_send(int fd, int maxfd, char *str)
{
    for (int i = maxfd; i > serverfd; i--) // Iterate over all connected clients
        if (client[i] != -1 && i != fd && FD_ISSET(i, &write_set)) // Check if the client is connected and ready for writing
            send(i, str, strlen(str), 0); // Send the message to the client
}

int extract_message(char **buf, char **msg)
{
    char *newbuf;
    int i;

    *msg = 0;
    if (*buf == 0)
        return (0);
    i = 0;
    while ((*buf)[i])
    {
        if ((*buf)[i] == '\n')
        {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            if (newbuf == 0)
                return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
}

char *str_join(char *buf, char *add)
{
    char *newbuf;
    int len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return (0);
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}

int main(int ac, char **av)
{
    if (ac != 2) // Check if the number of arguments is not equal to 2
        ft_error("Wrong number of arguments\n"); // Display an error message and exit

    serverfd = socket(AF_INET, SOCK_STREAM, 0); // Create a socket
    if (serverfd == -1) // Check if the socket creation failed
        ft_error("Fatal error\n"); // Display an error message and exit

    struct sockaddr_in servaddr = {0}; // Create a sockaddr_in structure
    servaddr.sin_family = AF_INET; // Set the address family to AF_INET
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Set the IP address to localhost (127.0.0.1)
    servaddr.sin_port = htons(atoi(av[1])); // Set the port number from the command line argument

    if ((bind(serverfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) // Bind the socket to the specified IP address and port number
        ft_error("Fatal error\n"); // Display an error message and exit
    if (listen(serverfd, 128) != 0) // Start listening for incoming connections on the socket
        ft_error("Fatal error\n"); // Display an error message and exit
    FD_ZERO(&current); // Clear the active file descriptor set
    FD_SET(serverfd, &current); // Add the server socket to the active file descriptor set
    int maxfd = serverfd; // Set the maximum file descriptor to the server socket
    int id = 0; // Initialize the client ID counter
    while (1) // Start an infinite loop for handling client connections
    {
        read_set = write_set = current; // Copy the active file descriptor set to the read and write file descriptor sets
        if (select(maxfd + 1, &read_set, &write_set, NULL, NULL) < 0) // Wait for activity on the file descriptors
            continue; // Continue to the next iteration if select() returns an error
        if (FD_ISSET(serverfd, &read_set)) // Check if there is activity on the server socket
        {
            int clientfd = accept(serverfd, NULL, NULL); // Accept a new client connection
            if (clientfd < 0) // Check if the client connection failed
                continue; // Continue to the next iteration if accept() returns an error
            FD_SET(clientfd, &current); // Add the client socket to the active file descriptor set
            char buf[64]; // Create a buffer for storing the client arrival message
            sprintf(buf, "server: client %d just arrived\n", id); // Format the client arrival message
            client[clientfd] = id++; // Assign a client ID to the new client
            messages[clientfd] = calloc(1,1); // Allocate memory for storing client messages
            maxfd = (clientfd > maxfd) ? clientfd: maxfd; // Update the maximum file descriptor if necessary
            ft_send(clientfd, maxfd, buf); // Send the client arrival message to all connected clients
        }
        for (int fd = maxfd; fd > serverfd; fd--) // Iterate over all connected clients
        {
            if (FD_ISSET(fd, &read_set)) // Check if there is activity on the client socket
            {
                char buffer[4095]; // Create a buffer for receiving client messages
                int bytes = recv(fd, buffer, 4094, 0); // Receive a client message
                if (bytes <= 0) // Check if the client connection was closed
                {
                    FD_CLR(fd, &current); // Remove the client socket from the active file descriptor set
                    char buf[64]; // Create a buffer for storing the client departure message
                    sprintf(buf, "server: client %d just left\n", client[fd]); // Format the client departure message
                    ft_send(fd, maxfd, buf); // Send the client departure message to all connected clients
                    client[fd] = -1; // Mark the client as disconnected
                    free(messages[fd]); // Free the memory allocated for storing client messages
                    close(fd); // Close the client socket
                }
                else
                {
                    buffer[bytes] = 0; // Null-terminate the received message
                    messages[fd] = str_join(messages[fd], buffer); // Append the received message to the client's message buffer
                    char *msg; // Pointer to the extracted message
                    while (extract_message(&messages[fd], &msg)) // Extract complete messages from the client's message buffer
                    {
                        char buf[64 + strlen(msg)]; // Create a buffer for storing the formatted message
                        sprintf(buf, "client %d: %s", client[fd], msg); // Format the client message
                        ft_send(fd, maxfd, buf); // Send the client message to all connected clients
                    }
                }
            }
        }
    }
    return 0;    
}
