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
#define ROOM_NAME_LEN 32
#define MAX_GUEST_IN_ROOM 100

#define PRINT_ERROR printf("[Error at line %d in %s]: %s\n", __LINE__, __FILE__, strerror(errno))

typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char username[CLIENT_NAME_LEN];
    int ready_chat; // is ready to chat ?
} Client; 

typedef struct account_ {
    char username[CLIENT_NAME_LEN];
    char password[32];
    int accStatus;
    int user_id;
} Account;

typedef struct room_ {
    char owner_name[CLIENT_NAME_LEN];
    char room_name[ROOM_NAME_LEN];
    int owner_id;
    // JRB list_guest; // Cực kì cẩn thận khi đọc list_guest rất hay bị lỗi do list JRB ko sạch
    int arr_list_guest[MAX_GUEST_IN_ROOM];
    int number_guest; // max near 300 guest with 1024 bytes
} Room;

int prompt_input(char const *message, char *buff); // Cấp phát động bộ nhớ cho buff
int prompt_input_ver2(char const *message, char *buff);// Buff bộ nhớ cố định 


void str_trim_lf(char *arr);

void append(char **p_src, char *dest);

void clear_line();

// Client make_client(/* struct sockaddr_in addr, */ int fd, int id, char *username);

// Room make_room() {
//     Room newRoom = (Room) malloc(sizeof(Room));

//     return newRoom;
// }

/* create delete room - function in server.c  when have roomList*/

/*
    Cần xem xét lại về tính thực tế của hàm này
*/
int config_server(int *fd, struct sockaddr_in *addr);

int sendData(int sockfd, char *data, int len) {
    int sent = 0;
    int tmp = 0;
    do {
        tmp = send(sockfd, data + sent, len - sent, 0);
        sent += tmp;
    } while (tmp > 0 && sent < len );
    return sent;
}

int recvData(int sockfd, char *data, int len) {
    int received = 0, tmp = 0;

    do {
        tmp = recv(sockfd, data + received, BUFF_SIZE, 0);
        received += tmp;
    } while (tmp < 0 && received < len && received < BUFF_SIZE);
    return received;
}

#endif  // UTILS_H