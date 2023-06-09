#ifndef PSD_LIB_H_INCLUDED
#define PSD_LIB_H_INCLUDED

// Local
#include "psd_hashmap.h"

// Base
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// -- Error codes ---

// No error
#define PSD_LIB_EOK 0
// Out of memory
#define PSD_LIB_ENOMEM 1
// I/O error
#define PSD_LIB_EIO 2
// Probable read only corruption (very bad, probably)
#define PSD_LIB_ECORRUPTION 3
// Max buffer size exceeded
#define PSD_LIB_EBUFFER 4
// Network error
#define PSD_LIB_ENET 5
// Regex compilation
#define PSD_LIB_ERCMP 6
// Regex execution
#define PSD_LIB_EREXE 7
// Regex (execution) no match
#define PSD_LIB_ERMATCH 8

// --- Structures ---

typedef struct {
    const char *message;
    int code;
} psd_err;

typedef struct {
    const char *const *values;
    size_t length;
} psd_fn_params;

typedef struct {
    char *data;
    size_t length;
} psd_fn_result;

typedef struct {
    psd_fn_result *list;
    size_t length;
} psd_fn_out;

typedef struct {
    char *data;
    size_t size;
    size_t length;
} psd_errMsgBuf;

typedef struct {
    double number;
    const char *string;
} psd_sqlParam;

typedef struct {
    // Maps files to temporary file handles
    psd_hashmap filesystem;
    psd_errMsgBuf errMsg;
} psd_state;

// --- Error handling ---

bool psd_isError(psd_err e);

// --- Library functions ---

// Uses POSIX Extended Regex
// This is NOT Perl-style
// See: https://en.wikibooks.org/wiki/Regular_Expressions/POSIX-Extended_Regular_Expressions
psd_err psd_fn_scrape_regex(psd_state *s, const char *url, const char *regex, const psd_fn_params params, psd_fn_out *out);
psd_err psd_fn_scrape_xpath(psd_state *s, const char *url, const char *xpath, const psd_fn_params params, psd_fn_out *out);
psd_sqlparam psd_mkParam_double(double val);
// NOTE: Does not copy the string, only records the pointer
psd_sqlparam psd_mkParam_string(const char *str);
// The index in `params` is treated as the second parameter described here https://www.sqlite.org/c3ref/bind_blob.html
psd_err psd_fn_sql(psd_state *s, const char *sql, const psd_sqlParam *params, psd_fn_out *out);
psd_err psd_fn_validate(psd_state *s, const char *value, const char *regex, bool *validation);

// --- Control ---

// Error: PSD_LIB_EIO
psd_err psd_reset(psd_state *s);
// Error: PSD_LIB_EIO
psd_err psd_commit(psd_state *s);

// --- Initialization ---

psd_err psd_init(psd_state *s);
psd_err psd_globalInit();
void psd_globalCleanup();

// --- Memory ---

void psd_free(psd_state *s);
void psd_free_out(psd_fn_out *out);
void psd_free_error(psd_err *e);

#endif
