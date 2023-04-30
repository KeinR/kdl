#ifndef KDL_ERROR_H_INCLUDED
#define KDL_ERROR_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>

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
// Undefined reference (runtime)
#define KDL_ERR_REF 6
// Type mismatch
#define KDL_ERR_TYP 7
// Not found
#define KDL_ERR_NTF 8

typedef long long kdl_int_t;
typedef long double kdl_float_t;

typedef struct {
    void*(*malloc)(size_t);
    void*(*realloc)(void*,size_t);
    void(*free)(void*);
} kdl_state_t;

typedef struct {
    const char *data;
    size_t dataLen;
    bool hasDataLen;

    int code;
    const char *message;
} kdl_error_t;

#endif
