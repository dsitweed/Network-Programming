#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jrb.h"
#include "jval.h"
#include "utils.h"

int main(int argc, char const *argv[]) {
    // Client cli1 = (Client)malloc(sizeof(struct client));
    // JRB node, clients = make_jrb();
    // JRB accounts = make_jrb();
    
    // FILE *accFile = fopen("./account.txt", "rt");
    // if (accFile == NULL) {
    //     printf("Cannot open %s\n", "account.txt");
    //     PRINT_ERROR;
    //     return EXIT_FAILURE;
    // }

    // while (1) {
    //     Account *acc = (Account *) malloc(sizeof(Account));
    //     if (fscanf(accFile, "%s %s %d\n", acc->username, acc->password, &acc->accStatus) == EOF) break;
    //     printf("%s %s %d\n", acc->username, acc->password, acc->accStatus);
    //     jrb_insert_str(accounts, acc->username, new_jval_v(acc));
    // }

    // jrb_traverse(node, accounts) {
    //     Account *acc1 = (Account *)(jval_v(node->val));
    //     printf("%s - %s - %d\n", acc1->username, acc1->password, acc1->accStatus);
    // }
    // printf("Test\n");

    JRB rooms = make_jrb(), node;
    Room room1 = make_room();

    strcpy(room1.room_name, "fist room");
    printf("%s\n", room1.room_name);

    jrb_insert_str(rooms, room1.room_name, new_jval_v(&room1));

    jrb_traverse(node, rooms) {
        Room roomBuff = *(Room *) jval_v(node->val);
        printf("%s\n", roomBuff.room_name   );
    }
    return 0;
}
