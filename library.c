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
static psd_err callback_filesystemReset(char *key, FILE *value);
static psd_err callback_filesystemCommit(char *key, FILE *value);
// Returns error: PSD_LIB_EOK, PSD_LIB_EIO
static psd_err copyFile(FILE *src, FILE *dest);
static void saveRegexError(int code, regex_t *handle, psd_errMsgBuf *errMsg);
static psd_err regexMatch(const char *string, const char *regex, regmatch_t **out, int *nMatches, psd_errMsgBuf *errMsg);
static psd_err startsWith(const char *str, const char *sequence, bool *result);
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
        assert(result.length == sizeof(FILE*));
        FILE *value = NULL;
        psd_hashmap_get(&s->filesystem, result, &value, sizeof(FILE*));
        char *key = (char *) malloc(sizeof(char) * result.keyLength);
        psd_hashmap_getKey(&s->filesystem, result, key, sizeof(char) * result.keyLength);
        psd_err e = callback(key, value);
        free(key);
        // WARNING: Continuing despite errors
        if (psd_isError(error)) {
            error = e;
        };
    }

    return error;
}

psd_err callback_filesystemReset(char *, FILE *handle) {
    // We use tmpfile - the file is deleted automatically
    // when the file is closed.
    int code = fclose(handle);
    if (code == EOF) {
        return mkError(PSD_LIB_EIO, "Close file");
    }
    return noError();
}

psd_err callback_filesystemCommit(char *path, FILE *from) {
    FILE *to = fopen(path, "w+");
    psd_err error = copyFile(from, to);
    int code = fclose(to);
    free(path);
    if (!psd_isError(error) && code == EOF) {
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
        // TODO: Get the number of matches and store it in *nMatches
        size_t matchesSize = MAX_REGEX_MATCHES;
        regmatch_t *matches = (regmatch_t *) malloc(sizeof(regmatch_t) * matchesSize);
        int execRes = regexec(&handle, string, matchesSize, matches, eflags);
        if (execRes) {
            saveRegexError(execRes, &handle, errMsg);
            error = mkError(PSD_LIB_EREXE, "Execute regex");
            free(matches);
        } else {
            // man 3 regex:
            // "Any unused structure elements will contain the value -1."
            for ((*nMatches) = 0; matches[*nMatches].rm_so != -1 && *nMatches < MAX_REGEX_MATCHES; (*nMatches)++);
            *out = matches;
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

// --- Error handling ---

bool psd_isError(psd_err error) {
    return error.code != PSD_LIB_EOK;
}

// --- Library functions ---

psd_err psd_fn_scrape_regex(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *regex, psd_fn_out *out) {
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

psd_err psd_fn_scrape_xpath(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *xpath, psd_fn_out *out) {
    assert(false);
    return noError();
}

psd_err psd_fn_sql(psd_state *s, bool effect, const char *sql, const psd_fn_params, psd_fn_out *out) {
    assert(false);
    return noError();
}

psd_err psd_fn_validate(psd_state *s, bool effect, const char *value, const char *regex) {
    assert(false);
    return noError();
}

// --- Control ---

psd_err psd_reset(psd_state *s) {
    psd_err err = filesystemPerform(s, callback_filesystemReset);
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
    s->errMsg.data[0] = '\0'; // Null terminator
    if (s->errMsg.data == NULL) {
        // I at least care a little bit about memleaks
        psd_hashmap_free(&s->filesystem);
        return mkError(PSD_LIB_ENOMEM, "Init error buffer");
    }
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
    psd_err e = psd_fn_scrape_regex(&state, false, "https://debuginfod.archlinux.org/", params, "^.*([a-z]{1,3}).*$", &out);

    printf("results: %li\n", out.length);
    printf("error: %s\n", e.message);
    printf("detail: %s\n", state.errMsg.data);
    if (psd_isError(e)) {
        printf("ERROR STATUS\n");
    } else {
        printf("No error\n");
    }



    regmatch_t *matches = NULL;
    int count = -1;
    psd_err error = regexMatch("foobar test", "(t[a-z])", &matches, &count, &state.errMsg);
    printf("results: %li\n", out.length);
    printf("error: %s\n", error.message);
    printf("detail: %s\n", state.errMsg.data);


    psd_free_out(&out);
    psd_free(&state);
    psd_globalCleanup();



    /*
    psd_init();

    psd_hashmap map;
    psd_hashmap_init(&map, 3);
    psd_hashmap_free(&map);

    buffer data;
    // Should be null terminated
    bool success = resourceGet("testFile.txt", &data);
    if (success) {
        printf("Got data:\n\"%s\"\n", data.data);
    } else {
        printf("Error getting resource (what happened!?)\n");
    }

    freeBuffer(&data);

    // Should be null terminated
    success = resourceGet("https://debuginfod.archlinux.org/", &data);
    if (success) {
        printf("Got data:\n\"%s\"\n", data.data);
    } else {
        printf("Error getting web resource (what happened!?)\n");
    }

    psd_cleanup();
    */


    regex_t regex;
    int reti;
    char msgbuf[100];

    /* Compile regular expression */
    reti = regcomp(&regex, "\\(t[a-z]+)", REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    /* Execute regular expression */
    reti = regexec(&regex, "foobar test", 0, NULL, 0);
    if (!reti) {
        puts("Match");
    }
    else if (reti == REG_NOMATCH) {
        puts("No match");
    }
    else {
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        exit(1);
    }

    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&regex);


    return 0;
}
