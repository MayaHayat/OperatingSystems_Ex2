#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define PATH_SIZE 256


void handle_all(int client_socket, char *root_directory){
    char buffer [BUFFER_SIZE];
    ssize_t received_so_far;
    received_so_far = recv(client_socket, buffer, sizeof(buffer),0);
    if (received_so_far < 0){
        perror("Error receiving sent data");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char *token = strtok(buffer, "\r\n");
    char *method, *my_path;

    if (token != NULL){
        sscanf(token, "%ms %ms", &method, &my_path);
    }
    else{
        perror("Error finding request");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char path[BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s %s", root_directory, my_path);

    if (strcmp(method, "GET") == 0){
        handle_get(client_socket, path);
    }
    else if (strcmp(method, "POST") == 0){
        handle_post(client_socket, path);
    }
}



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
            printf("Sending file");
        }
        
        fclose(file);
    }
}

// Function to handle POST requests
void handle_post(int client_socket, char *path, char *data) {
    int file_descriptor;
    struct flock lock;

    // Open the file with write mode
    file_descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (file_descriptor == -1) {
        // Error opening file, send 500 response
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Set up the lock structure
    lock.l_type = F_WRLCK; // Write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    // Try to acquire the lock
    if (fcntl(file_descriptor, F_SETLK, &lock) == -1) {
        // Lock acquisition failed, send 500 response
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(file_descriptor);
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Write data to file
    write(file_descriptor, data, strlen(data));

    // Release the lock
    lock.l_type = F_UNLCK;
    fcntl(file_descriptor, F_SETLK, &lock);

    // Close the file
    close(file_descriptor);

    // Send 200 OK response
    send(client_socket, "200 OK\r\n\r\n", strlen("200 OK\r\n\r\n"), 0);
}







int main(int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <root_directory>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_length;
    char *root_directory = argv[1];

    int server_socket, client_socket;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0){
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }


    // Initialize server address
    server_address.sin_family = AF_INET; // ipV4 address
    server_address.sin_addr.s_addr = INADDR_ANY; // socket will be bound to all available network interfaces
    server_address.sin_port = htons(8080); // will bind to socket 8080

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0){ // allow max 5 clients at the same time
        perror("Error listening");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on 8080!\n");

    client_length = sizeof(client_address);

    client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length);
    if (client_socket < 0 ){
        perror("Error accepting");
        exit(EXIT_FAILURE);
    }
    while (1){
        pid_t child_id;
        child_id = fork();
        if (child_id == -1){
            perror("Failed forking");
            close(client_socket);
            close(server_socket);
            exit(EXIT_FAILURE);
        }
        else if (child_id == 0){
            // child procces
            close(server_socket);
            handle_all(client_socket, root_directory);
            exit(EXIT_SUCCESS);
        }
        else{
            close(client_socket);
        }
    }
    close(server_socket);
    return 0;
    
}