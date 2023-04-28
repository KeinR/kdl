#include <stdio.h>
#include <sys/param.h>
#include <string.h>

#include "parser.h"

int main(int argc, char **argv) {
    const char *input = "(530 = 0 ? record [this])";
    kdl_program_t program;
    kdl_error_t error = kdl_parse(input, &program);
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
