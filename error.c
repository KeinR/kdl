#include "error.h"

kdl_error_t kdl_noError() {
    return mkError(KDL_ERR_OK, "");
}

kdl_error_t kdl_mkError1(int code) {
    return kdl_mkError(code, "");
}

kdl_error_t kdl_mkError(int code, const char *message) {
    kdl_error_t error;
    error.code = code;
    error.message = message;
    return error;
}
