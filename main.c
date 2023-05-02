#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

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

#define STR_BUF_SIZE 512

void cb_default(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    char buffer[STR_BUF_SIZE];
    snprintf(buffer, STR_BUF_SIZE, "push %s call", name);
    printf("Setting '%s'\n", buffer);
    kdl_machine_setInt(m, buffer, 1);
    for (size_t i = 0; i < length && i < 128; i++) {
        snprintf(buffer, STR_BUF_SIZE, "push %s _%lu", name, i);
        kdl_data_t p = params[i];
        switch (p.datatype) {
        case KDL_DT_INT:
            kdl_machine_setInt(m, buffer, *((kdl_int_t *)p.data));
            printf("Set %lu to %llu\n", i, *((kdl_int_t *)p.data));
            break;
        case KDL_DT_STR:
            kdl_machine_setString(m, buffer, (char *) p.data);
            break;
        case KDL_DT_FLT:
            kdl_machine_setFloat(m, buffer, *((kdl_float_t *)p.data));
            break;
        }
    }
}

void cb_result(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    printf("RESULT: %llu\n", *((kdl_int_t *)params[0].data));
    printf("Terminating program ungracefully (this is not a bad thing)...\n");
    exit(1);
}

void cb_write_int(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    kdl_machine_setInt(m, (char *) params[0].data, *((kdl_int_t*)params[1].data));
}

void cb_write_float(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    kdl_machine_setFloat(m, (char *) params[0].data, *((kdl_float_t*)params[1].data));
}

void cb_write_string(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    kdl_machine_setString(m, (char *) params[0].data, (char *) params[1].data);
}

void cb_do(kdl_machine_t *m, const char *context, const char *name, kdl_data_t *params, size_t length) {
    UNUSED(m);
    UNUSED(context);
    UNUSED(name);
    UNUSED(length);
    printf("Doing: %s...\n", (char *) params[0].data);
}

void initializeMachine(kdl_machine_t *machine) {
    kdl_verb_t verb;

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = false;
    verb.func = cb_print;
    kdl_machine_addVerb(machine, "print", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = true;
    verb.func = cb_write_int;
    verb.datatypesLen = 2;
    verb.datatypes[0] = KDL_DT_STR;
    verb.datatypes[1] = KDL_DT_INT;
    kdl_machine_addVerb(machine, "writeInt", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = true;
    verb.func = cb_write_float;
    verb.datatypesLen = 2;
    verb.datatypes[0] = KDL_DT_STR;
    verb.datatypes[1] = KDL_DT_FLT;
    kdl_machine_addVerb(machine, "writeFloat", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = true;

    verb.func = cb_write_string;
    verb.datatypesLen = 2;
    verb.datatypes[0] = KDL_DT_STR;
    verb.datatypes[1] = KDL_DT_STR;
    kdl_machine_addVerb(machine, "write", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = true;
    verb.func = cb_do;
    verb.datatypesLen = 1;
    verb.datatypes[0] = KDL_DT_STR;
    kdl_machine_addVerb(machine, "do", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = true;
    verb.func = cb_result;
    verb.datatypesLen = 1;
    verb.datatypes[0] = KDL_DT_INT;
    kdl_machine_addVerb(machine, "result", verb);

    memset(&verb, 0, sizeof(kdl_verb_t));
    verb.validate = false;
    verb.func = cb_default;
    kdl_machine_addDefVerb(machine, verb);
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    FILE *testFile = fopen("testFile.com", "r");
    assert(testFile);
    char buffer[2048];
    size_t written = fread(buffer, sizeof(char), 2047, testFile);
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

    initializeMachine(&machine);

    kdl_verb_t verb;
    verb.validate = false;
    verb.func = cb_print;
    verb.datatypesLen = 1;
    verb.datatypes[0] = KDL_DT_STR;
    kdl_machine_addVerb(&machine, "print", verb);
    kdl_machine_setString(&machine, "momomo", "TYRKEY");
    kdl_machine_setInt(&machine, "do", -50);

    for (size_t i = 0; i < 50; i++) {
        printf("Running machine.\n");
        kdl_machine_run(&machine);
    }
    printf("No segfaults!\n");
    kdl_machine_free(&machine);
    return 0;
}
