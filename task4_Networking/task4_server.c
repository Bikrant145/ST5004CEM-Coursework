/*
 * ST5004CEM - Task 4: Network Programming and IPC
 * task4_server.c
 *
 * A multi-threaded TCP server demonstrating:
 *   1. Socket-based client-server communication
 *   2. A simple line-based text protocol (AUTH, MSG, QUIT)
 *   3. Handling multiple concurrent clients (one thread per connection)
 *   4. Basic security: authentication required before other commands,
 *      and input validation (length limits, disconnect handling)
 *   5. Error handling and connection management
 *
 * Compile:  gcc task4_server.c -o task4_server -lpthread
 * Run:      ./task4_server [port]      (default port 8080)
 *
 * Protocol (see PROTOCOL.md for full documentation):
 *   Client -> Server: "AUTH <username> <password>\n"
 *   Server -> Client: "AUTH_OK\n" or "AUTH_FAIL\n"
 *   Client -> Server: "MSG <text>\n"          (only after AUTH_OK)
 *   Server -> Client: "ECHO: <text>\n"
 *   Client -> Server: "QUIT\n"                closes the connection
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
 
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 512
#define NUM_DEMO_USERS 3
 
typedef struct {
    const char *username;
    const char *password;
} Credential;
 
/* Hardcoded demo credentials - in a production system these would be
   hashed and stored in a file/database, as in Task 3.
   NOTE: NUM_DEMO_USERS must always match the number of entries below -
   a previous version of this code used a larger MAX_USERS than the
   number of initialized entries, which left trailing array slots as
   NULL and caused a crash (strcmp on a NULL pointer) whenever
   check_credentials() looped past the real entries on a failed login. */
Credential valid_users[NUM_DEMO_USERS] = {
    {"admin", "admin123"},
    {"alice", "alice123"},
    {"bob",   "bob123"},
};
 
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
 
void server_log(const char *client_ip, const char *message) {
    pthread_mutex_lock(&log_lock);
    FILE *f = fopen("server.log", "a");
    if (f) {
        fprintf(f, "[client %s] %s\n", client_ip, message);
        fclose(f);
    }
    printf("[client %s] %s\n", client_ip, message);
    pthread_mutex_unlock(&log_lock);
}
 
int check_credentials(const char *username, const char *password) {
    for (int i = 0; i < NUM_DEMO_USERS; i++) {
        if (strcmp(valid_users[i].username, username) == 0 &&
            strcmp(valid_users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}
 
/* Each connected client is handled on its own thread, so the server
   can serve multiple clients concurrently. */
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
 
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    getpeername(client_fd, (struct sockaddr *)&peer_addr, &peer_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer_addr.sin_addr, client_ip, sizeof(client_ip));
 
    server_log(client_ip, "Connected");
 
    char buffer[BUFFER_SIZE];
    int authenticated = 0;
    char authenticated_user[64] = "";
 
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
 
        if (bytes_received <= 0) {
            /* 0 = client closed connection cleanly; <0 = error */
            server_log(client_ip, "Disconnected");
            break;
        }
 
        /* basic input validation: strip trailing newline, reject
           anything suspiciously long or empty */
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (strlen(buffer) == 0) {
            continue;
        }
        if (strlen(buffer) >= BUFFER_SIZE - 1) {
            const char *msg = "ERROR: message too long\n";
            send(client_fd, msg, strlen(msg), 0);
            server_log(client_ip, "Rejected oversized message");
            continue;
        }
 
        char logbuf[BUFFER_SIZE + 32];
 
        if (strncmp(buffer, "AUTH ", 5) == 0) {
            char username[64], password[64];
            if (sscanf(buffer + 5, "%63s %63s", username, password) == 2 &&
                check_credentials(username, password)) {
                authenticated = 1;
                strncpy(authenticated_user, username, sizeof(authenticated_user) - 1);
                send(client_fd, "AUTH_OK\n", 8, 0);
                snprintf(logbuf, sizeof(logbuf), "AUTH success for user '%s'", username);
                server_log(client_ip, logbuf);
            } else {
                send(client_fd, "AUTH_FAIL\n", 10, 0);
                server_log(client_ip, "AUTH failed (bad credentials or malformed request)");
            }
        }
        else if (strncmp(buffer, "MSG ", 4) == 0) {
            if (!authenticated) {
                const char *msg = "ERROR: not authenticated\n";
                send(client_fd, msg, strlen(msg), 0);
                server_log(client_ip, "Rejected MSG - not authenticated");
                continue;
            }
            char response[BUFFER_SIZE + 16];
            snprintf(response, sizeof(response), "ECHO: %s\n", buffer + 4);
            send(client_fd, response, strlen(response), 0);
            snprintf(logbuf, sizeof(logbuf), "MSG from '%s': %s", authenticated_user, buffer + 4);
            server_log(client_ip, logbuf);
        }
        else if (strcmp(buffer, "QUIT") == 0) {
            send(client_fd, "BYE\n", 4, 0);
            server_log(client_ip, "Client requested QUIT");
            break;
        }
        else {
            const char *msg = "ERROR: unknown command\n";
            send(client_fd, msg, strlen(msg), 0);
            server_log(client_ip, "Rejected unknown command");
        }
    }
 
    close(client_fd);
    return NULL;
}
 
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
 
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
 
    /* allow quick restart of the server on the same port during testing */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
 
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
 
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
 
    printf("Server listening on port %d...\n", port);
    printf("Valid demo accounts: admin/admin123, alice/alice123, bob/bob123\n");
 
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
 
        if (client_fd < 0) {
            perror("accept failed");
            continue; /* don't crash the whole server on one bad accept */
        }
 
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
 
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd_ptr) != 0) {
            perror("failed to create thread for client");
            close(client_fd);
            free(client_fd_ptr);
            continue;
        }
        pthread_detach(tid); /* thread cleans up its own resources when done */
    }
 
    close(server_fd);
    return 0;
}
