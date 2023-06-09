// Local
#include "library.h"

// Base
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <regex.h> // Non-standard lol get out of here w*ndows
#include <sys/param.h>

// External
#include <sqlite3.h>
#include <curl/curl.h>
#include <openssl/md5.h>

// 0.25 GB
#define MAX_FILE_SIZE 0xFFFFFFF

#define MAX_REGEX_MATCHES 128

// This is actually apparently what it is on linux, though I care at least
// somewhat about cross compatability so I won't bother with the header.
// linux/limits.h btw
#define PATH_MAX_LEN 4096

#define FILE_COPY_BUFFER_SIZE 0xFFFF

// This ^2 is the # of buckets
#define BUCKET_PRECISION 5

// How long we are okay with iterating over a null-terminated string
#define STRING_OVERRUN_MAX 0xFFFF

// -- Private structure definitions ---

typedef struct {
    char *data;
    size_t length;
} buffer;

typedef psd_err(*fsCallback_t)(char*, FILE*);

// Automatically frees data if out of memory
static psd_err dataAppend(char **data, size_t *length, size_t *size, size_t buffer, char value);
static psd_err formatString(const char *template, psd_fn_params params, buffer *out);
// WARNING: Continues despite errors
static psd_err filesystemPerform(psd_state *s, fsCallback_t callback);
static psd_err callback_filesystemCommit(char *key, const char *value);
// Returns error: PSD_LIB_EOK, PSD_LIB_EIO
static psd_err copyFile(FILE *src, FILE *dest);
static void saveRegexError(int code, regex_t *handle, psd_errMsgBuf *errMsg);
// out and nMatches may be NULL
static psd_err regexMatch(const char *string, const char *regex, regmatch_t **out, int *nMatches, psd_errMsgBuf *errMsg);
static psd_err startsWith(const char *str, const char *sequence, bool *result);
static psd_sqlParam mkSqlParam(double number, const char *string);
static long fileSize(FILE *fp);
// All 3 ensure null terminator
static psd_err urlGet(const char *url, buffer *out);
static psd_err fileGet(const char *path, buffer *out);
static psd_err resourceGet(const char *url, buffer *out);
static size_t callback_curl_writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
static buffer mkBuffer();
static void freeBuffer(buffer *buf);
static psd_err mkError(int code, const char *message);
static psd_err noError();
static void getTmpName(char *name);

// --- Static helper functions ---

psd_err dataAppend(char **data, size_t *length, size_t *size, size_t buffer, char value) {
    if (*length >= *size) {
        *size = *length + buffer;
        char *b = (char *) realloc(*data, sizeof(char) * (*size));
        if (b == NULL) {
            free(*data);
            *data = NULL;
            *length = 0;
            *size = 0;
            return mkError(PSD_LIB_ENOMEM, "Realloc append to buffer");
        }
        *data = b;
    }
    (*data)[*length] = value;
    (*length)++;
    return noError();
}

psd_err formatString(const char *template, psd_fn_params params, buffer *out) {
    const int bufBuf = 512;
    size_t length = 0;
    size_t size = bufBuf;
    char *buffer = (char *) malloc(sizeof(char) * size);
    for (size_t i = 0; template[i]; i++) {
        char c = template[i];
        bool proceed = true;
        if (c == '$') {
            char n = template[i+1];
            if (n >= '0' && n <= '9') {
                size_t index = n - '0';
                if (index < params.length) {
                    proceed = false;
                    i++;
                    const char *value = params.values[index];
                    for (size_t f = 0; value[f]; f++) {
                        psd_err e = dataAppend(&buffer, &length, &size, bufBuf, value[f]);
                        // Don't worry, dataAppend's only error is NOMEM, and if
                        // that is the case, the buffer already has stopped
                        // existing.
                        if (psd_isError(e)) {
                            return e;
                        }
                    }
                }
            } else if (n == '$') {
                i++;
            }
        }
        if (proceed) {
            psd_err e = dataAppend(&buffer, &length, &size, bufBuf, c);
            if (psd_isError(e)) {
                return e;
            }
        }
    }
    psd_err e = dataAppend(&buffer, &length, &size, 1, '\0');
    if (psd_isError(e)) {
        return e;
    }
    out->data = buffer;
    out->length = length;
    return noError();
}

psd_err filesystemPerform(psd_state *s, fsCallback_t callback) {
    psd_err error = noError();
    psd_hashmap_iterator it;
    psd_hashmap_iterator_init(&s->filesystem, &it);

    for (int code = PSD_HASHMAP_EOK; code == PSD_HASHMAP_EOK; code = psd_hashmap_iterator_next(&it)) {
        psd_hashmap_searchResult result = psd_hashmap_iterator_get(it);
        assert(result.length == sizeof(char*));
        char *value = NULL;
        char *key = NULL;
        psd_hashmap_get(&s->filesystem, result, &value, NULL);
        psd_hashmap_getKey(&s->filesystem, result, &key, NULL);
        psd_err e = callback(key, value);
        // NOTE: Continuing despite errors
        if (psd_isError(error)) {
            error = e;
        };
    }

    return error;
}


psd_err callback_filesystemClean(char *toPath, char *fromPath) {
    if (remove(fromPath)) {
        return mkError(PSD_LIB_EIO, "Remove file");
    }
    return noError();
}

psd_err callback_filesystemCommit(char *toPath, char *fromPath) {
    FILE *to = fopen(toPath, "w+");
    FILE *from = fopen(fromPath, "r");
    psd_err error = copyFile(from, to);
    int code0 = fclose(to);
    int code1 = fclose(to);
    if (!psd_isError(error) && (code0 == EOF || code1 == EOF)) {
        error = mkError(PSD_LIB_EIO, "Close file");
    }
    return error;
}

psd_err copyFile(FILE *src, FILE *dest) {
    psd_err error = noError();
    const size_t bufSize = FILE_COPY_BUFFER_SIZE;
    char *buffer = (char *) malloc(sizeof(char) * bufSize);
    fseek(dest, 0, SEEK_SET);
    fseek(src, 0, SEEK_SET);
    size_t read = 0;
    size_t wrote = 0;
    do {
        read = fread(buffer, sizeof(char), bufSize, src);
        if (ferror(src)) {
            error = mkError(PSD_LIB_EIO, "Read file");
        } else {
            wrote = fwrite(buffer, sizeof(char), bufSize, dest);
            if (wrote != read) {
                error = mkError(PSD_LIB_EIO, "Write file");
            }
        }
    } while (read == bufSize && error.code == PSD_LIB_EOK);

    free(buffer);
    return error;
}

void saveRegexError(int code, regex_t *handle, psd_errMsgBuf *errMsg) {
    int wrote = regerror(code, handle, errMsg->data, sizeof(char) * errMsg->size);
    errMsg->length = MIN(wrote, errMsg->size) - 1;
}

psd_err regexMatch(const char *string, const char *regex, regmatch_t **out, int *nMatches, psd_errMsgBuf *errMsg) {
    const int cflags = REG_EXTENDED;
    const int eflags = 0;
    psd_err error = noError();
    regex_t handle;

    int compRes = regcomp(&handle, regex, cflags);
    if (compRes) {
        saveRegexError(compRes, &handle, errMsg);
        error = mkError(PSD_LIB_ERCMP, "Compile regex");
    } else {
        size_t matchesSize = 0;
        regmatch_t *matches = NULL;
        if (out != NULL) {
            matchesSize = MAX_REGEX_MATCHES;
            matches = (regmatch_t *) malloc(sizeof(regmatch_t) * matchesSize);
        }
        int execRes = regexec(&handle, string, matchesSize, matches, eflags);
        if (execRes) {
            saveRegexError(execRes, &handle, errMsg);
            if (execRes == REG_NOMATCH) {
                error = mkError(PSD_LIB_ERMATCH, "Execute regex - no match");
            } else {
                error = mkError(PSD_LIB_EREXE, "Execute regex");
            }
            free(matches);
        } else {
            if (nMatches != NULL) {
                // man 3 regex:
                // "Any unused structure elements will contain the value -1."
                for ((*nMatches) = 0; matches[*nMatches].rm_so != -1 && *nMatches < MAX_REGEX_MATCHES; (*nMatches)++);
            }
            if (out != NULL) {
                *out = matches;
            }
        }
    }

    regfree(&handle);
    return error;
}


psd_err startsWith(const char *str, const char *sequence, bool *result) {
    for (size_t i = 0;; i++) {
        if (!sequence[i]) {
            *result = true;
            return noError();
        }
        if (!str[i] || str[i] != sequence[i]) {
            *result = false;
            return noError();
        }
        // Failsafe
        if (i > STRING_OVERRUN_MAX) {
            // Critical: invalid string
            // Possible corruption
            return mkError(PSD_LIB_ECORRUPTION, "Iterate string, no null terminator");
        }
    }
    *result = false;
    return noError();
}

psd_sqlParam mkSqlParam(double number, const char *string) {
    psd_sqlParam p;
    p.number = number;
    p.string = string;
    return p;
}

long fileSize(FILE *fp) {
    long original = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, original, SEEK_SET);
    return size;
}

psd_err urlGet(const char *url, buffer *out) {
    psd_err error = noError();
    CURL *handle = curl_easy_init();
    buffer buffer = mkBuffer();

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, callback_curl_writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);

    CURLcode ccode = curl_easy_perform(handle);

    if (ccode != CURLE_OK) {
        error = mkError(PSD_LIB_ENET, curl_easy_strerror(ccode));
    } else {
        size_t dummy_size = buffer.length;
        error = dataAppend(&buffer.data, &buffer.length, &dummy_size, 1, '\0');
        buffer.length--;
    }

    curl_easy_cleanup(handle);
    *out = buffer;
    return error;
}

psd_err fileGet(const char *path, buffer *out) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return mkError(PSD_LIB_EIO, "Open file for reading");
    }
    long size = fileSize(file) + 1;
    // We are not buffering
    if (size > MAX_FILE_SIZE) {
        return mkError(PSD_LIB_EBUFFER, "File buffer");
    }
    buffer b = mkBuffer();
    b.length = size;
    b.data = (char *) malloc(sizeof(char) * b.length);
    if (b.data == NULL) {
        return mkError(PSD_LIB_ENOMEM, "Allocate file buffer");
    }

    size_t writableSize = b.length - 1;
    size_t r = fread(b.data, sizeof(char), writableSize, file);
    if (r != b.length) {
        return mkError(PSD_LIB_EIO, "Read from file");
    }

    int status = fclose(file);
    if (status == EOF) {
        return mkError(PSD_LIB_EIO, "Close file");
    }

    b.data[b.length - 1] = '\0';
    *out = b;
    return noError();
}

psd_err resourceGet(const char *url, buffer *out) {
    bool isUrl = false;
    psd_err e = startsWith(url, "http", &isUrl);
    if (psd_isError(e)) {
        assert(false); // We are literally using a string literal how
        return e;
    }
    if (isUrl) {
        return urlGet(url, out);
    } else {
        return fileGet(url, out);
    }
}

size_t callback_curl_writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    buffer *buf = (buffer *) userdata;
    int newLength = buf->length + nmemb;
    if (newLength > MAX_FILE_SIZE) {
        // Error: buffer length exceeded
        return CURL_WRITEFUNC_ERROR;
    }
    char *data = (char *) realloc(buf->data, sizeof(char) * newLength);
    if (data == NULL) {
        freeBuffer(buf);
        return CURL_WRITEFUNC_ERROR;
    }
    buf->data = data;
    memcpy(buf->data + buf->length, ptr, nmemb);
    buf->length = newLength;
    return nmemb;
}

buffer mkBuffer() {
    buffer b;
    b.data = NULL;
    b.length = 0;
    return b;
}

void freeBuffer(buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
}

psd_err mkError(int code, const char *message) {
    psd_err e;
    e.code = code;
    e.message = message;
    return e;
}

psd_err noError() {
    return mkError(PSD_LIB_EOK, "");
}

psd_err getTmpName(char *name) {
    size_t len = strlen(name);
    char *mod = (char *) malloc(sizeof(char) * len);
    int number = 0;
    bool loop = true;
    do {
        memcpy(mod, name, sizeof(char) * (len + 1));
        for (int i = 0, x = 1; i < len; i++) {
            char c = name[i];
            if (c == 'X') {
                c += number / x;
                x *= 10;
            }
        }
        FILE *file = fopen(mod, "r");
        if (file != NULL) {
            fclose(file);
            loop = false;
        }
        number++;
        if (number > STRING_OVERRUN_MAX) {
            free(mod);
        }
    } while (loop);
    memcpy(name, mod, sizeof(char) * (len + 1));
    clean
    free(mod);
    return noError();
}

// --- Error handling ---

bool psd_isError(psd_err error) {
    return error.code != PSD_LIB_EOK;
}

// --- Library functions ---

psd_err psd_fn_scrape_regex(psd_state *s, const char *url, const char *regex, const psd_fn_params params, psd_fn_out *out) {
    psd_err error = noError();
    regmatch_t *searchResults = NULL;
    int nMatches = 0;
    buffer formatted = mkBuffer();
    buffer data = mkBuffer();

    error = formatString(url, params, &formatted);
    if (psd_isError(error)) {
        goto cleanup;
    }

    error = resourceGet(formatted.data, &data);
    if (psd_isError(error)) {
        goto cleanup;
    }

    error = regexMatch(data.data, regex, &searchResults, &nMatches, &s->errMsg);
    if (psd_isError(error)) {
        goto cleanup;
    }

    out->length = nMatches;
    out->list = (psd_fn_result *) malloc(sizeof(psd_fn_result) * out->length);

    for (int i = 0; i < nMatches; i++) {
        regmatch_t o = searchResults[i];
        char *match = data.data + o.rm_so;
        size_t mLength = o.rm_eo - o.rm_so;
        size_t dSize = mLength + 1;
        psd_fn_result res;
        res.length = mLength;
        res.data = (char *) malloc(sizeof(char) * dSize);
        memcpy(res.data, match, sizeof(char) * mLength);
        res.data[res.length] = '\0';
        out->list[i] = res;
    }

cleanup:

    free(searchResults);
    freeBuffer(&formatted);
    freeBuffer(&data);

    return error;
}

psd_err psd_fn_scrape_xpath(psd_state *s, const char *url, const char *xpath, const psd_fn_params params, psd_fn_out *out) {
    assert(false);
    return noError();
}

psd_sqlparam psd_mkParam_double(double val) {
    return mkSqlParam(val, NULL);
}

psd_sqlParam psd_mkParam_string(const char *val) {
    return mkSqlParam(0, val);
}

psd_err psd_fn_sql(psd_state *s, const char *sql, const psd_sqlParam *params, psd_fn_out *out) {
    psd_err error = noError();
    return error;
}

psd_err psd_fn_validate(psd_state *s, const char *value, const char *regex, bool *validation) {
    psd_err error = regexMatch(value, regex, NULL, NULL, &s->errMsg);
    if (error.code == PSD_LIB_ERMATCH) {
        *validation = false;
        error = noError();
    } else if (!psd_isError(error)) {
        *validation = true;
    }
    return error;
}

// --- Control ---

void psd_reset(psd_state *s) {
    filesystemPerform(s, callback_filesystemClean);
    psd_hashmap_clear(&s->filesystem);
    return err;
}

psd_err psd_commit(psd_state *s) {
    return filesystemPerform(s, callback_filesystemCommit);
}

// --- Initialization ---

psd_err psd_init(psd_state *s) {
    int res = psd_hashmap_init(&s->filesystem, BUCKET_PRECISION);
    if (res == PSD_HASHMAP_ENOMEM) {
        return mkError(PSD_LIB_ENOMEM, "Init hashmap");
    }
    s->errMsg.size = 512;
    s->errMsg.length = 0;
    s->errMsg.data = (char *) malloc(sizeof(char) * s->errMsg.size);
    if (s->errMsg.data == NULL) {
        psd_hashmap_free(&s->filesystem);
        return mkError(PSD_LIB_ENOMEM, "Init error buffer");
    }
    s->errMsg.data[0] = '\0'; // Null terminator

    return noError();
}

psd_err psd_globalInit() {
    curl_version_info_data *ci = curl_version_info(CURLVERSION_NOW);
    if ((ci->features & CURL_VERSION_SSL) != CURL_VERSION_SSL) {
        // Error: we require SSL support
        // etc. etc.
    }
    // TODO: Check CURL version stuff

    curl_global_init(CURL_GLOBAL_ALL);

    return noError();
}

void psd_globalCleanup() {
    curl_global_cleanup();
}

// --- Memory ---

void psd_free(psd_state *s) {
    psd_reset(s);
    psd_hashmap_free(&s->filesystem);
    free(s->errMsg.data);
    s->errMsg.data = NULL;
    s->errMsg.size = 0;
    s->errMsg.length = 0;
}

void psd_free_out(psd_fn_out *out) {
    for (size_t i = 0; i < out->length; i++) {
        free(out->list[i].data);
    }
    free(out->list);
    out->list = NULL;
    out->length = 0;
}

// TEMP
// FOR TESTING ONLY
int main(int argc, char **argv) {

    psd_globalInit();

    psd_state state;
    psd_init(&state);


    psd_fn_params params;
    params.values = NULL;
    params.length = 0;
    psd_fn_out out;
    out.list = NULL;
    out.length = 0;
    psd_err e = psd_fn_scrape_regex(&state, "https://debuginfod.archlinux.org/", "https://([^/]+?)", params, &out);

    printf("results: %li\n", out.length);
    printf("error: %s\n", e.message);
    printf("detail: %s\n", state.errMsg.data);
    if (psd_isError(e)) {
        printf("ERROR STATUS\n");
    } else {
        printf("No error\n");
    }

    for (size_t i = 0; i < out.length; i++) {
        printf("Match %li: '%s'\n", i, out.list[i].data);
    }

    bool validation = false;
    psd_err verr = psd_fn_validate(&state, "42342.232", "^[[:digit:]]+\\.[[:digit:]]+$", &validation);
    if (psd_isError(verr)) {
        printf("Validation error'd out\n");
    }
    printf("Validation: %s\n", validation ? "true" : " false");

    psd_free_out(&out);
    psd_free(&state);
    psd_globalCleanup();

    return 0;
}
