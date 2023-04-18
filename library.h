#ifndef PSD_LIB_H_INCLUDED
#define PSD_LIB_H_INCLUDED

// Local
#include "psd_map.h"

// Base
#include <stdbool.h>

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

// --- Structures ---

typedef struct {
    const char *message;
    int code;
} psd_err;

typedef struct {
    const char **values;
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
    char *buf;
    int size;
    FILE *handle;
} psd_msgBuf;

typedef struct {
    // Maps files to temporary file handles
    psd_map filesystem;
} psd_state;

// --- Library functions ---

void psd_fn_scrape_regex(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *regex, psd_fn_out *out);
void psd_fn_scrape_xpath(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *xpath, psd_fn_out *out);
void psd_fn_sql(psd_state *s, bool effect, const char *sql, const psd_fn_params, psd_fn_out *out);
void psd_fn_validate(psd_state *s, bool effect, const char *value, const char *regex);

// --- Control ---

void psd_reset(psd_state *s);
// Error: 
psd_err psd_commit(psd_state *s);

// --- Initialization ---

void psd_init(psd_state *s);
bool psd_globalInit();
void psd_globalCleanup();

// --- Memory ---

void psd_free(psd_state *s);
void psd_free_out(psd_fn_out *out);

#endif
