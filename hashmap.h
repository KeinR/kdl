#ifndef KDL_HASHMAP_H_INCLUDED
#define KDL_HASHMAP_H_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

#include "def.h"

// --- Status codes ---

// No error
#define KDL_HASHMAP_EOK 0
// No result
#define KDL_HASHMAP_ENORESULT 1
// End of results
#define KDL_HASHMAP_EEND 2

// --- Structures ---

typedef void(*kdl_hashmap_dataFree_t)(kdl_state_t,void*);

typedef struct {
    uint8_t digest[16];
} kdl_hashmap_hash_t;

typedef struct {
    void *data;
    char *key;
    size_t keyLen;
} kdl_hashmap_data_t;

typedef struct {
    kdl_hashmap_data_t *data;
    size_t size;
    size_t length;
} kdl_hashmap_bucket_t;

typedef struct {
    kdl_state_t s;
    kdl_hashmap_dataFree_t freeFunc;
    kdl_hashmap_bucket_t *buckets;
    size_t nBuckets;
    size_t nElements;
    int mask;
} kdl_hashmap_t;


typedef struct {
    size_t bucket;
    size_t data;
    size_t keyLength;
    int code;
} kdl_hashmap_result_t;

// --- Access & modification ---

size_t kdl_hashmap_size(const kdl_hashmap_t *m);
// Key must be null terminated. Not the same for value.
void kdl_hashmap_insert(kdl_hashmap_t *m, const char *key, void *value);
// Searches are valid so long as the underlying container has not been modified
int kdl_hashmap_search(kdl_hashmap_t *m, const char *key, kdl_hashmap_result_t *result);
// These two getters pretty much just return the pointer to the data.
// They do NOT copy anything.
// `count` is the length of the pointed data, and may be null.
void kdl_hashmap_get(const kdl_hashmap_t *m, kdl_hashmap_result_t search, void **data);
void kdl_hashmap_remove(kdl_hashmap_t *m, kdl_hashmap_result_t search);
void kdl_hashmap_clear(kdl_hashmap_t *m);

// --- Iteration ---

kdl_hashmap_result_t kdl_hashmap_first(kdl_hashmap_t *m);
// Returns errors: PSD_HASHMAP_EEND
int kdl_hashmap_next(kdl_hashmap_t *m, kdl_hashmap_result_t *r);

// --- Memory and initailization ---

void kdl_hashmap_reclaim(kdl_hashmap_t *m);
void kdl_hashmap_free(kdl_hashmap_t *m);
void kdl_hashmap_init(kdl_state_t s, kdl_hashmap_t*m, int precision, kdl_hashmap_dataFree_t freeFunc);

#endif
