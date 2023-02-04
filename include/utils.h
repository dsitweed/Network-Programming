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
#define CLIENT_NAME_LEN 50
#define ROOM_NAME_LEN 50
#define MAX_GUEST_IN_ROOM 100

#define PRINT_ERROR printf("[Error at line %d in %s]: %s\n", __LINE__, __FILE__, strerror(errno))

typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char username[CLIENT_NAME_LEN];
    int ready_chat; // ready_chat = 1 khi tự mình gửi yêu cầu chat PVP hoặc join_room
    
    char with_room[ROOM_NAME_LEN]; // Sau khi join_room lưu hiện tại đang ở room nào
    int with_id;
} Client; 

typedef struct account_ {
    char username[CLIENT_NAME_LEN];
    char password[CLIENT_NAME_LEN];
    int accStatus;
    int user_id;
} Account;

typedef struct room_ {
    char owner_name[CLIENT_NAME_LEN];
    char room_name[ROOM_NAME_LEN];
    int owner_id;
    // JRB list_guest; // Cực kì cẩn thận khi đọc list_guest rất hay bị lỗi do list JRB ko sạch
    int arr_list_guest[MAX_GUEST_IN_ROOM];
    int number_member; // max near 300 guest with 1024 bytes
} Room;

int prompt_input(char const *message, char *buff); // Cấp phát động bộ nhớ cho buff
int prompt_input_ver2(char const *message, char *buff, int length);// Buff bộ nhớ cố định 

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

int sendData(int sockfd, char *data, int len);

int recvData(int sockfd, char *data, int len);

int len_of_number(int number);

#endif  // UTILS_H