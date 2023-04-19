#ifndef PSD_HASHMAP_H_INCLUDED
#define PSD_HASHMAP_H_INCLUDED

#include <stddef.h>

// --- Status codes ---

// No error
#define PSD_HASHMAP_EOK 0
// Out of memory
#define PSD_HASHMAP_ENOMEM 1
// No result
#define PSD_HASHMAP_ENORESULT 2
// End of results
#define PSD_HASHMAP_EEND 3

// --- Structures ---

typedef struct {
    char *key;
    size_t keyLen;

    void *value;
    // In bytes
    size_t valueLen;
} psd_hashmap_data;

typedef struct {
    psd_hashmap_data *data;
    size_t size;
    size_t length;
} psd_hashmap_bucket;

typedef struct {
    psd_hashmap_bucket *buckets;
    size_t nBuckets;
    int precision;
    int mask;
    const char *error;
    size_t nElements;
} psd_hashmap;

typedef struct {
    size_t bucket;
    size_t data;

    size_t length;
    size_t keyLength;
    int code;
} psd_hashmap_searchResult;

typedef struct {
    size_t bucket;
    size_t data;
    const psd_hashmap *m;
} psd_hashmap_iterator;

// --- Access & modification ---

size_t psd_hashmap_size(const psd_hashmap *m);
// Key must be null terminated. Not the same for value.
int psd_hashmap_insert(psd_hashmap *m, const char *key, const void *value, size_t valueLen);
// Searches are valid so long as the underlying container has not been modified
psd_hashmap_searchResult psd_hashmap_search(psd_hashmap *m, const char *key);
// These two getters pretty much just return the pointer to the data.
// They do NOT copy anything.
// `count` is the length of the pointed data, and may be null.
void psd_hashmap_get(const psd_hashmap *m, psd_hashmap_searchResult search, void **data, size_t *count);
void psd_hashmap_getKey(const psd_hashmap *m, psd_hashmap_searchResult search, char **data, size_t *count);
void psd_hashmap_remove(psd_hashmap *m, psd_hashmap_searchResult search);
void psd_hashmap_clear(psd_hashmap *m);

// --- Iteration ---

// No effect on iterator if there is an error.
// The behavior of the iterator is undefined in the event that the
// size of the hashmap is zero.

void psd_hashmap_iterator_init(const psd_hashmap *m, psd_hashmap_iterator *i);
void psd_hashmap_iterator_start(psd_hashmap_iterator *i);
// Returns errors: PSD_HASHMAP_EEND
int psd_hashmap_iterator_next(psd_hashmap_iterator *i);
// Returns errors: PSD_HASHMAP_EEND
int psd_hashmap_iterator_prev(psd_hashmap_iterator *i);
psd_hashmap_searchResult psd_hashmap_iterator_get(psd_hashmap_iterator i);

// --- Memory and initailization ---

// Returns errors: PSD_HASHMAP_ENOMEM
int psd_hashmap_reclaim(psd_hashmap *m);
void psd_hashmap_free(psd_hashmap *m);
// Returns errors: PSD_HASHMAP_ENOMEM
int psd_hashmap_init(psd_hashmap *m, int precision);

#endif
