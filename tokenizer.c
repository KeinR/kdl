#include "tokenizer.h"

#include <stdlib.h>

#define TOKEN_BUFFER_SIZE 512

// TODO: Handle OOM errors

static int getToken(const char *input, size_t *offset, size_t *legnth, int *type);

psd_error_t getToken(const char *input, kdl_token_t *token) {
    size_t i = 0;
    size_t l = 0;
    int t = KDL_TK_CTRL;
    for (; input[i] <= 0x20; i++);
    char c = input[i];
    char seek = '\0';
    if (input[i]) {
        seek = input[i+1];
    }

    switch (c) {
    case '\0':
        return kdl_mkError1(PSD_ERR_FAIL);
    case '>':
        if (seek == '>' || seek == '=') {
            len = 2;
        } else {
            len = 1;
        }
        break;
    case '<':
    case '!':
        if (seek == '=') {
            len = 2;
        } else {
            len = 1;
        }
        break;
    case '$':
    case '#':
    case '@':
    case '/':
    case '*':
    case '-':
    case '+':
    case '=':
    case ';':
    case ',':
    case '~':
    case ':':
    case '(':
    case ')':
    case '&':
    case '|':
    case '.':
    case '?':
        len = 1;
        break;
    default:
        if (c >= '0' && c <= '9') {
            t = KDL_TK_NUMBER;
            for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            if (input[i] == '.') {
                for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            }
        } else {
            t = KDL_TK_WORD;
            for (;  (input[i+l] >= 'a' && input[i+l] <= 'a') ||
                    (input[i+l] >= 'A' && input[i+l] <= 'Z') ||
                    (input[i+l] >= '0' && input[i+l] <= '9') ||
                    input[i+l] == '_'
                    ; l++);
        }
    }
    token->value = input + i;
    token->valueLen = l;
    token->type = t;
    return kdl_noError();
}

kdl_error_t kdl_tokenize(const char *input, kdl_tokenization_t *out) {
    size_t size = TOKEN_BUFER_SIZE;
    kdl_tokenization_t result;
    result.nTokens = 0;
    result.tokens = (kdl_token_t *) malloc(sizeof(kdl_token_t) * size);
    bool looping = true;
    size_t offset = 0;
    kdl_token_t token;
    while (!kdl_isError(getToken(input + offset, &token))) {
        if (result.nTokens + 1 > size) {
            size += TOKEN_BUFFER_SIZE;
            result.tokens = (kdl_token_t *) realloc(result.tokens, sizeof(kdl_token_t) * size);
        }
        result.tokens[result.nTokens] = token;
        result.nTokens++;
        offset += wordOfs + wordLen;
    }

    result.tokens = (kdl_token_t *) realloc(result.tokens, sizeof(kdl_token_t) * result.nTokens);

    *out = result;

    return kdl_noError();
}
