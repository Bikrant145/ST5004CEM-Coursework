/*
 * ST5004CEM - Task 4: Network Programming and IPC
 * task4_client.c
 *
 * Connects to task4_server, authenticates, then lets the user send
 * messages that the server echoes back. Type QUIT to disconnect.
 *
 * Compile:  gcc task4_client.c -o task4_client
 * Run:      ./task4_client [server_ip] [port]
 *            (defaults to 127.0.0.1 and port 8080)
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
 
#define BUFFER_SIZE 512
#define DEFAULT_PORT 8080
 
int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
 
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
 
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
 
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
 
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection to server failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
 
    printf("Connected to server %s:%d\n", server_ip, port);
 
    char username[64], password[64];
    printf("Username: ");
    scanf("%63s", username);
    printf("Password: ");
    scanf("%63s", password);
 
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "AUTH %s %s\n", username, password);
    send(sock_fd, buffer, strlen(buffer), 0);
 
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        printf("Server closed the connection during authentication.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server: %s", buffer);
 
    if (strncmp(buffer, "AUTH_OK", 7) != 0) {
        printf("Authentication failed. Disconnecting.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
 
    printf("Authenticated. Type messages to send (QUIT to exit):\n");
 
    char input[BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (scanf(" %[^\n]", input) != 1) {
            break;
        }
 
        char out[BUFFER_SIZE + 8];
        if (strcmp(input, "QUIT") == 0) {
            snprintf(out, sizeof(out), "QUIT\n");
        } else {
            snprintf(out, sizeof(out), "MSG %s\n", input);
        }
        send(sock_fd, out, strlen(out), 0);
 
        memset(buffer, 0, sizeof(buffer));
        n = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            printf("Server closed the connection.\n");
            break;
        }
        printf("Server: %s", buffer);
 
        if (strcmp(input, "QUIT") == 0) {
            break;
        }
    }
 
    close(sock_fd);
    return 0;
}
 
