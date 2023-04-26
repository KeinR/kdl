#ifndef KDL_TOKENIZER_H_INCLUDED
#define KDL_TOKENIZER_H_INCLUDED

#include "error.h"

#include <stddef.h>

#define KDL_TK_CTRL  0
#define KDL_TK_WORD  1
#define KDL_TK_INT   2
#define KDL_TK_FLOAT 3
#define KDL_TK_PERC  4
#define KDL_TK_VAR   5
#define KDL_TK_STR   6

#define KDL_OP_NOOP   -1
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

typedef kdl_int_t long long;
typedef kdl_float_t long double;

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
    // Empty string if root context
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

typedef struct kdl_rule_t;

typedef struct {
    kdl_rule_t *rules;
    size_t length;
} kdl_program_t;

typedef struct {
    // Never NULL
    // Empty string if root
    char *context;
    // ie function to execute
    char *verb;
    kdl_compute_t *params;
    size_t nParams;
} kdl_action_t;

typedef struct {
    kdl_program_t child;
    kdl_action_t order;
} kdl_execute_t;

typedef struct {
    kdl_execute_t set;
    kdl_compute_t get;
} kdl_rule_t;

kdl_error_t kdl_tokenize(const char *input, kdl_tokenization_t *out);
kdl_error_t kdl_build(kdl_tokenization_t tokens, kdl_program_t *out);

#endif

