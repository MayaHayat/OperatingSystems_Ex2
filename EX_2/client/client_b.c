#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>



#define BUFFER_SIZE 204800
#define CLIENT_FILES "myFiles/"




int base64_decode(const char *input, size_t length_coded, char **output) {
    BIO *bio, *b64;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(input, length_coded);
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    size_t max_len = (length_coded * 3) / 4 + 1; 
    *output = (char *)malloc(max_len);

    
    int len_normal = BIO_read(bio, *output, length_coded); 
    (*output)[len_normal] = '\0'; 
    BIO_free_all(bio);
    return len_normal;
}


int handle_download(char* path2list, int client_socket) {
    char response[BUFFER_SIZE];
  
    ssize_t len;
    if ((len = recv(client_socket, response, sizeof(response), 0)) == -1) {
        perror("Had a receiving issue!\n");
        return -1;
    } 
    else {
        printf("%s \n", response);
        char *token = strtok(response, "\r\n");
        token = strtok(NULL, "\r\n");

        char *client_path = malloc(strlen(CLIENT_FILES) + strlen(path2list) + 1);
        if(!client_path){
            perror("Couldn't allocate space for path");
            exit(1);
        }
        snprintf(client_path, strlen(CLIENT_FILES) + strlen(path2list) + 1, "%s%s", CLIENT_FILES, path2list);


        char *normal;
        int len_normal = base64_decode(token, strlen(token), &normal);

        
        FILE *file = fopen(client_path, "wb");
        fwrite(normal, 1, len_normal, file);
        fclose(file);

        free(normal); 
        free(client_path);
    }
    return 0;
}



int handle_select(int *sockets, char **file_array, int num_files) {
    fd_set readfdss;
    // Reset so we won't have random numbers
    FD_ZERO(&readfdss); 
    int fdmax = 0;

    for (int i = 0; i < num_files; i++) {
        // Add the socket to the file desctiptor list
        FD_SET(sockets[i], &readfdss);
        if (sockets[i] > fdmax) {
            fdmax = sockets[i];
        }
    }

    // Wait for all files
    usleep(700000);
    

    if (select(fdmax + 1, &readfdss, NULL, NULL, NULL) == -1) {
        perror("select");
        return -1;
    }

    for (int i = 0; i < num_files; i++) {
        if (FD_ISSET(sockets[i], &readfdss)) {
            int ans = handle_download(file_array[i], sockets[i]);
            if (ans == -1) {
                fprintf(stderr, "Couldn't download file %s\n", file_array[i]);
            
                }
            }
    }
    printf("\n%d files were downloaded from server!\nGoodbye :)\n", num_files);
    return 0;

}

int extract_file_names(const char *response, char ***file_array) {
    int num_files = 0;
    char *curr = strtok((char*)response, "\r\n");
    *file_array = NULL;
    while (curr) {
        curr = strtok(NULL, "\r\n");
        if (curr) {
            *file_array = (char **)realloc(*file_array, (num_files + 1) * sizeof(char *));
            if (!*file_array) {
                perror("Failed to allocate space for file_array array");
                return -1;
            }

            char *temp = strdup(curr);
            if (!temp) {
                perror("Failed to duplicate string");
                return -1;
            }
            (*file_array)[num_files] = temp;
            num_files++;
        }
    }
    return num_files;
}


int ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return -1;  // Suffix is longer than the string
    }

    const char *str_suffix = str + (str_len - suffix_len);

    return strcmp(str_suffix, suffix) == 0 ? 0 : -1;
}



// --------------------------------------------------------------------------------------------



int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Please provide a file list");
        exit(1);
    }

    const char *server_address = "127.0.0.1";
    const char *server_port = "8080";
    char *mylist = argv[1];

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket");
        exit(1);
    }

    struct addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_address, server_port, &hints, &server_info) != 0) {
        perror("getaddrinfo error");
        close(client_socket);
        exit(1);
    }

    struct sockaddr_in *server_addr = (struct sockaddr_in *)server_info->ai_addr;
    if (connect(client_socket, (struct sockaddr *)server_addr, sizeof(*server_addr)) == -1) {
        perror("Connect error");
        close(client_socket);
        freeaddrinfo(server_info);
        exit(1);
    }

    if (ends_with(mylist, ".list") == -1) {
        int ans = handle_download(mylist, client_socket);
        if (ans == -1){
            perror("Couldn't download file...");
        }
    } else {
        char *path2list = malloc(strlen(CLIENT_FILES) + strlen(mylist) + 1);
        if (!path2list) {
            perror("Failed to allocate space for path");
            exit(1);
        }

        // copy path2list 
        snprintf(path2list, strlen(CLIENT_FILES) + strlen(mylist) + 1, "%s%s",CLIENT_FILES, mylist);

        char request[BUFFER_SIZE];
        snprintf(request, sizeof(request), "GET /Downloads/%s \r\n\r\n", mylist);

        // sending request to server with Get (so server can handle it as implemented)
        send(client_socket, request, strlen(request),0);

        // Wait till all files are received
        usleep(700000); 

        
        char response[BUFFER_SIZE];
        ssize_t len = recv(client_socket, response, sizeof(response), 0);
        if (len == -1) {
            perror("Didn't get server's response!");
            close(client_socket); 
        } else {
            response[len] = '\0';
            close(client_socket); 

            char **file_array = NULL;
            //Use the help function to create an array of all the files so they can later be downloaded
            int num_files = extract_file_names(response, &file_array);
            if (num_files == -1){
                perror("Couldn't get files to download from .list file...");
                exit(1);
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));

            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(server_address);
            server_addr.sin_port = htons(atoi(server_port));


            int *sockets = (int *) malloc(sizeof(int) * num_files);
            if (!sockets){
                perror("Failed to allocate space for sockets");
                exit(1);
            }

            int fdmax = 0;
            
            // Create a socket for each cell in the array and update max fd
            // Connect all sockets
            for (int i = 0; i < num_files; i++) {
                sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (sockets[i] == -1) {
                    perror("socket issue in socket array!");
                    exit(1);
                }
                if (sockets[i] > fdmax)
                    fdmax = sockets[i];

                if (connect(sockets[i], (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
                    perror("connection issue in socket array!");
                    exit(1);
                }
            }

            for (int i = 0; i < num_files; i++) {
                char request[BUFFER_SIZE];
                snprintf(request, sizeof(request), "GET /Downloads/%s \r\n\r\n", file_array[i]);
                // Send a request to get each file seperatly from server
                send(sockets[i], request, strlen(request),0);
            }


            int select_ans = handle_select(sockets, file_array, num_files);
            if (select_ans == -1){
                perror("An error has occured in select");
                exit(1);
            }


            // Free all 
            for (int i = 0; i < num_files; i++) {
                free(file_array[i]); 
            }
            free(file_array); 
            free(sockets);
            free(path2list);
        }
    }

    close(client_socket);

    return 0;
}

