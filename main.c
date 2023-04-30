#include <stdio.h>
#include <sys/param.h>
#include <string.h>

#include "machine.h"

int main(int argc, char **argv) {
    const char *input = "(530 = 0 ?  record [this])";
    kdl_machine_t machine;
    kdl_error_t error = kdl_mkMachine(input, &machine);
    if (error.code != KDL_ERR_OK) {
        printf("ERROR: %s\n", error.message);
        if (error.hasDataLen) {
            printf("At: %.*s\n", (int) error.dataLen, error.data);
        } else {
            size_t len = strlen(error.data);
            printf("At: '%.*s\n'", (int) MIN(10, len), error.data);
        }
    }
    printf("No segfaults!\n");
    return 0;
}
