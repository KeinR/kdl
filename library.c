// Local
#include "library.h"

// Base
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h> // Non-standard lol get out of here w*ndows

// External
#include <sqlite3.h>
#include <curl/curl.h>

// 0.25 GB
#define MAX_FILE_SIZE 0xFFFFFFF

// How long we are okay with iterating over a null-terminated string
#define STRING_OVERRUN_MAX 0xFFFF

typedef struct {
    char *data;
    psd_size_t length;
} buffer;

buffer mkBuffer() {
    buffer b;
    b.data = NULL;
    b.length = 0;
    return b;
}

/*
bool strEqN(const char *a, psd_size_t lenA, const char *b, psd_length_t lenB) {
    if (lenA != lenB) {
        return false;
    }
    for (psd_size_t i = 0; i < lenA; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return false;
}

bool strEq(const char *a, const char *b) {
    psd_size_t i = 0;
    for (; a[i] && b[i]; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return a[i] == b[i];
}
*/

bool startsWith(const char *str, const char *sequence, bool *result) {
    for (psd_size_t i = 0;; i++) {
        if (!sequence[i]) {
            *result = true;
            return true;
        }
        if (!str[i] || str[i] != sequence[i]) {
            *result = false;
            return true;
        }
        // Failsafe
        if (i > STRING_OVERRUN_MAX) {
            // Critical: invalid string
            // Possible corruption
            return false;
        }
    }
    *result = false;
    return true;
}

void freeBuffer(buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
}

long fileSize(FILE *fp) {
    long original = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, original, SEEK_SET);
    return size;
}

bool addNullTerminator(buffer *buf) {
    int newLength = buf->length + 1;
    buf->data = (char *) realloc(buf->data, sizeof(char) * newLength);
    if (buf->data == NULL) {
        // Error: out of memory
        return false;
    }
    buf->data[buf->length] = '\0';
    buf->length = newLength;
    return true;
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

bool urlGet(const char *url, buffer *out) {
    bool success = true;
    CURL *handle = curl_easy_init();
    buffer buffer = mkBuffer();

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);

    CURLcode code = curl_easy_perform(handle);

    if (!addNullTerminator(&buffer)) {
        success = false;
    }

    curl_easy_cleanup(handle);
    *out = buffer;
    return success;
}

bool fileGet(const char *path, buffer *out) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        // Error: could not open file for reading
        return false;
    }
    long size = fileSize(file);
    // We are not buffering
    if (size > MAX_FILE_SIZE) {
        // Error: file size is too large
        return false;
    }
    buffer b = mkBuffer();
    b.length = size;
    b.data = (char *) malloc(sizeof(char) * b.length);
    if (b.data == NULL) {
        // Error: out of memory
        return false;
    }

    // Automatically writes terminating null
    size_t r = fread(b.data, sizeof(char), b.length, file);
    if (r != b.length) {
        // Error: failed to read from file, or estimate was incorrect
        return false;
    }

    int status = fclose(file);
    if (status == EOF) {
        // Error: failed closing file
        return false;
    }

    *out = b;
    return true;
}

bool resourceGet(const char *url, buffer *out) {
    bool isUrl = false;
    if (!startsWith(url, "http", &isUrl)) {
        // Corrupt string
        return false;
    }
    if (isUrl) {
        return urlGet(url, out);
    } else {
        return fileGet(url, out);
    }
}



void psd_fn_scrape_regex(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *regex, psd_fn_out *out) {

}

void psd_fn_scrape_xpath(psd_state *s, bool effect, const char *url, const psd_fn_params params, const char *xpath, psd_fn_out *out) {

}
void psd_fn_validate(psd_state *s, bool effect, const char *value, const char *regex) {

}
void psd_fn_sql(psd_state *s, bool effect, const char *sql, const psd_fn_params, psd_fn_out *out) {

}

void psd_free_out(psd_fn_out *out) {
    for (psd_size_t i = 0; i < out->length; i++) {
        free(out->list[i].data);
    }
    free(out->list);
    out->list = NULL;
    out->length = 0;
}

bool psd_init() {
    curl_version_info_data *ci = curl_version_info(CURLVERSION_NOW);
    if (ci->features & CURL_VERSION_SSL != CURL_VERSION_SSL) {
        // Error: we require SSL support
    }
    // TODO: Check CURL version stuff

    curl_global_init(CURL_GLOBAL_ALL);
}

void psd_uninit() {
    curl_global_cleanup();
}

void psd_start(psd_callback_t callback, void *userData) {
    psd_state state;
    if (setjmp(state.jumpBuf)) {
        // Error exit
    } else {
        callback(&state, userData);
        // Noerror exit

    }
}


// TEMP
// FOR TESTING ONLY
int main(int argc, char **argv) {

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

    return 0;
}
