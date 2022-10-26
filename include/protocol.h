#ifndef protocol
#define protocol
enum msg_type {
    SIGN_IN,
    SIGN_UP,
    SIGN_OUT,
    CREATE_ROOM,
    JOIN_ROOM,
    OUT_ROOM,
    INVITE_JOIN_ROOM,
    CONTACT,

    SUCCESS,
    ERROR
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
    Message mess;
    // 1 username password
}

#endif  // end