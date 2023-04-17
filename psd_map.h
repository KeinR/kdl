#ifndef PSD_HASHMAP_H_INCLUDED
#define PSD_HASHMAP_H_INCLUDED

// --- Error codes ---

#define PSD_HASHMAP_EOK 0
#define PSD_HASHMAP_ENOMEM 1
#define PSD_HASHMAP_ENORESULT 2

// --- Structures ---

typedef struct {
    char *key;
    size_t keyLen;
    char *value;
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
    uint8_t precision;
    int mask;
    const char *error;
} psd_hashmap;

typedef struct {
    size_t bucket;
    size_t data;

    size_t length;
    int code;
} psd_hashmap_searchResult;

// --- Access & modification ---

// Key must be null terminated. Not the same for value.
int psd_hashmap_insert(psd_hashmap *m, const char *key, const char *value, size_t valueLen);
// Searches are valid so long as the underlying container has not been modified
psd_hashmap_searchResult psd_hashmap_search(psd_hashmap *m, const char *key);
size_t psd_hashmap_get(const psd_hashmap *m, psd_hashmap_searchResult search, char *data, size_t count);
void psd_hashmap_remove(psd_hashmap *m, psd_hashmap_searchResult search);

// --- Memory and initailization ---

void psd_hashmap_reclaim(psd_hashmap *m);
void psd_hashmap_free(psd_hashmap *m);
int psd_hashmap_init(psd_hashmap *m);

#endif
