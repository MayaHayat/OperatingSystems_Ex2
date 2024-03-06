#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024


void handle_get(int client_socket, char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        send(client_socket, "404 FILE NOT FOUND\r\n\r\n", strlen("404 FILE NOT FOUND\r\n\r\n"), 0);
    } else {
        send(client_socket, "200 OK\r\n", strlen("200 OK\r\n"), 0);
        
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
            printf("Client requested get -> sending\n");
        }
        
        fclose(file);
    }
}



void handle_post(int client_socket, const char *full_path, char* token) {
    struct flock lock;
    lock.l_type = F_WRLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    FILE *file = fopen(full_path, "w");
    if (!file) {
        perror("Error opening file");
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(client_socket);
        exit(1);
    }

    // Trying to lock file
    if (fcntl(fileno(file), F_SETLK, &lock) == -1) {
        perror("Error locking file");
        fclose(file);
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(client_socket);
        exit(1);
    }

    // Into the new file
    while (1) {
        token = strtok(NULL, "\r\n");
        if (!token)
            break;
        fprintf(file, "%s", token);
    }

    printf("Finished writing to file\n");
    fflush(stdout);

    // Try unlocking the file
    lock.l_type = F_UNLCK;
    if (fcntl(fileno(file), F_SETLK, &lock) == -1) {
        perror("Error unlocking");
        fclose(file);
        close(client_socket);
        exit(1);
    }

    fclose(file);

    send(client_socket, "200 OK\r\n\r\n", strlen("200 OK\r\n\r\n"), 0);
}




void handle_client(int client_socket, char *root_dir) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    usleep(70000);

    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("Error receiving request");
        close(client_socket);
        exit(1);
    }



    char *token;
    char *method, *path;
    token = strtok(buffer, "\r\n");

    if (token) {
        sscanf(token, "%ms %ms", &method, &path);
    } else {
        send(client_socket, "400 Bad Request\r\n\r\n", strlen("400 Bad Request\r\n\r\n"), 0);
        close(client_socket);
        exit(1);
    }

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", root_dir, path);

    if (strcmp(method, "GET") == 0) {
        handle_get(client_socket, full_path);
    } else if (strcmp(method, "POST") == 0) {
        handle_post(client_socket, full_path, token);
    }

    free(method);
    free(path);
    close(client_socket);
    exit(0);
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        perror("You're missing the path to your folder!");
        exit(1);
    }

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    char *root_directory = argv[1];


    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket");
        exit(1);
    }

    // Initialize server address
    server_address.sin_family = AF_INET; // ipV4 address
    server_address.sin_addr.s_addr = INADDR_ANY; // socket will be bound to all available network interfaces
    server_address.sin_port = htons(8080); // will bind to socket 8080

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Binding");
        exit(1);
    }

    if (listen(server_socket, 5) == -1) { // allow max 5 clients at the same time
        perror("Listening (error)");
        exit(1);
    }
    printf("Server is listening on 8080!\n");

    socklen_t client_length = sizeof(client_address);


    int client_socket;

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length)) == -1) {
            perror("Error accepting");
            exit(1);
        }

        pid_t child_id = fork();
        if (child_id == -1) {
            perror("Failed forking");
            close(client_socket);
            close(server_socket);
            exit(1);
        } else if (child_id == 0) {
            // child process
            close(server_socket);
            handle_client(client_socket, root_directory);
            close(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
