#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include <assert.h>

#include "machine.h"

// Clever https://stackoverflow.com/a/3599170
#define UNUSED(x) (void)(x)

void cb_print(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    printf("vm::print:");
    for (size_t i = 0; i < length; i++) {
        switch(params[i].datatype) {
        case KDL_DT_INT:
            printf(" %llu", *((kdl_int_t *) params[i].data));
            break;
        case KDL_DT_FLT:
            printf(" %Lf", *((kdl_float_t *) params[i].data));
            break;
        case KDL_DT_STR:
            printf(" %s", (char *) params[i].data);
            break;
        }
    }
    printf("\n");
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    FILE *testFile = fopen("testFile.com", "r");
    assert(testFile);
    char buffer[512];
    size_t written = fread(buffer, sizeof(char), 511, testFile);
    buffer[written] = '\0';

    printf("Running program:\n-----------\n");
    printf("%s", buffer);
    printf("\n-----------\n");

    kdl_machine_t machine;
    kdl_mkMachine(&machine);
    kdl_error_t error = kdl_machine_load(&machine, buffer);
    if (error.code != KDL_ERR_OK) {
        printf("ERROR: %s\n", error.message);
        if (error.hasDataLen) {
            printf("At: %.*s\n", (int) error.dataLen, error.data);
        } else {
            size_t len = strlen(error.data);
            printf("At: '%.*s\n'", (int) MIN(10, len), error.data);
        }
    }
    kdl_verb_t verb;
    verb.validate = false;
    verb.func = cb_print;
    verb.datatypesLen = 1;
    verb.datatypes[0] = KDL_DT_STR;
    kdl_machine_addVerb(&machine, "print", verb);
    kdl_machine_setString(&machine, "momomo", "TYRKEY");
    kdl_machine_setInt(&machine, "do", -50);
    printf("Running machine.\n");
    kdl_machine_run(&machine);
    printf("Running machine2.\n");
    kdl_machine_run(&machine);
    printf("Running machine3.\n");
    kdl_machine_run(&machine);
    printf("No segfaults!\n");
    kdl_machine_free(&machine);
    return 0;
}
