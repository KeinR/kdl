#ifndef KDL_TOKENIZER_H_INCLUDED
#define KDL_TOKENIZER_H_INCLUDED

#include "error.h"

#include <stddef.h>

#define KDL_TK_CTRL 0
#define KDL_TK_WORD 1
#define KDL_TK_NUMBER 2

typedef struct {
    int type;
    const char *value;
    size_t valueLen;
} kdl_token_t;

typedef struct {
    kdl_token_t *tokens;
    size_t nTokens;
} kdl_tokenization_t;

typedef struct {
    // Never NULL
    // Empty string if root context
    char *context;
    // OP code
    int op;
    // Is NULL if no value
    // Dynamically allocated
    char *str;
    // To help with precision, keep as int whenever possible
    float number;
    int integer;
} kdl_op_t;

typedef struct {
    kdl_op_t *opers;
    size_t length;
} kdl_compute_t;

typedef struct {
    // <>!= comparisons and like
    int op;
    // !!!!!!!!!
    bool negate;
    // Operands to the operation
    kdl_compute_t a;
    kdl_compute_t b;
    // Logical operator (might be one)
    // If not an operator it's a raw value
    // AND or OR or NOTHING
    int lop;
} kdl_logic_t;

typedef struct {
    // Stack based logic
    kdl_logic_t *logic;
    size_t length;
} kdl_get_t;

typedef struct kdl_rule_t;

typedef struct {
    kdl_rule_t *rules;
    size_t length;
} kdl_program_t;

typedef struct {
    kdl_program child;
    // Never NULL
    // Empty string if root context
    char *context;
    // NULL if none
    char *verb;
    kdl_compute_t *params;
} kdl_set_t;

typedef struct {
    kdl_set_t set;
    kdl_get_t get;
} kdl_rule_t;

kdl_error_t kdl_tokenize(const char *input, kdl_tokenization_t *out);
kdl_error_t kdl_build(kdl_tokenization_t tokens, kdl_program_t *out);

#endif

