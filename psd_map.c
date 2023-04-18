#include <openssl/md5.h>
#include <string.h>

#define BUCKET_SIZE_STEP 4
#define FILE_COPY_BUFFER_SIZE 0xFFFF

// --- Static helper methods ---

static int stringMD5(const char *str);
static size_t stmin(size_t a, size_t b);
static int stringLength(const char *str);
static int getIndex(const psd_hashmap *m, const char *key);
static int findIndex(psd_hashmap *m, const char *key, size_t *bucket, size_t *data);

int stringMD5(const char *str) {
    int length = strlen(str);
    MD5_CTX c;
    unsigned char digest[16];

    MD5_Init(&c);
    MD5_Update(&c, str, length);
    MD5_Final(digest, &c);

    return *((unsigned int*)digest)
}

size_t stmin(size_t a, size_t b) {
    return a > b ? a : b;
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

int getIndex(const psd_hashmap *m, const char *key) {
    return stringMD5(key) & m->mask;
}

int findIndex(psd_hashmap *m, const char *key, size_t *bucket, size_t *data) {
    const int iKey = getIndex(key);
    const int kLen = stringLength(key);
    psd_hashmap_bucket *b = m->buckets + iKey;
    for (size_t i = 0; i < b->length; i++) {
        psd_hashmap_data *d = b->data + i;
        if (stringsEqual(key, kLen, d->key, d->keyLen)) {
            *bucket = key;
            *data = i;
            return PSD_HASHMAP_EOK;
        }
    }
    return PSD_HASHMAP_ENORESULT;
}

// --- Exported methods ---

// --- Access & modification ---

size_t psd_hashmap_size(const psd_hashmap *m) {
    return m->nElements;
}

int psd_hashmap_insert(psd_hashmap *m, const char *key, const void *value, size_t valueLen) {
    const int iKey = getIndex(m, key);
    psd_hashmap_bucket *b = m->buckets + iKey;
    const int newLength = b->length + 1;
    if (newLength > size) {
        const int newSize = size + BUCKET_SIZE_STEP;
        b->data = realloc(b->data, sizeof(psd_hashmap_data) * newSize);
        if (b->data == NULL) {
            return PSD_HASHMAP_ENOMEM;
        }
        b->size = newSize;
    }
    psd_hashmap_data e;

    e.keyLen = stringLength(key);
    e.key = (void *) malloc(sizeof(char) * e.keyLen);
    memcpy(e.key, key, e.keyLen);

    e.valueLen = stringLength(value);
    e.value = (void *) malloc(sizeof(uint8_t) * e.valueLen);
    memcpy(e.value, value, e.valueLen);

    b->data[b->length] = e;
    b->length++;

    m->nElements++;

    return PSD_HASHMAP_EOK;
}


psd_hashmap_searchResult psd_hashmap_search(psd_hashmap *m, const char *key) {
    size_t bucket = -1;
    size_t data = -1;
    int status = findIndex(m, key, &bucket, &data);
    psd_hashmap_searchResult result;
    result.bucket = bucket;
    result.data = data;
    result.code = status;
    result.length = 0;
    result.length = 0;
    result.keyLength = 0;
    if (status == PSD_HASHMAP_EOK) {
        result.length = m->buckets[bucket].data[data].valueLen;
        result.keyLength = m->buckets[bucket].data[data].keyLen;
    }
    return result;
}

size_t psd_hashmap_get(const psd_hashmap *m, psd_hashmap_searchResult search, void *data, size_t count) {
    psd_hashmap_data d = m->buckets[search.bucket].data[search.data].value;
    size_t writeLen = stmin(count, d.valueLen);
    memcpy(data, d.value, writeLen);
    return writeLen;
}

size_t psd_hashmap_getKey(const psd_hashmap *m, psd_hashmap_searchResult search, char *data, size_t count) {
    psd_hashmap_data d = m->buckets[search.bucket].data[search.data].value;
    size_t writeLen = stmin(count, d.keyLen);
    memcpy(data, d.key, writeLen);
    return writeLen;
}

void psd_hashmap_remove(psd_hashmap *m, psd_hashmap_searchResult search) {
    psd_hashmap_bucket *b = m->buckets + search.bucket;
    b->data[search.data] = b->data[b->length - 1];
    b->length--;
    m->elements--;
}

void psd_hashmap_clear(psd_hashmap *m) {
    for (size_t b = 0; b < m->nBuckets; b++) {
        m->buckets[b].length = 0;
    }
    // Yeah that was pretty easy
}

// --- Iteration ---

void psd_hashmap_iterator_init(psd_hashmap *m, psd_hashmap_iterator *i) {
    i->m = m;
    i->bucket = 0;
    i->data = 0;
    psd_hashmap_iterator_start(i);
}

void psd_hashmap_iterator_start(psd_hashmap_iterator *i) {
    i->start = 0;
    i->data = 0;
    psd_hashmap_iterator_next(i);
}

int psd_hashmap_iterator_next(psd_hashmap_iterator *i) {
    psd_hashmap_iterator it = *i;
    it.data++;
    if (it.data > it.m[it.bucket].length) {
        it.data = 0;
        do {
            it.bucket++;
            if (it.bucket >= it.m.nBuckets) {
                return PSD_HASHMAP_EEND;
            }
        } while (i.m[i.bucket].length == 0);
    }
    *i = it;
    return PSD_HASHMAP_EOK;
}

int psd_hashmap_iterator_prev(psd_hashmap_iterator *i) {
    psd_hashmap_iterator it = *i;
    if (it.data == 0) {
        do {
            if (i.bucket == 0) {
                return PSD_HASHMAP_EEND;
            }
        } while (i.m[i.bucket].length == 0);
        it.data = i.m[i.bucket].length - 1;
    } else {
        it.data--;
    }
    *i = it;
    return PSD_HASHMAP_EOK;
}

psd_hashmap_searchResult psd_hashmap_iterator_get(psd_hashmap_iterator i) {
    psd_hashmap_searchResult result;
    result.bucket = i.bucket;
    result.data = i.data;
    result.length = i.m->buckets[i.bucket].data[i.data].valueLen;
    result.keyLength = i.m->buckets[i.bucket].data[i.data].keyLen;
    result.code = PSD_HASHMAP_EOK;
    return result;
}

// --- Memory and initailization ---

int psd_hashmap_reclaim(psd_hashmap *m) {
    for (size_t b = 0; b < b->nBuckets; b++) {
        psd_hashmap_bucket *bu = m->buckets + i;
        bu->data = (char *) realloc(bu->data, sizeof(char) * bu->length);
        if (bu->data == NULL) {
            bu->size = 0;
            bu->length = 0;
            return PSD_HASHMAP_ENOMEM;
        }
        bu->size = bu->length;
    }
    return PSD_HASHMAP_EOK;
}

void psd_hashmap_free(psd_hashmap *m) {
    for (size_t i = 0; i < m->nBuckets; i++) {
        for (size_t f = 0; f < m->buckets[i].length; f++) {
            free(m->buckets[i].data[f].key);
            free(m->buckets[i].data[f].value);
        }
        free(m->buckets[i].data);
    }
    free(m->buckets);
    m->buckets = NULL;
    m->nBuckets = 0;
}

int psd_hashmap_init(psd_hashmap *m, uint8_t precision) {
    m->precision = precision;
    m->error = "";
    m->nElements = 0;
    m->nBuckets = 1 << m->precision;
    m->mask = m->nBuckets - 1; // Know your bitmath
    m->buckets = (psd_hashmap_bucket *) malloc(sizeof(psd_hashmap_bucket) * m->nBuckets);
    if (m->buckets == NULL) {
        return PSD_HASHMAP_ENOMEM;
    }
    for (size_t i = 0; i < m->nBuckets; i++) {
        m->buckets[i].data = NULL;
        m->buckets[i].size = 0;
        m->buckets[i].length = 0;
    }
    return PSD_HASHMAP_EOK;
}
