#ifndef KDL_ERROR_H_INCLUDED
#define KDL_ERROR_H_INCLUDED

#define KDL_ERR_OK 0
#define KDL_ERR_MEM 1
#define KDL_ERR_FAIL 2

typedef struct {
    int code;
    const char *message;
} kdl_error_t;

kdl_error_t kdl_noError();
kdl_error_t kdl_mkError(int code, const char *message);
kdl_error_t kdl_mkError1(int code);

#endif
