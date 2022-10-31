#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>

#define BUFF_SIZE 1024
#define CLIENT_NAME_LEN 32

#define PRINT_ERROR printf("[Error at line %d in %s]: %s\n", __LINE__, __FILE__, strerror(errno))

typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char username[CLIENT_NAME_LEN];
    JRB list_room;
} * Client;  // client is poiter

typedef struct account_ {
    char username[CLIENT_NAME_LEN];
    char password[32];
    int accStatus;
} Account;

typedef struct room_ {
    int room_id;
    char room_name[32];
    char owner_name[CLIENT_NAME_LEN];
    JRB list_guest;
} * Room;

int prompt_input(char const *message, char *buff); // Cấp phát động bộ nhớ cho buff
int prompt_input_ver2(char const *message, char *buff);// Buff bộ nhớ cố định 


void str_trim_lf(char *arr);

void append(char **p_src, char *dest);

void clear_line();

Client make_client(/* struct sockaddr_in addr, */ int fd, int id, char *username);

Room make_room() {
    Room newRoom = (Room) malloc(sizeof(Room));
    newRoom->list_guest = make_jrb();

    return newRoom;
}

/* create delete room - function in server.c  when have roomList*/

/*
    Cần xem xét lại về tính thực tế của hàm này
*/
int config_server(int *fd, struct sockaddr_in *addr);

#endif  // UTILS_H