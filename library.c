// Local
#include "library.h"

// Base
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h> // Non-standard lol get out of here w*ndows

// External
#include <sqlite3.h>
#include <curl/curl.h>
#include <openssl/md5.h>

// 0.25 GB
#define MAX_FILE_SIZE 0xFFFFFFF

// This is actually apparently what it is on linux, though I care at least
// somewhat about cross compatability so I won't bother with the header.
// linux/limits.h btw
#define PATH_MAX_LEN 4096

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

static psd_err filesystemPerform(psd_state *s, fsCallback_t callback);
static psd_err callback_filesystemReset(char *key, FILE *value);
static psd_err callback_filesystemCommit(char *key, FILE *value);
// Returns error: PSD_LIB_EOK, PSD_LIB_EIO
static psd_err copyFile(FILE *src, FILE *dest);
static psd_err addNullTerminator(buffer *buf);
static psd_err regexMatch(const char *string, const char *regex, buffer *out);
static psd_err startsWith(const char *str, const char *sequence, bool *result);
static long fileSize(FILE *fp);
static psd_err urlGet(const char *url, buffer *out);
static psd_err fileGet(const char *path, buffer *out);
bool resourceGet(const char *url, buffer *out);
static size_t callback_curl_writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
static void freeBuffer(buffer *buf);
static void setError(psd_state *s, const char *message, int code);
static bool checkError(psd_state *s);
static psd_err mkError(const char *message, int code);
static psd_err noError();
static bool isError(psd_err error);

// --- Static helper functions ---

buffer mkBuffer() {
    buffer b;
    b.data = NULL;
    b.length = 0;
    return b;
}

psd_err filesystemPerform(psd_state *s, fsCallback_t callback) {
    psd_err error = PSD_LIB_EOK;
    psd_hashmap_iterator it;
    psd_hashmap_iterator_init(&s->filesystem, &it);

    for (int code = PSD_HASHMAP_EOK; code == PSD_HASHMAP_EOK; code = psd_hashmap_iterator_next(&it)) {
        psd_hashmap_searchResult result = psd_hashmap_iterator_get(it);
        assert(result.length == sizeof(FILE*));
        FILE *value = NULL;
        psd_hashmap_get(s->filesystem, result, &value, sizeof(FILE*));
        char *key = (char *) malloc(sizeof(char) * result.keyLength);
        psd_hashmap_getKey(s->filesystem, result, key, sizeof(char) * result.keyLength);
        error = callback(key, value);
        free(key);
        if (isError(error)) {
            break;
        }
    }

    return error;
}

psd_err callback_filesystemReset(char *, FILE *handle) {
    int code = fclose(handle);
    if (code == EOF) {
        return mkError(PSD_LIB_EIO, "Close file");
    }
    return noError();
}

psd_err callback_filesystemCommit(char *path, FILE *from) {
    FILE *to = fopen(path, "w+");
    err error = fileCopy(from, to);
    int code = fclose(file);
    free(path);
    if (isError(error)) {
        break;
    }
    if (code == EOF) {
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
        read = fread(buffer, bufSize, src);
        if (ferror(src)) {
            error = mkError(PSD_LIB_EIO, "Read file");
        } else {
            wrote = fwrite(buffer, bufSize, dest);
            if (wrote != read) {
                error = mkError(PSD_LIB_EIO, "Write file");
            }
        }
    } while (read == bufSize && error.code == PSD_LIB_EOK);

    free(buffer);
    return error;
}

psd_err addNullTerminator(buffer *buf) {
    int newLength = buf->length + 1;
    buf->data = (char *) realloc(buf->data, sizeof(char) * newLength);
    if (buf->data == NULL) {
        return mkError(PSD_LIB_ENOMEM, "Allocate null terminator");
    }
    buf->data[buf->length] = '\0';
    buf->length = newLength;
    return return PSD_LIB_EOK;
}

psd_err regexMatch(const char *string, const char *regex, buffer *out) {
    /*
    buffer result = mkBuffer();
    regex_t handle;
    int reti;
    char msgbuf[100];

    if (regcomp(&handle, regex, 0)) {
        // Error: invalid regex?
        return false;
    }

    reti = regexec(&regex, "abc", 0, NULL, 0);
    if (regexec(&handle, string, 0, NULL, 0) == REG_) {
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

    regfree(&regex);
    */
}


int startsWith(const char *str, const char *sequence, bool *result) {
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
            return mkError(PSD_LIB_CORRUPTION, "Iterate string, no null terminator");
        }
    }
    *result = false;
    return PSD_LIB_EOK;
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
        error = addNullTerminator(&buffer);
    }

    curl_easy_cleanup(handle);
    *out = buffer;
    return success;
}

int fileGet(const char *path, buffer *out) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return mkError(PSD_LIB_EIO, "Open file for reading");
    }
    long size = fileSize(file);
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

    // Automatically writes terminating null
    size_t r = fread(b.data, sizeof(char), b.length, file);
    if (r != b.length) {
        return mkError(PSD_LIB_EIO, "Read from file");
    }

    int status = fclose(file);
    if (status == EOF) {
        return mkError(PSD_LIB_EIO, "Close file");
    }

    *out = b;
    return true;
}

int resourceGet(const char *url, buffer *out) {
    bool isUrl = false;
    int e = startsWith(url, "http", &isUrl);
    if (e != PSD_LIB_EOK) {
        assert(false); // We are literally using a string literal how
        return e;
    }
    if (isUrl) {
        return urlGet(url, out);
    } else {
        return fileGet(url, out);
    }
}

size_t curl_writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    buffer *buf = (buffer *) userdata;
    int newLength = buf->length + nmemb;
    if (newLength > MAX_FILE_SIZE) {
        // Error: buffer length exceeded
        return CURL_WRITEFUNC_ERROR;
    }
    buf->data = (char *) realloc(buf->data, sizeof(char) * newLength);
    memcpy(buf->data + buf->length, ptr, nmemb);
    buf->length = newLength;
    return nmemb;
}

void freeBuffer(buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
}

void setError(psd_state *s, const char *message, int code) {
    s->errCode = code;
    s->errMsg = message;
}

bool checkError(psd_state *s) {
    return s->errorCode != PSD_LIB_EOK;
}

psd_err mkError(const char *message, int code) {
    psd_err e;
    e.message = message;
    e.code = code;
    return e;
}

psd_err noError() {
    psd_err e;
    e.message = "";
    e.code = PSD_LIB_EOK;
    return e;
}

bool isError(psd_err error) {
    return error.code != PSD_LIB_EOK;
}

// --- Library functions ---

void psd_fn_scrape_regex(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *regex, psd_fn_out *out) {
    buffer data = mkBuffer();
    if (!getData(url, &data)) {
        // TERMINATION - ERROR
    }

}

void psd_fn_scrape_xpath(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *xpath, psd_fn_out *out) {

}

void psd_fn_sql(psd_state *s, bool effect, const char *sql, const psd_fn_params, psd_fn_out *out) {

}

void psd_fn_validate(psd_state *s, bool effect, const char *value, const char *regex) {

}


// --- Control ---

psd_err psd_reset(psd_state *s) {
    return filesystemPerform(s, callback_filesystemReset);
    /*
    // TODO: Reduce code by using bitfields and batching???
    psd_hashmap_iterator it;
    psd_hashmap_iterator_init(&s->filesystem, &it);

    for (int code = PSD_HASHMAP_EOK; code == PSD_HASHMAP_EOK; code = psd_hashmap_iterator_next(&it)) {
        psd_hashmap_searchResult result = psd_hashmap_iterator_get(it);
        assert(result.length == sizeof(FILE*));
        FILE *handle = NULL;
        psd_hashmap_get(s->filesystem, result, &handle, sizeof(FILE*));
        int r = fclose(handle);

    }

    psd_hashmap_clear(s->filesystem);
    */
}

psd_err psd_commit(psd_state *s) {
    return filesystemPerform(s, callback_filesystemCommit);
    /*
    psd_err error = PSD_LIB_EOK;
    psd_hashmap_iterator it;
    psd_hashmap_iterator_init(&s->filesystem, &it);

    for (int code = PSD_HASHMAP_EOK; code == PSD_HASHMAP_EOK; code = psd_hashmap_iterator_next(&it)) {
        psd_hashmap_searchResult result = psd_hashmap_iterator_get(it);
        assert(result.length == sizeof(FILE*));
        FILE *from = NULL;
        psd_hashmap_get(s->filesystem, result, &from, sizeof(FILE*));
        char *path = (char *) malloc(sizeof(char) * result.keyLength);
        FILE *to = fopen(path, "w+");
        error = fileCopy(from, to);
        int closeCode = fclose(file);
        free(path);
        if (isError(error)) {
            break;
        }
        if (closeCode == EOF) {
            error = mkError(PSD_LIB_EIO, "Closing file");
            break;
        }
    }

    return error;
    */
}

// --- Initialization ---

bool psd_globalInit() {
    curl_version_info_data *ci = curl_version_info(CURLVERSION_NOW);
    if (ci->features & CURL_VERSION_SSL != CURL_VERSION_SSL) {
        // Error: we require SSL support
    }
    // TODO: Check CURL version stuff

    curl_global_init(CURL_GLOBAL_ALL);
}

void psd_globalCleanup() {
    curl_global_cleanup();
}


void psd_init(psd_state *s) {
    psd_hashmap_init(s->filesystem, BUCKET_PRECISION);
}

// --- Memory ---

void psd_free(psd_state *s) {
    psd_hashmap_free(s->filesystem);
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

    return 0;
}
