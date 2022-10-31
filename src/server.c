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
#include "protocol.h"
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
static _Atomic JRB accounts;
static _Atomic JRB clients;
static _Atomic JRB rooms;
static int client_id = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
void send_message(char *s, int uid);
void print_client_addr(struct sockaddr_in addr);

void appendToFile(char *fileName, char *string) {
    FILE *p = fopen(fileName, "a");
    fprintf(p, "\n%s", string);
    fclose(p);
}

int select_room_screen(Client client, char *buff_out);

// return 0 if failed
// return 1 if success
int auth_screen(Client client, char *buff_out) {
    JRB node;
    Account *acc = (Account *) malloc(sizeof(Account));
    int logined_flag = 0;
    int type = 0, exit_flag = 0;
    char buff[BUFF_SIZE] = {0};

    // get type
    sscanf(buff_out, "%d", &type);

    if (type != SIGN_IN && type != SIGN_UP && type != SIGN_OUT) return 0;

    if (type != SIGN_OUT) {
        sscanf(buff_out, "%d %s %s", &type, acc->username, acc->password);
    } else {
        sscanf(buff_out, "%d %s", &type, acc->username);
    }
    printf("%s %s\n", acc->username, acc->password);

    switch (type) {
        case SIGN_IN:
            node = jrb_find_str(accounts, acc->username);

            if (node == NULL) {
                printf("Login failed\n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", FAILED);
                send(client->sockfd, buff, strlen(buff), 0);
                return 0;
            }

            Account * findedAcc = (Account *) jval_v(node->val);
            bzero(buff, sizeof(buff));

            if (strcmp(findedAcc->password, acc->password) != 0) {
                printf("Login failed\n");
                sprintf(buff, "%d", FAILED);
                send(client->sockfd, buff, strlen(buff), 0);
                return 0;
            }

            printf("%s has logined\n", acc->username);
            sprintf(buff, "%d", SUCCESS);
            send(client->sockfd, buff, strlen(buff), 0);
            return 1;
        case SIGN_OUT:
            node = jrb_find_int(clients, client->id);
            if (node == NULL) return 0;

            jrb_delete_node(node);
            // cli_count--; - ở đây ko có quyền xóa count_client 
            return 1;
            break;
        case SIGN_UP:
            node = jrb_find_str(accounts, acc->username);
            bzero(buff, sizeof(buff));

            if (node != NULL) {
                printf("Failed\n");
                sprintf(buff, "%d", FAILED);
                send(client->sockfd, buff, strlen(buff), 0);
                return 0;
            }
            
            jrb_insert_str(accounts, acc->username, new_jval_v(acc));
            sprintf(buff, "%s %s", acc->username, acc->password);   
            appendToFile("account.txt", buff);
            
            bzero(buff, sizeof(buff));
            sprintf(buff, "%d", SUCCESS);
            send(client->sockfd, buff, strlen(buff), 0);
            break;
        default:
            break;
    }

    return 1;
}

int getAccList() {
    FILE *accFile = fopen("account.txt", "rt");
    if (accFile == NULL) return 0;
    while (1) {
        Account *acc = (Account *)malloc(sizeof(Account));
        if (fscanf(accFile, "%s %s %d\n", acc->username, acc->password, &acc->accStatus) == EOF) break;
        jrb_insert_str(accounts, acc->username, new_jval_v(acc));
    }

    fclose(accFile);
    return 1;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    JRB node;  // tree of accounts saved in server
    char *IP = "127.0.0.1";
    int PORT = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t thread_id;
    Account *acc = (Account *)malloc(sizeof(Account));

    clients = make_jrb();
    rooms = make_jrb();
    // get all rooms of logined clients

    // get account list from FIle
    accounts = make_jrb();
    if (getAccList() == 0) {
        printf("Read input account file error!\n");
        PRINT_ERROR;
        return EXIT_FAILURE;
    }
    // jrb_traverse(node, accounts) {
    //     acc = (Account*) jval_v(node->val);
    //     printf("%s - %s - %d\n", acc->username, acc->password, acc->accStatus);
    // }

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
        Client client = (Client)malloc(sizeof(struct client));
        client->addr = cli_addr;
        client->sockfd = connfd;
        client->id = client_id++;

        /* Add client to List and fork thread*/
        jrb_insert_int(clients, client->id, new_jval_v(client));
        JRB node;
        int i = 0;
        jrb_traverse(node, clients) {
            Client cli = (Client) jval_v(node->val);
            printf("%d %d\n", i, cli->id);
        }

        pthread_create(&thread_id, NULL, handle_client, client);
        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}

int select_room_screen(Client client, char *buff_out) {
    JRB node;
    Room room = make_room();
    Account *acc = (Account *) malloc(sizeof(Account));
    int type = 0, exit_flag = 0;
    int run_traverse_time;
    int recvBytes = 0;
    char buff[BUFF_SIZE] = {0};
    char *sendString;


    // get type
    sscanf(buff_out, "%d", &type);

    switch (type) {
        case CREATE_NEW_ROOM:
            sscanf(buff_out, "%d %s %s", &type, room->room_name, room->owner_name);
            node = jrb_find_str(rooms, room->room_name);

            bzero(buff, sizeof(buff));
            if (node != NULL) {
                sprintf(buff, "%d", FAILED);
                send(client->sockfd, buff, strlen(buff), 0);
                return 0;
            }

            jrb_insert_str(rooms, room->room_name, new_jval_v(room));
            // send success to client and return 1
           break;
        case SHOW_LIST_ROOMS:
            printf("Show list rooms\n");
            
            jrb_traverse(node, rooms) {
                run_traverse_time++;
                room = (Room *) jval_v(node->val);
                bzero(buff, sizeof(buff));
                printf("%s %s\n", room->room_name, room->owner_name);
                sprintf(buff, "%-20s %s\n", room->room_name, room->owner_name);
                append(&sendString, buff);
            }
            if (run_traverse_time == 0) printf("No have room!\n");
            send(client->sockfd, sendString, strlen(sendString), 0);
            // send success to client and return 1
            break;
        case JOIN_ROOM:
            
            break;
        case SHOW_LIST_USERS:
            break;
        case PVP_CHAT:
            break;
        case SIGN_OUT:
            exit_flag = 1;
            // sign out 
            break;
        default:
            break;
    }


    bzero(buff, sizeof(buff));
    sprintf(buff, "%d", SUCCESS);
    send(client->sockfd, buff, strlen(buff), 0);
    sendString = NULL;
    if (exit_flag == 1) return EXIT;
    return 1;
}

int chat_in_room_screen(Client client, char *buff_out) {

}

void *handle_client(void *arg) {
    Client cli = (Client)arg;  // Client - is a poiter
    JRB node;
    char buff_out[BUFF_SIZE + CLIENT_NAME_LEN + 3] = {0};  // save send string from clients
    int leave_flag = 0;
    int type = -1;  // type of mess send from clients

    int err = 0;  // read recvbytes or read err number
    int res = 0;

    cli_count++;

    bzero(buff_out, sizeof(buff_out));

    /* Loop communicate with clients */
    while (1) {
        if (leave_flag == 1) break;

        /* received navigate screen control */
        bzero(buff_out, sizeof(buff_out));
        int received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
        // printf("received: %d; buffsize:  %ld\n", received, strlen(buff_out));
        /*
            continue read if have
        */

        /* Read type of navigate screen control */
        sscanf(buff_out, "%d", &type);

        switch (type) {
            case AUTH_SCREEEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                err = auth_screen(cli, buff_out);
                break;
            case SELECT_ROOM_SCREEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                res = select_room_screen(cli, buff_out);
                if (res == EXIT) leave_flag = 1;
                break;
            case CHAT_IN_ROOM_SCREEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                err = chat_in_room_screen(cli, buff_out);
                break;
            case EXIT:
                printf("EXIT\n");
                leave_flag = 1;
                break;
            default:
                break;
        }
    }  // end while - client communicate with server

    /* Delete client from List and yield thread */

    close(cli->sockfd);
    /* delete client from List */
    node = jrb_find_int(clients, cli->id);

    jrb_delete_node(node);

    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

/* Send message to all clients except sender */
void send_message(char *s, int cli_id) {
    pthread_mutex_lock(&clients_mutex);
    JRB node;
    jrb_traverse(node, clients) {
        Client cli = (Client)jval_v(node->val);
        if (cli->id != cli_id) {
            send(cli->sockfd, s, strlen(s), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}