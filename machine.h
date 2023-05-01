#ifndef KDL_MACHINE_H_INCLUDED
#define KDL_MACHINE_H_INCLUDED

#include <stdbool.h>

#include "def.h"
#include "parser.h"
#include "hashmap.h"

// For assertions only, never used seriously
#define KDL_DT_NIL 0
#define KDL_DT_INT 1
#define KDL_DT_FLT 2
#define KDL_DT_STR 3
#define KDL_DT_PRC 4

#define KDL_NFPARAMS 6

#include <stddef.h>

struct _kdl_machine_t;

typedef struct {
    int datatype;
    void *data;
} kdl_data_t;

typedef void(*kdl_function_t)(struct _kdl_machine_t *machine, const char *context, const char *name, kdl_data_t *params, size_t paramsLen);
typedef void(*kdl_watcher_t)(struct _kdl_machine_t *machine, const char *name, kdl_data_t *data);

typedef struct {
    kdl_function_t func;
    int datatypes[KDL_NFPARAMS];
    size_t datatypesLen;
    bool validate;
} kdl_verb_t;

typedef struct {
    char *name;
    kdl_data_t data;
    kdl_watcher_t watcher;
} kdl_entry_t;

typedef struct {
    kdl_rule_t *rules;
    size_t length;
    size_t size;
} kdl_programBuffer_t;

typedef struct _kdl_machine_t {
    kdl_program_t start; // Never changes
    kdl_programBuffer_t pbuf[2];
    size_t front;
    size_t back;

    kdl_state_t s;
    kdl_hashmap_t vars;
    kdl_hashmap_t verbs;
} kdl_machine_t;

void kdl_machine_setInt(kdl_machine_t *m, const char *name, kdl_int_t value);
void kdl_machine_setString(kdl_machine_t *m, const char *name, const char *value);
void kdl_machine_setFloat(kdl_machine_t *m, const char *name, kdl_float_t value);

int kdl_machine_getInt(kdl_machine_t *m, const char *name, kdl_int_t *value);
// The value is not allocated
int kdl_machine_getString(kdl_machine_t *m, const char *name, const char **value);
int kdl_machine_getFloat(kdl_machine_t *m, const char *name, kdl_float_t *value);

// If it doesn't exist, will create an integer of value zero
int kdl_machine_addWatcher(kdl_machine_t *m, const char *target, kdl_watcher_t callback);
void kdl_machine_addVerb(kdl_machine_t *m, const char *target, kdl_verb_t v);

void kdl_mkMachine(kdl_machine_t *out);
kdl_error_t kdl_machine_load(kdl_machine_t *machine, const char *program);
void kdl_machine_run(kdl_machine_t *machine);

void kdl_machine_free(kdl_machine_t *machine);

#endif

