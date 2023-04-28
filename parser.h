#ifndef KDL_PARSER_H_INCLUDED
#define KDL_PARSER_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>

// TODO:
// In getCompute()
//      get precidence from map created from KDL_OP_* defs instead of hard
//      coding it?
//      Or add defs for it

#define KDL_ERR_OK  0
// Expected a character to be somewhere
#define KDL_ERR_EXP 1
// Unexpected character
#define KDL_ERR_UNX 2
// Unexpected EOF
#define KDL_ERR_EOF 3
// Value unsupported
#define KDL_ERR_VAL 4
// A static buffer size was exceeded
#define KDL_ERR_BUF 5

// KDL_TK_NULL trigger assertions if given to the wrong places.
// So try not to use it.
// Or be careful.
// Or do neither.
#define KDL_TK_NULL -1
#define KDL_TK_CTRL  0
#define KDL_TK_WORD  1
#define KDL_TK_INT   2
#define KDL_TK_FLOAT 3
#define KDL_TK_PERC  4
#define KDL_TK_VAR   5
#define KDL_TK_STR   6

#define KDL_OP_NOOP  -1
#define KDL_OP_PINT   0
#define KDL_OP_PFLOAT 1
#define KDL_OP_PSTR   2
#define KDL_OP_PVAR   3
#define KDL_OP_ADD    4
#define KDL_OP_SUB    5
#define KDL_OP_DIV    6
#define KDL_OP_MUL    7
#define KDL_OP_EQU    8
// Less than or greater to, etc.
#define KDL_OP_LEQ    9
#define KDL_OP_GEQ    10
// Less than, greater than
#define KDL_OP_LTH    11
#define KDL_OP_GTH    12
#define KDL_OP_AND    13
#define KDL_OP_OR     14
#define KDL_OP_NOT    15
#define KDL_OP_PPERC  16

typedef long long kdl_int_t;
typedef long double kdl_float_t;

typedef struct {
    int type;
    // Re-use pointer locations in the input string lol
    // Hence the length property and the constant qualifier.
    // Do not free.
    // Not null terminated (probably).
    // Primarially for error reporting.
    // Secondairally for memory saving (lol)
    const char *value;
    size_t valueLen;
} kdl_token_t;

typedef struct {
    kdl_token_t *tokens;
    size_t nTokens;
} kdl_tokenization_t;

typedef struct {
    // NULL if not needed (eg. this is a literal)
    // Empty string or NULL if root context
    char *context;
    // Of the type KDL_OP_*
    // The operation to perfrom on the stack
    int op;
    // Is NULL if no value
    void *value;
} kdl_op_t;

typedef struct {
    kdl_op_t *opers;
    size_t length;
} kdl_compute_t;

struct kdl_rule_p;

typedef struct {
    struct kdl_rule_p *rules;
    size_t length;
} kdl_program_t;

typedef struct {
    // Empty string if root
    // Should be NULL if the verb is NULL
    // (verb uses context, you see)
    char *context;
    // ie function to execute
    // Is NULL if there is nothing todo
    char *verb;
    kdl_compute_t *params;
    size_t nParams;
} kdl_action_t;

typedef struct {
    kdl_program_t child;
    kdl_action_t order;
} kdl_execute_t;

typedef struct kdl_rule_p {
    kdl_compute_t compute;
    kdl_execute_t execute;
} kdl_rule_t;

typedef struct {
    const char *data;
    size_t dataLen;
    bool hasDataLen;

    int code;
    const char *message;
} kdl_error_t;

typedef struct {
    void*(*malloc)(size_t);
    void*(*realloc)(void*,size_t);
    void(*free)(void*);
} kdl_state_t;

kdl_error_t kdl_parse(const char *input, kdl_program_t *program);

#endif

