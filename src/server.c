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

#define ACCOUNT_FILE "accounts.txt"

// Tránh xung đột tranh dành ghi dữ liệu
// (Kiểu 2 thàng cùng cli_count++ (0 -> 1 trong khi có 2 đứa))
static _Atomic unsigned int cli_count = 0;
static _Atomic unsigned int id_count = 0;
static _Atomic JRB accounts;
static _Atomic JRB clients;
static _Atomic JRB rooms;
static int user_id = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
int read_action_type(char *string);
void handle_exit(); // for server exit
void print_client_addr(struct sockaddr_in addr);

int read_action_type(char *string);
int send_response(int sockfd, int type, char* message);
int sign_in(Client *client, char *req);
int sign_up(Client *client, char *req);
int sign_out(Client *client, char *req);
int show_list_users(Client *client, char *req);
int show_list_rooms(Client *client, char *req);
int create_new_room(Client *client, char *req);
int out_room(Client *client, char *req);
int join_room(Client *client, char *req);
int room_chat(Client *client, char *req);
int connect_PVP(Client* client, char* req);
int pvp_chat(Client *client, char *req);
int out_chat(Client *client, char *req);
int share_file(Client *client, char *req);

void appendToFile(char *fileName, char *string) {
    FILE *p = fopen(fileName, "a");
    fprintf(p, "\n%s", string);
    fclose(p);
}


JRB getAccList(const char *sourceFile) {
    JRB list = make_jrb();
    FILE *accFile = fopen(sourceFile, "r");
    if (accFile == NULL) return NULL;

    while (1) {
        Account *acc = (Account *) malloc(sizeof(Account));
        if (fscanf(accFile, "%s %s %d\n", acc->username, acc->password, &acc->user_id) == EOF) break;
        jrb_insert_str(list, strdup(acc->username), new_jval_v(acc));
        /* count number of Account in file text */
        id_count++;
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
        if (fscanf(file, "%s %s %d %d\n", room->room_name, room->owner_name, &room->owner_id, &room->number_member) <= 0) break;

        for (int j = 0; j < room->number_member; j++) {
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
    // get all rooms from file 
    rooms = getRoomList("rooms.txt");
    if (rooms == NULL) {
        printf("Read input from rooms file error!\n");
        PRINT_ERROR;
        return EXIT_FAILURE;
    }

    // get account list from File
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
        client->ready_chat = 0;

        /* Add client to List and fork thread*/
        jrb_insert_int(clients, client->id, new_jval_v(client));
        
        JRB node;
        int i = 0;
        printf("List of clients in the system (id: -1 is not login)\n");
        printf("%-10s %-10s %-10s %-10s\n", "number_cli", "client_id", "node->key", "client->sockfd");
        jrb_traverse(node, clients) {
            Client* cli = (Client*) jval_v(node->val);
            printf("%-10d %-10d %-10d %-10d\n", ++i, cli->id, jval_i(node->key), cli->sockfd);
        }

        pthread_create(&thread_id, NULL, handle_client, client);
        /* Reduce CPU usage */
        sleep(1);
    }

    printf("\n=== END CHATROOM ===\n");
    close(connfd);
    close(listenfd);
    return EXIT_SUCCESS;
}

void *handle_client(void *arg) {
    Client* cli = (Client*) arg;  // Client - is a pointer
    char buff_out[BUFSIZ] = {0};  // save send string from clients
    int leave_flag = 0;
    int action_type = -1;  // type of mess send from clients

    cli_count++;

    /* Loop communicate with clients */
    while (1) {
        if (leave_flag == 1) break;

        /* Read action type client request */
        bzero(buff_out, sizeof(buff_out));
        int received = recvData(cli->sockfd, buff_out, sizeof(buff_out));
        if (received <= 0 ) break; // exit
        // printf("received bytes: %d; String length: %ld; String: %s\n", received, strlen(buff_out), buff_out);

        action_type = read_action_type(buff_out);
        switch (action_type)
        {
        case SIGN_IN:
            sign_in(cli, buff_out);
            break;
        case SIGN_UP:
            sign_up(cli, buff_out);
            break;
        case SIGN_OUT:
            sign_out(cli, buff_out);
            break;
        case SHOW_LIST_USERS:
            show_list_users(cli, buff_out);
            break;
        case SHOW_LIST_ROOMS:
            show_list_rooms(cli, buff_out);
            break;
        case CREATE_NEW_ROOM:
            create_new_room(cli, buff_out);
            break;
        case JOIN_ROOM:
            join_room(cli, buff_out);
            break;
        case OUT_ROOM:
            out_room(cli, buff_out); // đang phát triển chưa tích hợp vào
            break;
        case CONNECT_PVP:
            connect_PVP(cli, buff_out);
            break;
        case PVP_CHAT:
            pvp_chat(cli, buff_out);
            break;
        case ROOM_CHAT:
            room_chat(cli, buff_out);
            break;
        case OUT_CHAT:
            out_chat(cli, buff_out);
            break;
        case SHARE_FILE:
            share_file(cli, buff_out); // đang phát triển chưa tích hợp vào 
            break;
        default:
            printf("\nHAVE ERROR\n");
            leave_flag = 1;
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
        fprintf(fp, "%s %s %d %d\n", room->room_name, room->owner_name, room->owner_id, room->number_member);
        for (int i = 0; i < room->number_member; i++) {
            fprintf(fp, "%d\n", room->arr_list_guest[i]);
        }
    }
    fclose(fp);
    exit(1);
}

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff, (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16, (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

int read_action_type(char *string) {
    int action = -1;
    if (sscanf(string, "%d", &action))
        return action;
    else return -1; // if have error
}

int send_response(int sockfd, int type, char* message) {
    char *buff = NULL;
    char type_s[10] = {0};
    sprintf(type_s, "%d ", type);
    append(&buff, type_s);
    append(&buff, message);

    return sendData(sockfd, buff, strlen(buff));
}

int sign_in(Client *client, char *req) {
    JRB node = NULL;
    int action;
    char username[50] = {0}, password[50] = {0};
    sscanf(req, "%d %s %s", &action, username, password);
    node = jrb_find_str(accounts, username);
    if (node == NULL) {
        send_response(client->sockfd, FAILED, "LOGIN FAILED\n");
        printf("LOGIN FAILED\n");
        return 0;
    }
    // check password
    Account* acc = (Account*) jval_v(node->val);
    if (strcmp(acc->password, password) != 0) {
        send_response(client->sockfd, FAILED, "LOGIN FAILED\n");
        printf("LOGIN FAILED\n");
        return 0;
    }

    // check logint 
    node = NULL;
    node = jrb_find_int(clients, acc->user_id);
    if (node != NULL) {
        send_response(client->sockfd, FAILED, "ANOTHER LOGINT\n");
        printf("ANOTHER LOGINT\n");
        return 0;
    } else {
        client->id = acc->user_id;
        strcpy(client->username, acc->username);

        node = NULL;
        jrb_traverse(node, clients) {
            Client* cli = (Client*) jval_v(node->val);
            if (client->sockfd == cli->sockfd) break;
        }
        // Save to list clients is running in system
        Client* cli = (Client*) jval_v(node->val);
        jrb_delete_node(node);
        cli->id = acc->user_id;
        strcpy(cli->username, acc->username);
        jrb_insert_int(clients, cli->id, new_jval_v(cli));

        send_response(client->sockfd, SUCCESS, "LOGIN SUCCESS\n");
        printf("LOGIN SUCCESS\n");
        return 1;
    }
}

int sign_up(Client *client, char* req) {
    JRB node = NULL;
    int action;
    char username[50] = {0}, password[50] = {0};
    sscanf(req, "%d %s %s", &action, username, password);

    node = jrb_find_str(accounts, username);
    if(node) {// username exits
        char mess[] = "SIGN UP FAILED\n";
        send_response(client->sockfd, FAILED, mess);
        puts(mess);
    } else {
        Account *acc = (Account*) malloc(sizeof(Account));
        strcpy(acc->username, username);
        strcpy(acc->password, password);
        acc->user_id = id_count;
        jrb_insert_str(accounts, strdup(acc->username), new_jval_v(acc));

        char mess[200] = {0};
        sprintf(mess, "%s %s %d", username, password, id_count);
        id_count++;
        appendToFile(ACCOUNT_FILE, mess);

        send_response(client->sockfd, SUCCESS, "SIGN UP SUCCESS\n");
        printf("SIGN UP SUCCESS\n");
    }
    return 1;
}

int sign_out(Client *client, char *req) {
    JRB node = NULL;
    int action;
    char username[50] = {0};
    sscanf(req, "%d %s", &action, username);

    printf("username: %s %s\n", username, client->username);

    node = jrb_find_int(clients, client->id);
    if ((node == NULL) || (strcmp(client->username, username) != 0)) {
        send_response(client->sockfd, FAILED, "SIGN OUT FAILED\n");
        printf("SIGN OUT FAILED\n");
    } else {
        send_response(client->sockfd, SUCCESS, "SIGN OUT SUCCESS\n");
        jrb_delete_node(node);
        printf("SIGN OUT SUCCESS\n");
    }
    return 1;
}

int show_list_users(Client* client, char *req) {
    JRB node;
    char *message = NULL;
    char buff[BUFF_SIZE] = {0};

    sprintf(buff, "%-20s%-20s%-10s%-20s\n", "username", "userId", "sockfd", "status");
    puts(buff);
    append(&message, buff);

    jrb_traverse(node, clients) {
        Client* cli = (Client*) jval_v(node->val);
        if (cli->id == client->id || cli->id == -1) continue;// skip if is mine or not login
        bzero(buff, sizeof(buff));
        sprintf(buff, "%-20s%-20d%-10d%-20s\n", cli->username, cli->id, cli->sockfd, cli->ready_chat ? "chatting" : "free");
        puts(buff);
        append(&message, buff);
    }
    send_response(client->sockfd, SUCCESS, message);
    printf("SHOW LIST USERS SUCCESS\n");
    return 1;
}

int show_list_rooms(Client* client, char* req) {
    JRB node;
    char* message = NULL, buff[BUFF_SIZE] = {0};
    printf("List rooms in program\n");
    sprintf(buff, "%-50s%-20s%-20s%-20s\n", "room name", "owner name", "owner id", "number member");
    puts(buff);
    append(&message, buff);

    jrb_traverse(node, rooms) {
        Room* room = (Room*) jval_v(node->val);
        bzero(buff, sizeof(buff));
        sprintf(buff, "%-50s%-20s%-20d%-20d\n", room->room_name, room->owner_name, room->owner_id, room->number_member);
        puts(buff);
        append(&message, buff);
    }
    send_response(client->sockfd, SUCCESS, message);
    printf("SHOW LIST ROOMS SUCCESS\n");
    return 1;
}

int create_new_room(Client* client, char* req) {
    JRB node = NULL;
    int action = -1;
    char room_name[40] = {0}, owner_name[50] = {0};
    sscanf(req, "%d%s%s", &action, room_name, owner_name); 
    node = jrb_find_str(rooms, room_name);
    if (node != NULL || (strcmp(owner_name, client->username) != 0)) {// exist
        send_response(client->sockfd, FAILED, "CREATE NEW ROOM FAILED\n");
        printf("CREATE NEW ROOM FAILED\n");
    } else {
        Room* room = (Room*) malloc(sizeof(Room));
        strcpy(room->room_name, room_name);
        room->owner_id = client->id;
        room->number_member = 1;
        strcpy(room->owner_name, client->username);
        room->arr_list_guest[0] = client->id;

        jrb_insert_str(rooms, strdup(room->room_name), new_jval_v(room));
        send_response(client->sockfd, SUCCESS, "CREATE NEW ROOM SUCCESS\n");
        printf("CREATE NEW ROOM SUCCESS\n");
    }
    return 1;
}

int join_room(Client* client, char* req) {
    JRB node = NULL;
    int action = -1;
    char room_name[ROOM_NAME_LEN] = {0};
    sscanf(req, "%d%s", &action, room_name);

    node = jrb_find_str(rooms, room_name);
    if (node == NULL) {
        send_response(client->sockfd, FAILED, "JOIN ROOM FAILED\n");
        printf("JOIN ROOM FAILED. ROOM NOT EXITS\n");
    } else {
        Room* room = (Room*) jval_v(node->val);
        int isHave = 0;
        for (int i = 0; i < room->number_member; i++) {
            if (room->arr_list_guest[i] == client->id) {
                isHave = 1; 
                break;
            }
        }

        if (isHave == 0) {
            room->arr_list_guest[room->number_member] = client->id;
            room->number_member++;
        }

        client->ready_chat = 1;
        strcpy(client->with_room, room_name);
        // Save information to tree clients in system
        node = jrb_find_int(clients, client->id);
        if (node == NULL) {
            send_response(client->sockfd, FAILED, "JOIN ROOM FAILED\n");
            printf("JOIN ROOM FAILED. CLIENT DON'T EXITS\n");
            return 0;
        }
        Client* node_cli = (Client*) jval_v(node->val);
        node_cli->ready_chat = 1;
        strcpy(node_cli->with_room, room_name);

        send_response(client->sockfd, SUCCESS, "JOIN ROOM SUCCESS\n");
        printf("JOIN ROOM SUCCESS\n");
    }
    return 1;
}

int out_room(Client* client, char* req) {
    /*
    
    JRB node = NULL;
    int action = -1;
    char room_name[40] = {0};
    sscanf(req, "%d%s", &action, room_name);

    node = jrb_find_str(rooms, room_name);
    if (node == NULL) {
        send_response(client->sockfd, FAILED, "OUT ROOM FAILED. ROOM NOT EXITS\n");
        printf("OUT ROOM FAILED. ROOM NOT EXITS\n");
    } else {
        Room* room = (Room*) jval_v(node->val);
        int i;
        // Xác định vị trí phần tử cần xóa
        for (i = 0; i < room->number_member; i++) {
            if (client->id == room->arr_list_guest[i]) break;
        }

        room->number_member--;// số thành viên giảm 1
        // xóa và dịch các phần tử phía sau vị trí xóa lên
        for (i; i < room->number_member; i++) {
            room->arr_list_guest[i] = room->arr_list_guest[i+1];
        }

        send_response(client->sockfd, SUCCESS, "OUT ROOM SUCCESS\n");
        printf("OUT ROOM SUCCESS\n");
    }

    */
    return 1;
}

int room_chat(Client* client, char* req) {
    JRB node = NULL;
    int action = -1;
    char room_name[ROOM_NAME_LEN] = {0}, message[BUFSIZ] = {0};// save message want send to room

    pthread_mutex_lock(&clients_mutex);
    sscanf(req, "%d%s", &action, room_name);
    strcpy(message, req + len_of_number(action) + strlen(room_name) + 2);// get message

    node = jrb_find_str(rooms, room_name);
    if (node == NULL) {
        char mess[] = "ROOM CHAT FAILED. ROOM NOT EXITS\n";
        send_response(client->sockfd, FAILED, mess);
        puts(mess);
        return 0;
    }

    Room* room = (Room*) jval_v(node->val);
    for (int i = 0; i < room->number_member; i++) {
        JRB tmp = jrb_find_int(clients, room->arr_list_guest[i]);
        if (tmp != NULL) {
            Client* cli = (Client*) jval_v(tmp->val);
            if (cli->id == client->id) continue; // skip send for myself

            if ((cli->ready_chat == 1) && (strcmp(cli->with_room, client->with_room) == 0)) {
                sendData(cli->sockfd, message, strlen(message));
            }
        }
    }// end loop check member in room

    send_response(client->sockfd, SUCCESS, "ROOM CHAT SUCCESS\n");
    printf("ROOM CHAT SUCCESS\n");
    pthread_mutex_unlock(&clients_mutex);

    return 1;
}

int connect_PVP(Client* client, char* req) {
    JRB node = NULL;
    int action = -1, friend_id = -1;
    char mess[50] = {0};
    sscanf(req,"%d%d", &action, &friend_id);

    node = jrb_find_int(clients, friend_id);
    if (node == NULL) {
        client->ready_chat = 0;
        client->with_id = -1;
        sprintf(mess, "CONNECT WITH %d FAILED\n", friend_id);
        puts(mess);
        send_response(client->sockfd, FAILED, mess);
    } else {
        client->ready_chat = 1;
        client->with_id = friend_id;
        sprintf(mess, "CONNECT WITH %d SUCCESS\n", friend_id);
        puts(mess);
        send_response(client->sockfd, SUCCESS, mess);
    }
    return 1;
}

int pvp_chat(Client* client, char* req) {
    JRB node = NULL;
    int action = -1, friend_id = -1;
    char message[BUFSIZ] = {0};
    sscanf(req, "%d%d", &action, &friend_id);
    strcpy(message, req + len_of_number(action) + len_of_number(friend_id) + 2);

    node = jrb_find_int(clients, friend_id);
    if (node == NULL) {
        send_response(client->sockfd, FAILED, "PVP CHAT FAILED. FRIEND NOT EXITS\n");
        printf("PVP CHAT FAILED. FRIEND NOT EXITS\n");
        return 0;
    }

    Client* friend = (Client*) jval_v(node->val);
    char mess[200] = {0};
    if(friend->ready_chat == 1) {
        sendData(friend->sockfd, message, strlen(message));
        sprintf(mess, "%s => %s", "Success send to", friend->username);
    } else {
        sprintf(mess, "%s %s", friend->username, "not ready for chat");
    }
    
    send_response(client->sockfd, SUCCESS, mess);
    return 1;
}

int out_chat(Client* client, char* req) {
    JRB node = NULL;
    // int action = -1, type_chat = -1;

    node = jrb_find_int(clients, client->id);
    if (node == NULL) {
        send_response(client->sockfd, FAILED, "OUT CHAT FAILED\n");
        printf("OUT CHAt FAILED\n");
        return 0;
    }

    Client* cli_node = (Client*) jval_v(node->val);
    cli_node->ready_chat = 0;
    cli_node->with_id = -1;
    memset(cli_node->with_room, 0, sizeof(cli_node->with_room));

    send_response(client->sockfd, SUCCESS, "OUT CHAt SUCCESS\n");
    printf("OUT CHAt SUCCESS\n");
    return 1;
}


int share_file(Client* client, char* req) {
    return 1;
}