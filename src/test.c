#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jrb.h"
#include "jval.h"
#include "utils.h"

int main(int argc, char const *argv[]) {
    Client cli1 = (Client) malloc(sizeof(struct client));
    JRB node, clients = make_jrb();
    cli1->id = 1;

    jrb_insert_int(clients, cli1->id, new_jval_v(cli1));

    jrb_traverse(node, clients) {
        Client buff = (Client) jval_v(node->val);
        printf("Id: %d\n", buff->id);
    }
    char test[BUFF_SIZE] = {0};
    printf("Test\n");

    return 0;
}
