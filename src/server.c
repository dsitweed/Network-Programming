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
void send_message_toPVP(Client* client, char *s);
void send_message_toGroup(Client* client, char *s);
void print_client_addr(struct sockaddr_in addr);

void appendToFile(char *fileName, char *string) {
    FILE *p = fopen(fileName, "a");
    fprintf(p, "\n%s", string);
    fclose(p);
}

int select_room_screen(Client *client, char *buff_out);

// return EXIT if sign out
// return 1 if success
// return 0 if failed
int auth_screen(Client *client, char *buff_out) {
    JRB node;
    Account *acc = (Account *) malloc(sizeof(Account));
    int type = 0;
    int success_flag = 1; // default success
    char buff[BUFF_SIZE] = {0};
    // get type
    sscanf(buff_out, "%d", &type);
    if (type != SIGN_IN && type != SIGN_UP && type != SIGN_OUT) return 0;

    if (type != SIGN_OUT) {
        sscanf(buff_out, "%d %s %s", &type, acc->username, acc->password);
        printf("%s %s\n", acc->username, acc->password);
    } else {
        sscanf(buff_out, "%d %s", &type, acc->username);
        printf("Sign out: %s\n", acc->username);
    }

    switch (type) {
        case SIGN_IN:
            node = jrb_find_str(accounts, acc->username);
            if (node == NULL) {
                printf("Login failed\n");
                success_flag = 0;
            }

            Account * findedAcc = (Account *) jval_v(node->val);
            if (strcmp(findedAcc->password, acc->password) != 0) {
                printf("Login failed\n");
                success_flag = 0;
            }

            printf("%s has logined\n", acc->username);
            // copy user name into clients Lists
            node = jrb_find_int(clients, findedAcc->user_id); // check use_id is exits
            if (node == NULL) {
                // find node have same sockfd
                JRB finded = NULL;
                jrb_traverse(node, clients) {
                    Client *cli = (Client *) jval_v(node->val);
                    if (cli->sockfd == client->sockfd) {
                        finded = node;
                        break;
                    }
                }

                if (finded != NULL) {
                    Client *cli = (Client *) jval_v(finded->val);
                    finded->key = new_jval_i(findedAcc->user_id);
                    cli->id = findedAcc->user_id;
                    strcpy(cli->username, findedAcc->username);
                } else success_flag = 0;
            } else success_flag = 0;
            break;
        case SIGN_OUT:
        {   JRB finded = NULL;
            jrb_traverse(node, clients) {
                Client* cli = (Client*) jval_v(node->val);
                if (cli->sockfd == client->sockfd) {
                    finded = node;
                    break;
                }
            }
            if (finded != NULL) jrb_delete_node(finded); else success_flag = 0;
            success_flag = EXIT;
            // cli_count--; - ở đây ko có quyền xóa count_client 
            break;}
        case SIGN_UP:
            node = jrb_find_str(accounts, acc->username);

            if (node != NULL) {
                printf("Failed\n");
                success_flag = 0;
            } else {
                acc->user_id = user_id++;
                jrb_insert_str(accounts, acc->username, new_jval_v(acc));
                sprintf(buff, "%s %s %d", acc->username, acc->password, acc->user_id);   
                appendToFile("accounts.txt", buff);
            }
            break;
        default:
            break;
    }
    
    bzero(buff, sizeof(buff));
    if (success_flag == 0) {
        sprintf(buff, "%d", FAILED); 
        send(client->sockfd, buff, strlen(buff), 0);
        return 0;
    } else {
        sprintf(buff, "%d", SUCCESS);
        send(client->sockfd, buff, strlen(buff), 0);
        if (success_flag == EXIT) return EXIT;
        else return 1;
    }
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
    
    fclose(file);
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
        printf("%s %s %s %s\n", "number_cli", "client_id", "node->key", "client->sockfd");
        jrb_traverse(node, clients) {
            Client* cli = (Client*) jval_v(node->val);
            printf("%d %d %d %d\n", i++, cli->id, jval_i(node->key), cli->sockfd);
        }

        pthread_create(&thread_id, NULL, handle_client, client);
        /* Reduce CPU usage */
        sleep(1);
    }

    printf("end\n");
    close(listenfd);
    return EXIT_SUCCESS;
}

int select_room_screen(Client *client, char *buff_out) {
    JRB node;
    Room *room = (Room*) malloc(sizeof(Room));
    int type = 0, exit_flag = 0, true_flag = 1; // default
    int run_traverse_time;
    char buff[BUFF_SIZE] = {0};
    char *sendString;

    // get type
    sscanf(buff_out, "%d", &type);

    switch (type) {
        case CREATE_NEW_ROOM:
            sscanf(buff_out, "%d %s %s", &type, room->room_name, room->owner_name);
            room->owner_id = client->id;
            room->arr_list_guest[0] = client->id;
            room->number_guest = 1;

            node = jrb_find_str(rooms, room->room_name);
            if (node != NULL) {
                true_flag = 0;
            } else {
                jrb_insert_str(rooms, room->room_name, new_jval_v(room));
            }
            // send success to client and return 1
           break;
        case SHOW_LIST_ROOMS:
            printf("Show list rooms\n");
            sendString = NULL;
            run_traverse_time = 0;
            jrb_traverse(node, rooms) {
                room = (Room *) jval_v(node->val);
                bzero(buff, sizeof(buff));
                printf("%-20s %s\n", room->room_name, room->owner_name);
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
            int isHaveInList = 0;
            room = (Room*) jval_v(node->val);
            for (int i = 0; i < room->number_guest; i++) {
                if (room->arr_list_guest[i] == client->id) {
                    isHaveInList = 1;
                    break;
                }
            }
            if (isHaveInList == 0)
                room->arr_list_guest[room->number_guest++] = client->id;
            /* Client ready to chat = 1 */
            JRB finded = NULL;
            jrb_traverse(node, clients) {
                int buff_i = jval_i(node->key);
                if (buff_i == client->id) {
                    finded = node;
                    break;
                }
            }
            /* check and change value ready_chat */
            if (finded != NULL) {
                Client *cli = (Client*) jval_v(finded->val);
                cli->ready_chat = 1;
            } else {
                true_flag = 0;
                PRINT_ERROR;
                break;
            }
            exit_flag = JOIN_ROOM;
            break;}
        case SHOW_LIST_USERS:
            printf("Show list users (is online) \n");
            printf("%-32s %-10s", "Username", "userId\n");
            sendString = NULL;

            run_traverse_time = 0;
            jrb_traverse(node, clients) {
                Client *cli = (Client *) jval_v(node->val);
                if (cli->id == client->id || cli->id == -1) continue;
                bzero(buff, sizeof(buff));
                printf("%-32s %-10d\n", cli->username, cli->id);
                sprintf(buff, "%-32s %d\n", cli->username, cli->id);
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

            JRB finded = NULL;
            jrb_traverse(node, clients) {
                int buff_i = jval_i(node->key);
                if (friend_id == buff_i) {
                    finded = node;
                    break;
                }
            }
            if (finded == NULL) {
                true_flag = 0;
                printf("Can't connect with user have id: %d\n", friend_id);
                break;
            }

            client->ready_chat = 1;
            /* Change status in data is running */ 
            finded = NULL;
            jrb_traverse(node, clients) {
                int buff_i = jval_i(node->key);
                if (client->id == buff_i) {
                    finded = node;
                    break;
                }
            }

            if (finded != NULL) {
                Client *buffCli = (Client*) jval_v(node->val);
                buffCli->ready_chat = 1;       
            } else {
                true_flag = 0;
                printf("Can't find youselft, id: %d\n", client->id);
                break;
            }

            exit_flag = PVP_CHAT;
            break;}
        case SIGN_OUT:
            JRB finded = NULL;
            /* Delete in clients list */
            jrb_traverse(node, clients) {
                int buff_i = jval_i(node->key);
                if (buff_i == client->id) {
                    finded = node;
                    break;
                }
            }

            if (finded == NULL) {
                printf("EXIT Failed\n");
                true_flag = 0;
                break;
            }
            printf("EXIT Success\n");
            jrb_delete_node(finded);
            exit_flag = EXIT;
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

    if (exit_flag == EXIT) return EXIT;
    if (exit_flag == PVP_CHAT) return PVP_CHAT;
    if (exit_flag == JOIN_ROOM) return JOIN_ROOM;
    return 1;
}

int chat_in_room_screen(Client *client, int typeChat) {
    JRB node;
    char buff[BUFF_SIZE] = {0};

    while (1) {
        bzero(buff, sizeof(buff));
        int received = recvData(client->sockfd, buff, sizeof(buff));
        printf("Check chat_in_room_screen: Type: %d, received Bytes: %d\n", typeChat, received);
        if (received == 0 || strcmp(buff, "exit") == 0) {
            client->ready_chat = 0;
            node = NULL;
            jrb_traverse(node, clients) {
                int buff = jval_i(node->key);
                if (buff == client->id) break;
            }
            if (node != NULL) {
                Client *cl = (Client *) jval_v(node->val);
                cl->ready_chat = 0;
            }
            fflush(stdout);
            break;
        } else if (received > 0 && typeChat == PVP_CHAT) {
            printf("Send PVP: %s -> %s\n", client->username, buff);
            send_message_toPVP(client, buff);
        }else if (received > 0 && typeChat == JOIN_ROOM) {
            printf("Send to group: %s -> %s\n", client->username, buff);
            send_message_toGroup(client, buff);
        } else {
            PRINT_ERROR;
            break;
        }
    }

    printf("End chat\n");

    return EXIT;
}

void *handle_client(void *arg) {
    Client* cli = (Client*) arg;  // Client - is a poiter
    char buff_out[BUFF_SIZE + CLIENT_NAME_LEN + 3] = {0};  // save send string from clients
    int leave_flag = 0;
    int type = -1;  // type of mess send from clients

    int chat_room_type = 0;  // type chat at select_room_screen
    int res = 0; // response from child funtion

    cli_count++;

    bzero(buff_out, sizeof(buff_out));

    /* Loop communicate with clients */
    while (1) {
        if (leave_flag == 1) break;

        /* received navigate screen control */
        bzero(buff_out, sizeof(buff_out));
        int received = 0;
        if (recv(cli->sockfd, buff_out, sizeof(buff_out), 0) <= 0) break; // exit
        printf("received: %d; buffsize:  %ld\n", received, strlen(buff_out));
        printf("%s\n", buff_out);

        /* Read type of navigate screen control */
        if (strlen(buff_out) > 0) sscanf(buff_out, "%d", &type);
        else break; // exit

        switch (type) {
            case AUTH_SCREEEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                res = auth_screen(cli, buff_out);
                if (res == EXIT ) leave_flag = 1;
                break;
            case SELECT_ROOM_SCREEN:
                bzero(buff_out, sizeof(buff_out));
                received = recv(cli->sockfd, buff_out, sizeof(buff_out), 0);
                chat_room_type = select_room_screen(cli, buff_out);
                if (chat_room_type == EXIT) leave_flag = 1;
                /* Đã select xong room và chọn xong người chat  */
                break;
            case CHAT_IN_ROOM_SCREEN:
                res = chat_in_room_screen(cli, chat_room_type);
                break;
            default:
                break;
        }
    }  // end while - client communicate with server

    /* Delete client from List and yield thread */
    close(cli->sockfd);

    cli_count--;
    pthread_detach(pthread_self());
    return NULL;
}

void handle_exit() {
    printf("\nEXIT\n");
    /* Save data */
    /* Save rooms */
    JRB node;
    FILE *fp = fopen("rooms.txt", "w");
    jrb_traverse(node, rooms) {
        Room *room = (Room *) jval_v(node->val);
        fprintf(fp, "%s %s %d %d\n", room->room_name, room->owner_name, room->owner_id, room->number_guest);
        for (int i = 0; i < room->number_guest; i++) {
            fprintf(fp, "%d\n", room->arr_list_guest[i]);
        }
    }
    fclose(fp);
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

int len_of_number(int number) {
    int count = 0;
    if (number == 0) return 1;
    while (number > 0) {
        number = number / 10;
        count++;
    }
    return count;
}

void send_message_toPVP(Client* client, char *s) {
    JRB node;
    int destId, lenNumber = 0;
    int success_flag = 0;
    char buff[BUFF_SIZE] = { 0 };
    pthread_mutex_lock(&clients_mutex);

    sscanf(s, "%d", &destId);
    lenNumber = len_of_number(destId);
    strcpy(buff, s + lenNumber + 1);

    
    node = NULL;
    jrb_traverse(node, clients) {
        int buff = jval_i(node->key);
        if (buff == destId) break;
    }

    if (node != NULL) {
        Client *destCli = (Client *) jval_v(node->val);
        if(destCli->ready_chat == 1) {
            success_flag = 1;            
            sendData(destCli->sockfd, buff, strlen(buff));
        }
    }
    /* If node == null or destCli not ready to chat*/
    if (success_flag == 0) {
        bzero(buff, sizeof(buff));
        sprintf(buff, "%s %d","Can't send to", destId);
        sendData(client->sockfd, buff, strlen(buff));
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message_toGroup(Client* client, char *s) {
    JRB node;
    char roomName[ROOM_NAME_LEN + 1] = { 0 };
    char buff[BUFF_SIZE] = { 0 };
    
    pthread_mutex_lock(&clients_mutex);

    sscanf(s, "%s", roomName);
    strcpy(buff, s + strlen(roomName) + 1);
    node = jrb_find_str(rooms, roomName);
    if (node != NULL) {
        Room *room = (Room *) jval_v(node->val);
        for (int i = 0; i < room->number_guest; i++) {
            JRB run;
            if (room->arr_list_guest[i] != client->id) {
                // find destClient
                int finded = 0;;
                jrb_traverse(run, clients) {
                    int buff_i = jval_i(run->key);
                    if (buff_i == room->arr_list_guest[i]) {
                        finded = 1;
                        printf("%d %d\n", buff_i, room->arr_list_guest[i]);
                        break;
                    }
                }
                if (finded == 0) run = NULL;

                if (run != NULL) {
                    Client*cli = (Client*) jval_v(run->val);
                    PRINT_ERROR;
                    printf("%d %d %s %d\n", cli->id, cli->sockfd, cli->username, cli->ready_chat);
                    PRINT_ERROR;
                    if (cli->ready_chat == 1){
                        PRINT_ERROR;
                        sendData(cli->sockfd, buff, strlen(buff));
                    }
                }
            }
        } // send mess to all client in room and is ready to recving
    } else {
        bzero(buff, sizeof(buff));
        sprintf(buff, "Can't send to %s", roomName);
        printf("Can't send to group %s\n", roomName);
        send(client->sockfd, buff, strlen(buff), 0);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}