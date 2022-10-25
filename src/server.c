#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>  // signal
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "jrb.h"
#include "jval.h"
#include "utils.h"

#define MAX_THREADS 10
#define MAX_ROOMS 100
#define MAX_CLIENTS 100

/* - defined in utils.h
typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char *username;
} * Client;  // client is poiter
*/

// Tránh xung đột tranh dành ghi dữ liệu
// (Kiểu 2 thàng cùng cli_count++ (0 -> 1 trong khi có 2 đứa))
static _Atomic unsigned int cli_count = 0;
static int client_id = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
JRB clients;

void *handle_client(void *arg);
void send_message(char *s, int uid);
void print_client_addr(struct sockaddr_in addr);

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *IP = "127.0.0.1";
    int PORT = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t thread_id;

    clients = make_jrb();

    /*Socket settings*/
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    serv_addr.sin_port = htons(PORT);

    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    /*Tránh được lỗi "Address / Port already in use"*/
    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0) {
        PRINT_ERROR;
        printf("ERROR: setsockopt failed\n");
        return EXIT_FAILURE;
    }

    /* Bind */
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        PRINT_ERROR;
        return EXIT_FAILURE;
    }

    /* Listen request from client */
    if (listen(listenfd, MAX_THREADS) < 0) {
        PRINT_ERROR;
        printf("Over max threads\n");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");

    /*Communicate with client*/
    while (1) {
        socklen_t clientAddrlen = sizeof(cli_addr);
        /*Accept*/
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clientAddrlen);

        /* Check if max clients is reached */
        if ((cli_count + 1) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        /* Client settings */
        Client client = (Client) malloc(sizeof(struct client));
        client->addr = cli_addr;
        client->sockfd = connfd;
        client->id = client_id++;

        /* Add client to List and fork thread*/
        jrb_insert_int(clients, client->id, new_jval_v(client));
        JRB node;
        int i = 0;
        jrb_traverse(node, clients) {
            Client cli = (Client) jval_v(node->val);
            printf("%d %d\n",i ,cli->id);
        }
        pthread_create(&thread_id, NULL, handle_client, client);
        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}

void *handle_client(void *arg) {
    Client cli = (Client) arg;  // client - is a poiter
    char buff_out[BUFF_SIZE + CLIENT_NAME_LEN + 3];
    char name[CLIENT_NAME_LEN];
    int leave_flag = 0;

    cli_count++;

    // receive name
    if (recv(cli->sockfd, name, CLIENT_NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= CLIENT_NAME_LEN - 1) {
        printf("Didn't enter the name. %s\n", name);
        leave_flag = 1;
    } else {
        strcpy(cli->username, name);
        sprintf(buff_out, "%s has joined", cli->username);
        printf("%s\n", buff_out);
    }

    bzero(buff_out, sizeof(buff_out));

    while (1) {
        if (leave_flag == 1) break;

        int received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
        /*
            continue read if have
        */
        // printf("received: %d; buffsize:  %ld\n", received, strlen(buff_out));
        if (received > 0) {
            if (strlen(buff_out) > 0) {
                send_message(buff_out, cli->id);
                printf("%s -> %s\n", buff_out, cli->username);
            } else if (received == 0 || strcmp(buff_out, "exit") == 0) {
                sprintf(buff_out, "%s has left", cli->username);
                printf("%s\n", buff_out);
                send_message(buff_out, cli->id);
                leave_flag = 1;
            } else {
                PRINT_ERROR;
                leave_flag = 1;
            }

            bzero(buff_out, sizeof(buff_out));

        }  // end if recv no error
    }      // end while - client communicate with server

    /* Delete client from List and yield thread */
    close(cli->sockfd);
    /*
        delete client from List
    */
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

/* Send message to all clients except sender */
void send_message(char *s, int cli_id) {
    pthread_mutex_lock(&clients_mutex);
    JRB node;
    jrb_traverse(node, clients) {
        Client cli = (Client) jval_v(node->val);
        if (cli->id != cli_id) {
            send(cli->sockfd, s, strlen(s), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, 
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16, 
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}