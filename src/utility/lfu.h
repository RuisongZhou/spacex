#ifndef LFU_H

#define LFU_H

#include "indexer.h"
#include "infracture.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct tagFreqNode{
    struct list_head freqNode;
    void *pItemHead;
    uint32_t freq;
}FreqNode;
typedef struct tagItemNode{
    struct list_head itemNode;
    void *pFreqHead;
    uint64_t key;
}ItemNode;

typedef struct tagLFUData{
    struct list_head freqQ;
    Indexer *lfuIndexer;
    uint32_t age;
    uint32_t keySize;
}LFUData;

typedef struct tagLFUIntf{
    int32_t (*update) (void *self, void *key, uint32_t freq);
    int32_t (*evict) (void *self, void *key);
    int32_t (*delete) (void *self, void *key);
}LFUIntf;

typedef struct tagLFU{
    LFUData data;
    LFUIntf *intf;
}LFU;

typedef struct tagLFUConfigure{
    uint32_t keySize;
    uint32_t bucketCount;
}LFUConfigure;

int32_t CreateLFU(void **fle, LFUConfigure *con);

void DestroyLFU(LFU *lfu);

#ifdef __cplusplus
}
#endif

#endif //LFU_H
