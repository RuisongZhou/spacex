
#ifndef GC_MODULE_H
#define GC_MODULE_H

#include <stdint.h>

#include "src/base/disk_operator.h"
#include "src/include/conf.h"
#include "src/utility/indexer.h"
#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC_BUCKET_NUM 50

enum PAGE_STATUS_E {
    WRITEABLE = 0,
    VALID = 1,
    INVALID = 2,
};

enum BLOCK_STATUS_E {
    FREE = 0,
    WRITING = 1,
    FULL = 2,
};

enum BLOCK_TYPE_E {
    HOST_BLOCK = 0,
    GC_BLOCK = 1,
};

typedef struct tagPageLoc {
    uint32_t blockId;
    uint32_t pageId;
} PageLoc;

typedef struct tagPageInfo {
    uint64_t lba;    /* Logic address */
    uint32_t status; /* Status of page */
    PageLoc pageLoc;
    // uint32_t gcNum; /* GC count since last host write */
} PageInfo;

typedef struct tagBlock {
    struct list_head gcNode; /* used to attach free queue or garbage queue */
    uint32_t blockId;
    uint32_t blockStatus;
    uint32_t garbageWL;
    PageInfo pages[PAGE_NUM_PER_BLOCK];
    uint32_t pageCursor;
    uint32_t garbageCnt;
    uint32_t blockType;
    uint64_t createSeqNo;
} Block;

typedef struct tagGarbageBuckets {
    struct list_head buckets[GC_BUCKET_NUM];
    uint32_t blockCnt[GC_BUCKET_NUM];
} GarbageBuckets;

typedef struct tagGCModuleData {
    Indexer *spaceIndexer;
    Indexer *blockIndexer;
    struct list_head freeQ;
    int32_t freeQsize;
    GarbageBuckets garbageBuckets;
    Block *hostBlock;
    Block *gcBlock;
    DiskOperator *diskOp;
    uint64_t hostIOSeqNo;
} GCModuleData;

typedef struct tagGCModuleIntf {
    int32_t (*readIO)(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc);
    int32_t (*writeIO)(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc);
    int32_t (*garbageCollection)(void *self, uint32_t recyleCnt, uint32_t *eraseBlockId,
                                 uint32_t *gcWriteCnt, PageLoc *readPageLoc, PageLoc *writePageLo);
    int32_t (*delete)(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc);
    bool (*isExist)(void *self, uint64_t lba);
    uint32_t (*getLogicalSizeOccupied)(void *self);
    uint32_t (*getStorageCapacity)(void *self);
    uint32_t (*getFreeBlockNum)(void *self);
} GCModuleIntf;

typedef struct tagGCModule {
    GCModuleData data;
    GCModuleIntf *intf;
} GCModule;

typedef struct tagGCModuleConfigure {
    uint32_t totalBlockCnt;
    uint32_t indexBucketCnt;
} GCModuleConfigure;

int32_t CreateGCModule(GCModule **ppGCModule, GCModuleConfigure *conf);

int32_t DestroyGCModule(GCModule **ppGCModule);

#ifdef __cplusplus
}
#endif

#endif  // GC_MODULE_H