#ifndef INDEX_H
#define INDEX_H

#define INDEXER H

#include <stdint.h>

#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagHashTable {
    struct list_head *buckets;
    uint32_t bucketCount;
} HashTable;

typedef struct tagHashEntry {
    struct list_head node;
    void *key;
    void *val;
} HashEntry;

typedef struct tagIndexerData {
    uint32_t keySize;
    void *indexHandle;
    uint32_t indexSize;
} IndexerData;

typedef struct tagIndexerIntf {
    int32_t (*insertKV)(void *self, void *key, void *val);
    int32_t (*query)(void *self, void *key, void **val);
    int32_t (*delete)(void *self, void *key);
} IndexerIntf;

typedef struct tagIndexer {
    IndexerData data;
    IndexerIntf *intf;
} Indexer;

typedef struct tagIndexerConfigure {
    uint32_t keySize;
    uint32_t bucketCount;
} IndexerConfigure;

int32_t CreateIndexer(void **indexer, IndexerConfigure *conf);

int32_t DestroyIndexer(void *indexer);

#ifdef __cplusplus
}

#endif
#endif  // INDEX_H
