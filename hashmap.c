#include "hashmap.h"

#include <sys/types.h>
#include <md5.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define BUCKET_SIZE_STEP 4

// --- Static helper methods ---

static kdl_hashmap_hash_t stringMD5(const char *str);
static int stringLength(const char *str);
static int getIndex(const kdl_hashmap_t *m, const char *key);
static int findIndex(kdl_hashmap_t *m, const char *key, size_t *bucket, size_t *data);

kdl_hashmap_hash_t stringMD5(const char *str) {
    int length = strlen(str);
    MD5_CTX c;
    kdl_hashmap_hash_t hash;

    MD5Init(&c);
    MD5Update(&c, (const unsigned char *)str, length);
    MD5Final(hash.digest, &c);

    return hash;
}

int stringLength(const char *str) {
    return strlen(str) + 1;
}

bool stringsEqual(const char *a, size_t lenA, const char *b, size_t lenB) {
    if (lenA != lenB) {
        return false;
    }
    for (size_t i = 0; i < lenA; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

int getIndex(const kdl_hashmap_t *m, const char *key) {
    return *((unsigned long long *)stringMD5(key).digest) & m->mask;
}

int findIndex(kdl_hashmap_t *m, const char *key, size_t *bucket, size_t *data) {
    const int iKey = getIndex(m, key);
    const int kLen = stringLength(key);
    kdl_hashmap_bucket_t *b = m->buckets + iKey;
    for (size_t i = 0; i < b->length; i++) {
        kdl_hashmap_data_t *d = b->data + i;
        if (stringsEqual(key, kLen, d->key, d->keyLen)) {
            *bucket = iKey;
            *data = i;
            return KDL_HASHMAP_EOK;
        }
    }
    return KDL_HASHMAP_ENORESULT;
}

// --- Exported methods ---

// --- Access & modification ---

size_t kdl_hashmap_size(const kdl_hashmap_t *m) {
    return m->nElements;
}

void kdl_hashmap_insert(kdl_hashmap_t *m, const char *key, void *data) {
    const int iKey = getIndex(m, key);
    kdl_hashmap_bucket_t *b = m->buckets + iKey;
    const size_t newLength = b->length + 1;
    if (newLength > b->size) {
        b->size = b->size + BUCKET_SIZE_STEP;
        b->data = (kdl_hashmap_data_t *) m->s.realloc(b->data, sizeof(kdl_hashmap_data_t) * b->size);
    }
    kdl_hashmap_data_t e;

    e.keyLen = stringLength(key);
    e.key = (void *) m->s.malloc(sizeof(char) * e.keyLen);
    memcpy(e.key, key, e.keyLen);

    e.data = data;

    b->data[b->length++] = e;

    m->nElements++;
}


int kdl_hashmap_search(kdl_hashmap_t *m, const char *key, kdl_hashmap_result_t *result) {
    size_t bucket = 0;
    size_t data = 0;
    int status = findIndex(m, key, &bucket, &data);
    memset(result, 0, sizeof(kdl_hashmap_result_t));
    result->bucket = bucket;
    result->data = data;
    result->code = status;
    if (status == KDL_HASHMAP_EOK) {
        result->keyLength = m->buckets[bucket].data[data].keyLen;
    }
    return result->code;
}

void kdl_hashmap_get(const kdl_hashmap_t *m, kdl_hashmap_result_t search, void **data) {
    assert(search.bucket < m->nBuckets && search.data < m->buckets[search.bucket].length);

    kdl_hashmap_data_t d = m->buckets[search.bucket].data[search.data];
    *data = d.data;
}

void kdl_hashmap_remove(kdl_hashmap_t *m, kdl_hashmap_result_t search) {
    kdl_hashmap_bucket_t *b = m->buckets + search.bucket;
    assert(b->length > 0);
    m->s.free(b->data[search.data].key);
    m->freeFunc(m->s, b->data[search.data].data);
    b->data[search.data] = b->data[b->length - 1];
    b->length--;
    m->nElements--;
}

void kdl_hashmap_clear(kdl_hashmap_t *m) {
    for (size_t b = 0; b < m->nBuckets; b++) {
        for (size_t d = 0; d < m->buckets[b].length; d++) {
            m->s.free(m->buckets[b].data[d].key);
            m->freeFunc(m->s, m->buckets[b].data[d].data);
        }
        // Do not reclaim memory - that can be done with reclaim()
        m->buckets[b].length = 0;
    }
    m->nElements = 0;
}

// --- Iteration ---

kdl_hashmap_result_t kdl_hashmap_first(kdl_hashmap_t *m) {
    kdl_hashmap_result_t s;
    memset(&s, 0, sizeof(kdl_hashmap_result_t));
    s.code = KDL_HASHMAP_ENORESULT;
    kdl_hashmap_next(m, &s);
    return s;
}

int kdl_hashmap_next(kdl_hashmap_t *m, kdl_hashmap_result_t *i) {
    i->data++;
    i->code = KDL_HASHMAP_EOK;
    if (i->data >= m->buckets[i->bucket].length) {
        i->data = 0;
        do {
            i->bucket++;
            if (i->bucket >= m->nBuckets) {
                i->code = KDL_HASHMAP_EEND;
                break;
            }
        } while (m->buckets[i->bucket].length == 0);
    }
    return i->code;
}

// --- Memory and initailization ---

void kdl_hashmap_reclaim(kdl_hashmap_t *m) {
    for (size_t b = 0; b < m->nBuckets; b++) {
        kdl_hashmap_bucket_t *bu = m->buckets + b;
        bu->size = bu->length;
        bu->data = (kdl_hashmap_data_t *) m->s.realloc(bu->data, sizeof(kdl_hashmap_data_t) * bu->size);
    }
}

void kdl_hashmap_free(kdl_hashmap_t *m) {
    for (size_t i = 0; i < m->nBuckets; i++) {
        for (size_t f = 0; f < m->buckets[i].length; f++) {
            m->s.free(m->buckets[i].data[f].key);
            m->freeFunc(m->s, m->buckets[i].data[f].data);
        }
        m->s.free(m->buckets[i].data);
    }
    m->s.free(m->buckets);
    memset(m, 0, sizeof(kdl_hashmap_t));
}

void kdl_hashmap_init(kdl_state_t s, kdl_hashmap_t *m, int precision, kdl_hashmap_dataFree_t freeFunc) {
    m->s = s;
    m->freeFunc = freeFunc;
    m->nElements = 0;
    m->nBuckets = 1 << precision;
    m->mask = m->nBuckets - 1; // Know your bitmath
    m->buckets = (kdl_hashmap_bucket_t *) s.malloc(sizeof(kdl_hashmap_bucket_t) * m->nBuckets);
    memset(m->buckets, 0, sizeof(kdl_hashmap_bucket_t) * m->nBuckets);
}
