#ifndef protocol
#define protocol
enum msg_type {
    SIGN_IN,
    SIGN_UP,
    SIGN_OUT,
    CREATE_NEW_ROOM,
    DELETE_ROOM,
    JOIN_ROOM,
    OUT_ROOM,
    INVITE_JOIN_ROOM,
    SHOW_LIST_ROOMS,
    SHOW_LIST_USERS, // user is online - logined
    PVP_CHAT,
    CONTACT,

    AUTH_SCREEEN,
    SELECT_ROOM_SCREEN,
    CHAT_IN_ROOM_SCREEN,

    SUCCESS,
    FAILED,
    ERROR,
    EXIT,
};


struct msg_payload {
    char userName[100];
    int userId;
    char password[100];
    char message[100];
};

struct message_ {
    enum msg_type type;
    struct msg_payload payload;
};

typedef struct message_ Message;

void send_protocol() {
    // 1 username password
}

#endif  // end