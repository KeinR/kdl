#ifndef KDL_TOKENIZER_H_INCLUDED
#define KDL_TOKENIZER_H_INCLUDED

#include "error.h"

#include <stddef.h>

#define KDL_TK_CTRL 0
#define KDL_TK_WORD 1
#define KDL_TK_NUMBER 2
// Genuinely don't think I need strings lol
// #define KDL_TK_STRING

typedef struct {
    int type;
    const char *value;
    size_t valueLen;
} kdl_token_t;

typedef struct {
    kdl_token_t *tokens;
    size_t nTokens;
} kdl_tokenization_t;

kdl_error_t kdl_tokenize(const char *input, kdl_tokenization_t *out);

#endif

