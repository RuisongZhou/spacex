#include "indexer.h"

#include <string.h>

#include "hash.h"

static inline uint32_t GetBucketIndex(int32_t *key, uint32_t keySize, uint32_t seed, uint32_t bucketCount)
{
    return HashFunc(key, keySize, seed) % bucketCount;
}

static HashEntry *CreateHashEntry(uint32_t keySize, void *key, void *val)
{
    HashEntry *entry = (HashEntry *)malloc(sizeof(HashEntry));
    if (!entry) {
        return NULL;
    }
    (void)memset(entry, 0, sizeof(HashEntry));
    entry->key = (void *)malloc(keySize);
    assert(entry->key);

    (void)memcpy(entry->key, key, keySize);
    INIT_LIST_HEAD(&entry->node);
    entry->val = val;
    return entry;
}

static inline void DestroyHashEntry(void *entry)
{
    HashEntry *__entry = (HashEntry *)entry;
    free(__entry->key);
    free(__entry->val);
    list_del_init(&__entry->node);
    free(__entry);
}

static int32_t Query(void *self, void *key, void **val)
{
    Indexer *indexer = (Indexer *)self;
    HashTable *table = (HashTable *)indexer->data.indexHandle;

    uint32_t bucketIdx = GetBucketIndex(key, indexer->data.keySize, 0, table->bucketCount);
    assert(bucketIdx < table->bucketCount);

    struct list_head *targetBucket = &table->buckets[bucketIdx];
    struct list_head *cur = NULL;
    struct list_head *next = NULL;
    list_for_each_safe(cur, next, targetBucket)
    {
        HashEntry *entry = (HashEntry *)list_entry(cur, HashEntry, node);
        if (memcmp(entry->key, key, indexer->data.keySize) == 0) {
            *val = entry->val;
            return RETURN_OK;
        }
    }
    return RETURN_ERROR;
}

static int32_t InsertKV(void *self, void *key, void *val)
{
    Indexer *indexer = (Indexer *)self;
    HashTable *table = (HashTable *)indexer->data.indexHandle;

    uint32_t bucketIdx = GetBucketIndex(key, indexer->data.keySize, 0, table->bucketCount);
    assert(bucketIdx < table->bucketCount);
    HashEntry *entry = CreateHashEntry(indexer->data.keySize, key, val);
    if (entry == NULL) return RETURN_ERROR;
    list_add(&entry->node, &table->buckets[bucketIdx]);
    indexer->data.indexSize++;
    return RETURN_OK;
}

static int32_t Delete(void *self, void *key)
{
    Indexer *indexer = (Indexer *)self;
    HashTable *table = (HashTable *)indexer->data.indexHandle;

    uint32_t bucketIdx = GetBucketIndex(key, indexer->data.keySize, 0, table->bucketCount);
    assert(bucketIdx < table->bucketCount);
    struct list_head *targetBucket = &table->buckets[bucketIdx];
    struct list_head *cur, *next;
    list_for_each_safe(cur, next, targetBucket)
    {
        HashEntry *entry = (HashEntry *)list_entry(cur, HashEntry, node);
        if (memcmp(entry->key, key, indexer->data.keySize) == 0) {
            DestroyHashEntry(entry);
            indexer->data.indexSize--;
        }
    }
    return RETURN_OK;
}

static IndexerIntf g_indexerIntf = {
    .insertKV = InsertKV,
    .query = Query,
    .delete = Delete,
};

int32_t CreateIndexer(void **indexer, IndexerConfigure *conf)
{
    Indexer *__indexer = (Indexer *)malloc(sizeof(Indexer));
    if (!__indexer) {
        return RETURN_ERROR;
    }
    (void)memset(__indexer, 0, sizeof(Indexer));
    HashTable *__table = (HashTable *)malloc(sizeof(HashTable));
    if (!__table) {
        free(__indexer);
        return RETURN_ERROR;
    }
    __table->bucketCount = conf->bucketCount;
    __table->buckets = (struct list_head *)malloc(sizeof(struct list_head) * conf->bucketCount);
    if (!__table->buckets) {
        free(__table);
        free(__indexer);
        return RETURN_ERROR;
    }

    for (uint32_t i = 0; i < __table->bucketCount; i++) {
        INIT_LIST_HEAD(&__table->buckets[i]);
    }
    __indexer->data.indexHandle = (void *)__table;
    __indexer->data.keySize = conf->keySize;
    __indexer->intf = &g_indexerIntf;
    *indexer = __indexer;
    return RETURN_OK;
}

int32_t DestroyIndexer(void *indexer)
{
    Indexer *__indexer = (Indexer *)indexer;
    HashTable *hashTable = (HashTable *)__indexer->data.indexHandle;
    for (uint32_t i = 0; i < hashTable->bucketCount; i++) {
        struct list_head *targetBucket = &hashTable->buckets[i];
        struct list_head *cur = NULL, *next = NULL;
        list_for_each_safe(cur, next, targetBucket)
        {
            HashEntry *entry = (HashEntry *)list_entry(cur, HashEntry, node);
            DestroyHashEntry(entry);
            __indexer->data.indexSize--;
        }
    }
    free(hashTable->buckets);
    free(hashTable);
    free(indexer);
    indexer = NULL;
    return RETURN_OK;
}
