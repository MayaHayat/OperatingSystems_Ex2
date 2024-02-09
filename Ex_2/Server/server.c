#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PATH_SIZE 256

// Function to handle GET requests
void handle_get(int client_socket, char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        // File not found, send 404 response
        send(client_socket, "404 FILE NOT FOUND\r\n\r\n", strlen("404 FILE NOT FOUND\r\n\r\n"), 0);
    } else {
        // File found, send 200 OK response
        send(client_socket, "200 OK\r\n", strlen("200 OK\r\n"), 0);
        
        // Read file contents
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
        }
        
        fclose(file);
    }
}

// Function to handle POST requests
void handle_post(int client_socket, char *path, char *data) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        // Error opening file, send 500 response
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
    } else {
        // Write data to file
        fwrite(data, strlen(data), 1, file);
        fclose(file);
        
        // Send 200 OK response
        send(client_socket, "200 OK\r\n\r\n", strlen("200 OK\r\n\r\n"), 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <root_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_socket;
    int client_socket;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 8080...\n");

    while (1) {
        // Accept incoming connection
        client_address_len = sizeof(client_address);
        // client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        printf("Before accept ----------------\n");
        fflush(stdout);
        client_socket = accept(server_socket, NULL, NULL);
        printf("running ----------------\n");
        fflush(stdout);

        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        // Fork a child process
        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("Error forking process");
            close(client_socket);
            continue;
        }

        if (child_pid == 0) {
            // Child process
            close(server_socket); // Close server socket in child process
            
            //usleep(10000);

            // Receive request
            char request[BUFFER_SIZE];
            ssize_t bytes_received = recv(client_socket, request, BUFFER_SIZE, 0);
            if (bytes_received == -1) {
                perror("Error receiving request");
                close(client_socket);
                exit(EXIT_FAILURE);
            }

            // Extract method and path from request
            char method[10], path[1000], data[1000];
            sscanf(request, "%s %s %s", method, path, data);

            // Handle request
            if (strcmp(method, "GET") == 0) {
                handle_get(client_socket, path);
            } else if (strcmp(method, "POST") == 0) {
                handle_post(client_socket, path, data);
            }

            // Close connection
            close(client_socket);
            printf("Connection closed\n");
            exit(EXIT_SUCCESS); // Terminate child process
        } else {
            // Parent process
            close(client_socket); // Close client socket in parent process
        }
    }

    close(server_socket);

    return 0;
}