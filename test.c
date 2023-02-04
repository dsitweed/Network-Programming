#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char string[] = "123123" ;
    char tmp[] = "user1";
    strcpy(string, tmp);

    printf("%s - %s\n", string, tmp);
}