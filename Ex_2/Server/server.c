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
    if (file == NULL) {
        send(client_socket, "404 FILE NOT FOUND\r\n\r\n", strlen("404 FILE NOT FOUND\r\n\r\n"), 0);
    } else {
        send(client_socket, "200 OK\r\n", strlen("200 OK\r\n"), 0);
        
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
            printf("Sending file");
        }
        
        fclose(file);
    }
}




// void handle_post(int client_socket, const char *full_path, char* token) {
//     struct flock lock;
//     lock.l_type = F_WRLCK; 
//     lock.l_whence = SEEK_SET;
//     lock.l_start = 0;
//     lock.l_len = 0;

//     int file = open(full_path, O_WRONLY | O_CREAT | O_TRUNC);
//     if (file < 0) {
//         perror("Error opening file");
//         send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"),0);
//         close(client_socket);
//         exit(EXIT_FAILURE);
//     }

//     // Trying to lock file
//     if (fcntl(file, F_SETLK, &lock) == -1) {
//         perror("Error locking file");
//         close(file);
//         send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"),0);
//         close(client_socket);
//         exit(EXIT_FAILURE);
//     }

//     char buffer[BUFFER_SIZE];
//     ssize_t bytes_received;

//     while (1) {
//             token = strtok(NULL, "\r\n");
//             if(token == NULL)
//                 break;
//             printf("token contains <<<<%s>>>>\n", token);

//             if (write(file, token, strlen(token)) < 0) {
//                 perror("Error writing to file");
//                 close(file);
//                 close(client_socket);
//                 exit(EXIT_FAILURE);
//             }
//         }


//     printf("Finished writing to file\n");
//     fflush(stdout);

//     // Try unlocking the file
//     lock.l_type = F_UNLCK;
//     if (fcntl(file, F_SETLK, &lock) == -1) {
//         perror("Error unlocking");
//         close(file);
//         close(client_socket);
//         exit(EXIT_FAILURE);
//     }

//     close(file);

//     send(client_socket, "200 OK\r\n\r\n", strlen("200 OK\r\n\r\n"), 0);
// }

void handle_post(int client_socket, const char *full_path, char* token) {
    struct flock lock;
    lock.l_type = F_WRLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    FILE *file = fopen(full_path, "w");
    if (file == NULL) {
        perror("Error opening file");
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Trying to lock file
    if (fcntl(fileno(file), F_SETLK, &lock) == -1) {
        perror("Error locking file");
        fclose(file);
        send(client_socket, "500 INTERNAL ERROR\r\n\r\n", strlen("500 INTERNAL ERROR\r\n\r\n"), 0);
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Writing tokens into the sent file
    while (1) {
        token = strtok(NULL, "\r\n");
        if (token == NULL)
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
        exit(EXIT_FAILURE);
    }

    fclose(file);

    send(client_socket, "200 OK\r\n\r\n", strlen("200 OK\r\n\r\n"), 0);
}




void handle_client(int client_socket, char *root_dir) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    usleep(100000);

    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("Error receiving request");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    //printf("buffer contains <<<<%.*s>>>>\n", (int)bytes_received, buffer);


    char *token;
    char *method, *path;
    token = strtok(buffer, "\r\n");


    if (token != NULL) {
        sscanf(token, "%ms %ms", &method, &path);
    } else {
        send(client_socket, "400 Bad Request\r\n\r\n", strlen("400 Bad Request\r\n\r\n"), 0);
        close(client_socket);
        exit(EXIT_FAILURE);
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
    exit(EXIT_SUCCESS);
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <root_directory>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_length;
    char *root_directory = argv[1];

    int server_socket, client_socket;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    server_address.sin_family = AF_INET; // ipV4 address
    server_address.sin_addr.s_addr = INADDR_ANY; // socket will be bound to all available network interfaces
    server_address.sin_port = htons(8080); // will bind to socket 8080

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) { // allow max 5 clients at the same time
        perror("Error listening");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on 8080!\n");

    client_length = sizeof(client_address);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length);
        if (client_socket < 0) {
            perror("Error accepting");
            exit(EXIT_FAILURE);
        }

        pid_t child_id = fork();
        if (child_id == -1) {
            perror("Failed forking");
            close(client_socket);
            close(server_socket);
            exit(EXIT_FAILURE);
        } else if (child_id == 0) {
            // child process
            close(server_socket);
            handle_client(client_socket, root_directory);
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
