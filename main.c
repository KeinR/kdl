#include <stdio.h>
#include <sys/param.h>
#include <string.h>

#include "machine.h"

// Clever https://stackoverflow.com/a/3599170
#define UNUSED(x) (void)(x)

void cb_print(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    printf("vm::print: \"%s\"\n", (char *) params[0].data);
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    const char *input = "(1 + 0 ? print [Hewlow world] :: (? print [More hwllo])\n   (? print [uwu desu] :: (? print momomo)))";
    kdl_machine_t machine;
    kdl_mkMachine(&machine);
    kdl_error_t error = kdl_machine_load(&machine, input);
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
    verb.func = cb_print;
    verb.datatypesLen = 1;
    verb.datatypes[0] = KDL_DT_STR;
    kdl_machine_addVerb(&machine, "print", verb);
    kdl_machine_setString(&machine, "momomo", "TYRKEY");
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
