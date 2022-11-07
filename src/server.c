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
static int user_id = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
void handle_exit(); // for sersev exit
void send_message(char *s, int uid);
void print_client_addr(struct sockaddr_in addr);

void appendToFile(char *fileName, char *string) {
    FILE *p = fopen(fileName, "a");
    fprintf(p, "\n%s", string);
    fclose(p);
}

int select_room_screen(Client *client, char *buff_out);

// return 0 if failed
// return 1 if success
int auth_screen(Client *client, char *buff_out) {
    JRB node;
    Account *acc = (Account *) malloc(sizeof(Account));
    // int logined_flag = 0, exit_flag = 0;
    int type = 0;
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
            // copy user name into clients Lists
            jrb_traverse(node, clients) {
                Client *cli = (Client *) jval_v(node->val);
                if (cli->sockfd == client->sockfd){
                    node->key = new_jval_i(findedAcc->user_id);
                    cli->id = findedAcc->user_id;
                    strcpy(cli->username, findedAcc->username);
                }
            }
            
            send(client->sockfd, buff, strlen(buff), 0);
            return 1;
        case SIGN_OUT:
            node = jrb_find_int(clients, client->id);
            if (node == NULL) return 0;

            jrb_delete_node(node);
            // cli_count--; - ở đây ko có quyền xóa count_client 
            return EXIT;
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
            
            acc->user_id = user_id++;
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

JRB getAccList(const char *sourceFile) {
    JRB list = make_jrb();
    FILE *accFile = fopen(sourceFile, "r");
    if (accFile == NULL) return NULL;

    while (1) {
        Account *acc = (Account *)malloc(sizeof(Account));
        if (fscanf(accFile, "%s %s %d\n", acc->username, acc->password, &acc->user_id) == EOF) break;
        jrb_insert_str(list, strdup(acc->username), new_jval_v(acc));
        /* count number of Account in file text */
        user_id++;
    }
    fclose(accFile);
    return list;
}

JRB getRoomList(const char *sourceFile) {
    FILE *file = fopen(sourceFile, "r");
    JRB list = make_jrb();
    if (file == NULL) return NULL;
    
    while(1) {
        Room* room = (Room*) malloc(sizeof(Room));
        if (fscanf(file, "%s %s %d %d\n", room->room_name, room->owner_name, &room->owner_id, &room->number_guest) <= 0) break;

        for (int j = 0; j < room->number_guest; j++) {
            fscanf(file, "%d\n", &room->arr_list_guest[j]);
            // printf("%d\n", room->arr_list_guest[j]);
        }
        jrb_insert_str(list, room->room_name, new_jval_v(room));
    }
    return list;
}

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
    rooms = make_jrb();
    // get all rooms of logined clients
    rooms = getRoomList("rooms.txt");
    if (rooms == NULL) {
        printf("Read input from rooms file error!\n");
        PRINT_ERROR;
        return EXIT_FAILURE;
    }

    // get account list from FIle
    accounts = make_jrb();
    accounts = getAccList("accounts.txt");
    if (accounts == NULL) {
        printf("Read input from accounts file error!\n");
        PRINT_ERROR;
        return EXIT_FAILURE;
    }

    /* Step 1: Construct a TCP socket to listen connection request */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    serv_addr.sin_port = htons(PORT);

    /* Signal exit */
    signal(SIGINT, handle_exit); 

    /*Tránh được lỗi "Address / Port already in use"*/
    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0) {
        PRINT_ERROR;
        printf("ERROR: setsockopt failed\n");
        return EXIT_FAILURE;
    }

    /* Step 2: Bind address to socket */
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        PRINT_ERROR;
        return EXIT_FAILURE;
    }

    /* Step 3: Listen request from client_addr */
    if (listen(listenfd, MAX_THREADS) < 0) {
        PRINT_ERROR;
        printf("Over max threads\n");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");

    /* Step 4: Communicate with client*/
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
        Client* client = (Client*) malloc(sizeof(struct client));
        client->addr = cli_addr;
        client->sockfd = connfd;
        client->id = -1; // default = -1

        strcpy(client->username, "default_name");

        /* Add client to List and fork thread*/
        jrb_insert_int(clients, client->id, new_jval_v(client));
        
        JRB node;
        int i = 0;
        jrb_traverse(node, clients) {
            Client* cli = (Client*) jval_v(node->val);
            printf("%d %d\n", i, cli->id);
        }

        pthread_create(&thread_id, NULL, handle_client, client);
        /* Reduce CPU usage */
        sleep(1);
    }

    printf("end\n");
    return EXIT_SUCCESS;
}

int select_room_screen(Client *client, char *buff_out) {
    JRB node;
    Room *room = (Room*) malloc(sizeof(Room));
    int type = 0, exit_flag = 0, true_flag = 1; // default
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
            sendString = NULL;
            
            run_traverse_time = 0;
            jrb_traverse(node, rooms) {
                room = (Room *) jval_v(node->val);
                bzero(buff, sizeof(buff));
                printf("%s %s\n", room->room_name, room->owner_name);
                sprintf(buff, "%-20s %s\n", room->room_name, room->owner_name);
                append(&sendString, buff);
                run_traverse_time++;
            }
            if (run_traverse_time == 0) {
                printf("No have room!\n");
                append(&sendString, "No have room!\n");
            }
            send(client->sockfd, sendString, strlen(sendString), 0);
            // send success to client and return 1
            break;
        case JOIN_ROOM:
            { char room_name[100];

            sscanf(buff_out, "%d %s", &type, room_name);
            node = jrb_find_str(rooms, room_name);
            if (node == NULL) {
                true_flag = 0;
                break;
            }
            room = (Room*) jval_v(node->val);
            int number_guest = room->number_guest;
            room->arr_list_guest[number_guest++] = client->id;
            break;}
        case SHOW_LIST_USERS:
            printf("Show list users (is online) \n");
            sendString = NULL;

            run_traverse_time = 0;
            jrb_traverse(node, clients) {
                Client *cli = (Client *) jval_v(node->val);
                if (cli->id == client->id) continue;
                bzero(buff, sizeof(buff));
                printf("%s %d\n", cli->username, cli->id);
                sprintf(buff, "%s %d\n", cli->username, cli->id);
                append(&sendString, buff);
                run_traverse_time++;
            }
            if (run_traverse_time == 0) {
                printf("No have users is online\n");
                append(&sendString, "No have users is online\n");
            }
            send(client->sockfd, sendString, strlen(sendString), 0);
            break;
        case PVP_CHAT:
            {int friend_id;
            printf("Chat PvP\n");
            sscanf(buff_out, "%d %d", &type, &friend_id);
            
            room = (Room*) malloc(sizeof(Room));
            room->arr_list_guest[0] = client->id;
            room->arr_list_guest[1] = friend_id;
            room->number_guest = 2;

            jrb_insert_str(rooms, "PvP", new_jval_v(room));
            exit_flag = 1;
            break;}
        case SIGN_OUT:
            exit_flag = 1;
            // sign out 
            break;
        default:
            break;
    }


    bzero(buff, sizeof(buff));
    if (true_flag) sprintf(buff, "%d", SUCCESS);
    else sprintf(buff, "%d", FAILED);
    send(client->sockfd, buff, strlen(buff), 0);
    sendString = NULL;
    if (exit_flag == 1) return EXIT;
    return 1;
}

int chat_in_room_screen(Client *client) {
    int back_flag = 0;
    char buff[BUFF_SIZE] = {0};

    while (1) {

        bzero(buff, sizeof(buff));
        int received = recv(client->sockfd, buff, sizeof(buff), 0);
        if (received > 0) {

        } else if (received == 0 || strcmp(buff, "ext") == 0) {

        } else {
            PRINT_ERROR;

        }

    }

    return 1;
}

void *handle_client(void *arg) {
    Client* cli = (Client*) arg;  // Client - is a poiter
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
        int received = 0;
        received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
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
                if (err == EXIT) leave_flag = 1;
                break;
            case SELECT_ROOM_SCREEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                res = select_room_screen(cli, buff_out);
                if (res == EXIT) leave_flag = 1;
                break;
            case CHAT_IN_ROOM_SCREEN:
                bzero(buff_out, sizeof(buff_out));
                // received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                err = chat_in_room_screen(cli);
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
    node = jrb_find_int(clients, cli->sockfd);

    if (node != NULL) jrb_delete_node(node);

    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

void handle_exit() {
    printf("\nEXIT\n");
    /* Save data */
    exit(1);
}

/* Send message to all clients except sender */
void send_message(char *s, int cli_id) {
    pthread_mutex_lock(&clients_mutex);
    JRB node;
    jrb_traverse(node, clients) {
        Client *cli = (Client*)jval_v(node->val);
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