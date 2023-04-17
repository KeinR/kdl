#ifndef PSD_H_INCLUDED
#define PSD_H_INCLUDED

// Base
#include <stdbool.h>
#include <setjmp.h>

// --- Typedefs ---

typedef unsigned long psd_size_t;

// --- Structures ---

typedef struct {
    const char **values;
    psd_size_t length;
} psd_fn_params;

typedef struct {
    char *data;
    psd_size_t length;
} psd_fn_result;

typedef struct {
    psd_fn_result *list;
    psd_size_t length;
} psd_fn_out;

typedef struct {
    jmp_buf jumpBuf;
    char tmpDir[16];
} psd_state;

// --- Typedefs ---

typedef void (*psd_callback_t)(psd_state*, void*);

// --- Library functions ---

void psd_fn_scrape_regex(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *regex, psd_fn_out *out);
void psd_fn_scrape_xpath(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *xpath, psd_fn_out *out);
void psd_fn_validate(psd_state *s, bool effect, const char *value, const char *regex);
void psd_fn_sql(psd_state *s, bool effect, const char *sql, const psd_fn_params, psd_fn_out *out);

// --- Initialization ---

void psd_start(psd_callback_t callback, void *userData);
bool psd_init();
void psd_uninit();

// --- Memory ---

void psd_free_out(psd_fn_out *out);

#endif
