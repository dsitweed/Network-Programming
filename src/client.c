#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>  // signal
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "jrb.h"
#include "jval.h"
#include "protocol.h"
#include "utils.h"

/*
typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char *username;
} * Client;  // client is poiter
*/

// global variables

/* An integer type which can be accessed as an atomic
entity even in the presence of asynchronous interrupts made by signals.*/
volatile sig_atomic_t exit_flag = 0;
char clientName[CLIENT_NAME_LEN] = "Ky";
int sockfd = 0;  // socket of this client

void signal_handler(int sig);
void send_msg_handler();
void recv_msg_handler();

typedef union chatWith
{
    char with_room[33];
    int with_id;
} ChatWith;

ChatWith with;


// return 0 error
// return 1 -> ... is success
int auth_menu(int sockfd) {
    int menu, err = 0;
    char buff[BUFF_SIZE] = {0};
    int response;
    Account acc;

    /* send navigate screen control */
    do {
        bzero(buff, sizeof(buff));
        sprintf(buff, "%d", AUTH_SCREEEN);
        send(sockfd, buff, strlen(buff), 0);

        printf(
            "===== THE CHATROOM AUTH SCREEN ===== \n"
            "1. Register - Sign up\n"
            "2. Sign in \n"
            "3. Exit\n"
            "Your choice (1-3): ");
        scanf("%d%*c", &menu);
        switch (menu) {
            case 1:
                printf("SIGN UP\n");
                prompt_input_ver2("Nhap username: ", acc.username);
                prompt_input_ver2("Nhap password: ", acc.password);

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", SIGN_UP, acc.username, acc.password);
                send(sockfd, buff, strlen(buff), 0);

                bzero(buff, sizeof(buff));
                err = recv(sockfd, buff, sizeof(buff), 0);
                if (err >= 0) {
                    response = atoi(buff);
                    if (response == SUCCESS) {
                        printf("success\n");
                    }
                    if (response == FAILED) {
                        printf("failed\n");
                    }
                }
                break;
            case 2:
                printf("SIGN IN\n");
                prompt_input_ver2("Nhap username: ", acc.username);
                prompt_input_ver2("Nhap password: ", acc.password);

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", SIGN_IN, acc.username, acc.password);
                err = send(sockfd, buff, strlen(buff), 0);

                bzero(buff, sizeof(buff));
                err = recv(sockfd, buff, sizeof(buff), 0);
                printf("%s - %ld\n", buff, strlen(buff));
                if (err >= 0) {
                    response = atoi(buff);
                    if (response == SUCCESS) {
                        printf("success\n");
                        strcpy(clientName, acc.username);
                        return 1;
                    }
                    if (response == FAILED) {
                        printf("failed\n");
                    }
                } else
                    printf("Error\n");
                break;
            case 3:
                printf("EXIT\n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s", SIGN_OUT, clientName);
                err = send(sockfd, buff, strlen(buff), 0);
                return 0;
                break;
            default:
                printf("Please select valid options\n");
                break;
        }  // end switch
    } while (menu != 5);
    return 1;
}

int select_room_menu(int sockfd) {
    int menu, err = 0;
    char buff[BUFF_SIZE * 2] = {0};
    int response;
    int exit_flag = 0;
    Room *new_room = (Room *)malloc(sizeof(Room));

    bzero(with.with_room, sizeof(with.with_room));
    /* send navigate screen control */
    do {
        bzero(buff, sizeof(buff));
        sprintf(buff, "%d", SELECT_ROOM_SCREEN);
        send(sockfd, buff, strlen(buff), 0);

        printf(
            "===== THE SELECT ROOM SCREEN ===== \n"
            "1. Create new room\n"
            "2. Show list rooms \n"
            "3. Join room by name \n"
            "4. Show list users \n"
            "5. Chat PvP with 1 user by id \n"
            "6. Sign out of chat room \n"
            "Your choice (1-6): ");
        scanf("%d%*c", &menu);
        switch (menu) {
            case 1:
                printf("Create new room\n");
                prompt_input_ver2("Name of room: ", new_room->room_name);
                strcpy(new_room->owner_name, clientName);

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", CREATE_NEW_ROOM, new_room->room_name, new_room->owner_name);
                send(sockfd, buff, strlen(buff), 0);

                break;
            case 2:
                printf("Show list rooms\n");
                printf("%-20s %s\n", "Room ID", "Room Owner name");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", SHOW_LIST_ROOMS);
                send(sockfd, buff, strlen(buff), 0);

                bzero(buff, sizeof(buff));
                err = recv(sockfd, buff, sizeof(buff), 0);
                if (err >= 0) {
                    printf("%s", buff);
                }

                break;
            case 3:
                printf("Join room by name\n");
                char room_name[100] = {0};
                prompt_input_ver2("Input room name: ", with.with_room);

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s", JOIN_ROOM, room_name);
                send(sockfd, buff, strlen(buff), 0);

                menu = 6; // break out while
                break;
            case 4:
                printf("Show list users \n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", SHOW_LIST_USERS);
                send(sockfd, buff, strlen(buff), 0);

                bzero(buff, sizeof(buff));
                err = recv(sockfd, buff, sizeof(buff), 0);
                if (err >= 0) {
                    printf("%s", buff);
                }
                break;
            case 5: {
                int friend_id;
                printf("Chat PvP with 1 user by id \n");
                bzero(buff, sizeof(buff));
                printf("Input friend id: ");
                scanf("%d%*c", &with.with_id);

                sprintf(buff, "%d %d", PVP_CHAT, friend_id);
                send(sockfd, buff, strlen(buff), 0);

                menu = 6; // break out while
                break;
            }
            case 6:
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", SIGN_OUT);
                send(sockfd, buff, strlen(buff), 0);

                exit_flag = 1;
                break;
            default:
                printf("Please select valid options\n");
                break;
        }  // end switch

        /* check response from client  */
        bzero(buff, sizeof(buff));
        err = recv(sockfd, buff, sizeof(buff), 0);
        if (err >= 0) {
            response = atoi(buff);
            if (response == SUCCESS) {
                printf("success\n");
            }
            if (response == FAILED) {
                printf("failed\n");
            }
        }
    } while (menu != 6 && exit_flag != 1);

    if (exit_flag == 1) return EXIT;
    return 1;
}

// return 1 no error
int chat_in_room_menu(int sockfd) {
    char buff[BUFF_SIZE];
    pthread_t send_mesg_thread;
    pthread_t recv_mesg_thread;

    signal(SIGINT, signal_handler);

    bzero(buff, sizeof(buff));
    sprintf(buff, "%d", CHAT_IN_ROOM_SCREEN);
    send(sockfd, buff, strlen(buff), 0);

    printf("===== WELCOME TO THE CHATROOM =====\n");

    if (pthread_create(&send_mesg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&recv_mesg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    // communicate with server
    while (1) {
        if (exit_flag) {
            printf("\nEND CHATROOOM.\n");
            break;
        }
    }

    return 1;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[1]);
        return EXIT_FAILURE;
    }

    int err;
    struct sockaddr_in server_addr;
    pthread_t send_mesg_thread;
    pthread_t recv_mesg_thread;
    char *IP = "127.0.0.1";
    int PORT = atoi(argv[1]);

    // signal(SIGINT, signal_handler);

    /*Socket settings*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(PORT);

    /*Connect to server*/
    err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf("ERROR: connect\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* Check login - Auth_screen */
    err = auth_menu(sockfd);
    if (err == 0) exit_flag = 1;

    while (1) {
        if (exit_flag == 1) break;
        /* Select room  */
        err = select_room_menu(sockfd);
        if (err == EXIT) break;

        /* Chat PvP or Chat with Room */
        err = chat_in_room_menu(sockfd);
    }

    // if (exit_flag == 0) {
    //     printf("===== WELCOME TO THE CHATROOM =====\n");

    //     if (pthread_create(&send_mesg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
    //         printf("ERROR: pthread\n");
    //         return EXIT_FAILURE;
    //     }

    //     if (pthread_create(&recv_mesg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
    //         printf("ERROR: pthread\n");
    //         return EXIT_FAILURE;
    //     }
    // }

    // // communicate with server
    // while (1) {
    //     if (exit_flag) {
    //         printf("\nEND CHATROOOM.\n");
    //         break;
    //     }
    // }

    close(sockfd);  // have to close socket
    return EXIT_SUCCESS;
}

void signal_handler(int sig) { exit_flag = 1; }

void send_msg_handler() {
    char message[BUFF_SIZE];
    char buffer[BUFF_SIZE + CLIENT_NAME_LEN + 3];  // = message + name of user + 3 (for 3 time \0)

    while (1) {
        printf("> You: ");
        fgets(message, sizeof(message), stdin);
        str_trim_lf(message);

        if (strcmp(message, "exit") == 0) {
            break;
        } else {
            sprintf(buffer, "%s: %s", clientName, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }  // end send message

        bzero(message, sizeof(message));
        bzero(buffer, sizeof(buffer));
    }

    signal_handler(1);
}

void recv_msg_handler() {
    char message[BUFF_SIZE] = {0};
    int received = 0;

    while (1) {
        received = recv(sockfd, message, sizeof(message), 0);

        if (received > 0) {
            clear_line();
            printf("> %s\n", message);
            printf("> You: ");
            fflush(stdout);
        } else if (received == 0) {
            break;
        } else {
            // -1
        }

        memset(message, 0, sizeof(message));
    }
}