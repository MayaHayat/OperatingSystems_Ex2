#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PATH_SIZE 1000
#define DATA_SIZE 1000

void send_response(int client_socket, const char *response) {
    send(client_socket, response, strlen(response), 0);
}

void handle_get(int client_socket, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        send_response(client_socket, "404 FILE NOT FOUND\r\n\r\n");
    } else {
        send_response(client_socket, "200 OK\r\n");
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
        }
        fclose(file);
    }
}

void handle_post(int client_socket, const char *path, const char *data) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        send_response(client_socket, "500 INTERNAL ERROR\r\n\r\n");
    } else {
        fwrite(data, strlen(data), 1, file);
        fclose(file);
        send_response(client_socket, "200 OK\r\n\r\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <root_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 8080...\n");

    while (1) {
        client_address_len = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("Error forking process");
            close(client_socket);
            continue;
        }

        if (child_pid == 0) {
            // Child process
            close(server_socket);

            usleep(10000);

            char request[BUFFER_SIZE];
            ssize_t bytes_received = recv(client_socket, request, BUFFER_SIZE, 0);
            if (bytes_received == -1) {
                perror("Error receiving request");
                close(client_socket);
                exit(EXIT_FAILURE);
            }

            char method[10], path[PATH_SIZE], data[DATA_SIZE];
            sscanf(request, "%s %s %s", method, path, data);

            if (strcmp(method, "GET") == 0) {
                handle_get(client_socket, path);
            } else if (strcmp(method, "POST") == 0) {
                handle_post(client_socket, path, data);
            }

            close(client_socket);
            printf("Connection closed\n");
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
