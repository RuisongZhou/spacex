
#include "gc_module.h"

#include <string.h>

typedef struct tagWriteContext {
    uint64_t lba;
    Block *writeBlock;
    PageLoc *writePageLoc;
    Block *oldBlock;
    GCModule *gcModule;
} WriteContext;

typedef struct tagStepEntry {
    char *stepDesc;
    int32_t (*execute)(WriteContext *context);
} StepEntry;

static inline Block *TryAllocBlockFromFreeQ(GCModule *gcModule)
{
    ASSERT(gcModule->data.freeQsize > 0);
    // fetch from tail
    Block *block = (Block *)list_entry(gcModule->data.freeQ.prev, Block, gcNode);
    list_del_init(&block->gcNode);
    gcModule->data.freeQsize--;
    block->blockStatus = WRITING;
    block->createSeqNo = gcModule->data.hostIOSeqNo;

    return block;
}

static inline Block *GetHostBlock(GCModule *gcModule)
{
    if (gcModule->data.hostBlock == NULL) {
        gcModule->data.hostBlock = TryAllocBlockFromFreeQ(gcModule);
        gcModule->data.hostBlock->blockType = HOST_BLOCK;
    }
    return gcModule->data.hostBlock;
}

static int32_t WriteHostBlock(WriteContext *context)
{
    GCModule *gcModule = context->gcModule;

    Block *block = GetHostBlock(gcModule);
    ASSERT(block);
    block->pages[block->pageCursor].lba = context->lba;
    block->pages[block->pageCursor].status = VALID;
    block->pages[block->pageCursor].pageLoc.blockId = block->blockId;
    block->pages[block->pageCursor].pageLoc.pageId = block->pageCursor;

    context->writeBlock = block;
    context->writePageLoc = &block->pages[block->pageCursor].pageLoc;

    block->pageCursor++;

    return RETURN_OK;
}

static void MarkPageInvalid(GCModule *gcModule, uint64_t lba, PageLoc *pageLoc, Block **ppBlock)
{
    Indexer *blockIndexer = gcModule->data.blockIndexer;
    Indexer *spaceIndexer = gcModule->data.spaceIndexer;

    int32_t ret = blockIndexer->intf->query(blockIndexer, &pageLoc->blockId, (void **)ppBlock);
    ASSERT(ret == RETURN_OK);
    (*ppBlock)->pages[pageLoc->pageId].status = INVALID;
    ret = spaceIndexer->intf->delete (spaceIndexer, (void *)&lba);
    ASSERT(ret == RETURN_OK);
    (*ppBlock)->garbageCnt++;
}

static int32_t UpdateSpaceIndex(WriteContext *context)
{
    GCModule *gcModule = context->gcModule;
    Indexer *spaceIndexer = gcModule->data.spaceIndexer;

    PageLoc **ppPageLoc = NULL;
    int32_t ret = spaceIndexer->intf->query(spaceIndexer, (void *)&context->lba, (void **)&ppPageLoc);
    if (ret == RETURN_OK) {
        MarkPageInvalid(gcModule, context->lba, *ppPageLoc, &context->oldBlock);
    }

    ppPageLoc = (PageLoc **)malloc(sizeof(PageLoc *));
    *ppPageLoc = context->writePageLoc;
    ret = spaceIndexer->intf->insertKV(spaceIndexer, (void *)&context->lba, (void *)ppPageLoc);
    ASSERT(ret == RETURN_OK);

    return ret;
}

static inline uint32_t GetCurBucketIdx(uint32_t garbageCnt)
{
    uint32_t waterLevel = garbageCnt / (PAGE_NUM_PER_BLOCK / (GC_BUCKET_NUM - 1));
    return waterLevel >= GC_BUCKET_NUM ? (GC_BUCKET_NUM - 1) : waterLevel;
}

static inline void Insert2GarbageBuckets(GarbageBuckets *garbageBuckets, Block *block)
{
    // ASSERT(block->pageCursor == PAGE_NUM_PER_BLOCK);
    uint32_t garbageWL = GetCurBucketIdx(block->garbageCnt);
    block->garbageWL = garbageWL;
    block->blockStatus = FULL;

    list_add(&block->gcNode, garbageBuckets->buckets[garbageWL].prev);
    garbageBuckets->blockCnt[garbageWL]++;
}

static void AdjustGarbageBuckets(GarbageBuckets *garbageBuckets, Block *block)
{
    if (block == NULL || block->blockStatus != FULL) {
        return;
    }
    uint32_t garbageWL = GetCurBucketIdx(block->garbageCnt);
    if (garbageWL == block->garbageWL) {
        return;
    }
    list_del_init(&block->gcNode);
    garbageBuckets->blockCnt[block->garbageWL]--;
    list_add(&block->gcNode, garbageBuckets->buckets[garbageWL].prev);
    block->garbageWL = garbageWL;
    garbageBuckets->blockCnt[garbageWL]++;
}

static int32_t AttachGarbageBuckets(WriteContext *context)
{
    GCModule *gcModule = context->gcModule;

    if (context->writeBlock->pageCursor == PAGE_NUM_PER_BLOCK) {
        Insert2GarbageBuckets(&gcModule->data.garbageBuckets, context->writeBlock);
        gcModule->data.hostBlock = NULL;
    }

    AdjustGarbageBuckets(&gcModule->data.garbageBuckets, context->oldBlock);

    return RETURN_OK;
}

StepEntry g_StepEntry[] = {
    {"write host block", WriteHostBlock},
    {"update space index", UpdateSpaceIndex},
    {"attach garbage list", AttachGarbageBuckets},
};

static int32_t WriteIO(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc)
{
    GCModule *gcModule = (GCModule *)self;

    lba = lba >> PAGE_8K_SHIFT << PAGE_8K_SHIFT;
    WriteContext context = {
        .lba = lba,
        .writeBlock = NULL,
        .writePageLoc = NULL,
        .oldBlock = NULL,
        .gcModule = gcModule,
    };

    for (uint32_t i = 0; i < sizeof(g_StepEntry) / sizeof(StepEntry); i++) {
        int32_t ret = g_StepEntry[i].execute(&context);
        ASSERT(ret == RETURN_OK);
    }

    gcModule->data.hostIOSeqNo++;
    pageLoc->blockId = context.writePageLoc->blockId;
    pageLoc->pageId = context.writePageLoc->pageId;

    return RETURN_OK;
}

static inline Block *GetGCBlock(GCModule *gcModule)
{
    if (gcModule->data.gcBlock == NULL) {
        gcModule->data.gcBlock = TryAllocBlockFromFreeQ(gcModule);
        gcModule->data.gcBlock->blockType = GC_BLOCK;
    }
    return gcModule->data.gcBlock;
}

static int32_t WriteGCBlock(GCModule *gcModule, PageInfo *page, PageLoc *writePageLoc)
{
    Block *opBlock = GetGCBlock(gcModule);
    ASSERT(opBlock);

    Indexer *spaceIndexer = gcModule->data.spaceIndexer;
    int32_t ret = spaceIndexer->intf->delete (spaceIndexer, &page->lba);
    ASSERT(ret == RETURN_OK);

    opBlock->pages[opBlock->pageCursor].lba = page->lba;
    opBlock->pages[opBlock->pageCursor].status = VALID;
    opBlock->pages[opBlock->pageCursor].pageLoc.blockId = opBlock->blockId;
    opBlock->pages[opBlock->pageCursor].pageLoc.pageId = opBlock->pageCursor;
    PageLoc **ppPageLoc = (PageLoc **)malloc(sizeof(PageLoc *));
    ASSERT(ppPageLoc);
    *ppPageLoc = &(opBlock->pages[opBlock->pageCursor].pageLoc);

    ret = spaceIndexer->intf->insertKV(spaceIndexer, &page->lba, (void *)ppPageLoc);
    ASSERT(ret == RETURN_OK);

    opBlock->pageCursor++;

    if (opBlock->pageCursor == PAGE_NUM_PER_BLOCK) {
        Insert2GarbageBuckets(&gcModule->data.garbageBuckets, opBlock);
        gcModule->data.gcBlock = NULL;
    }

    writePageLoc->blockId = (*ppPageLoc)->blockId;
    writePageLoc->pageId = (*ppPageLoc)->pageId;

    return RETURN_OK;
}

static void ReturnBlock2FreeQ(GCModule *gcModule, Block *block)
{
    // ASSERT(gcModule && block);

    list_del_init(&block->gcNode);
    gcModule->data.garbageBuckets.blockCnt[block->garbageWL]--;

    uint32_t blockId = block->blockId;
    (void)memset(block, 0, sizeof(Block));
    block->blockId = blockId;
    // block->blockStatus = FREE;

    list_add(&block->gcNode, &gcModule->data.freeQ);  // back to head
    gcModule->data.freeQsize++;
}

static int32_t GarbageCollection(void *self, uint32_t recyleCnt, uint32_t *eraseBlockId, uint32_t *gcWriteCnt,
                                 PageLoc *readPageLoc, PageLoc *writePageLoc)
{
    GCModule *gcModule = (GCModule *)self;
    uint32_t gcBucketIdx = 0;
    struct list_head *targetBucket = NULL;
    struct list_head *cur = NULL;
    struct list_head *next = NULL;

    for (gcBucketIdx = GC_BUCKET_NUM - 1; gcBucketIdx >= 0 && recyleCnt > 0; gcBucketIdx--) {
        if (gcModule->data.garbageBuckets.blockCnt[gcBucketIdx] == 0) continue;

        targetBucket = &gcModule->data.garbageBuckets.buckets[gcBucketIdx];
        list_for_each_safe(cur, next, targetBucket)
        {
            Block *block = (Block *)list_entry(cur, Block, gcNode);

            for (uint32_t pageIdx = 0; pageIdx < block->pageCursor; pageIdx++) {
                if (block->pages[pageIdx].status == VALID) {
                    readPageLoc[*gcWriteCnt].blockId = block->blockId;
                    readPageLoc[*gcWriteCnt].pageId = pageIdx;
                    int32_t ret = WriteGCBlock(gcModule, &block->pages[pageIdx], &writePageLoc[*gcWriteCnt]);
                    ASSERT(ret == RETURN_OK);
                    (*gcWriteCnt)++;
                }
            }
            ReturnBlock2FreeQ(gcModule, block);
            recyleCnt--;
            eraseBlockId[recyleCnt] = block->blockId;
            if (recyleCnt == 0) break;
        }
    }
    return RETURN_OK;
}

static int32_t ReadIO(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc)
{
    GCModule *gcModule = (GCModule *)self;
    Indexer *spaceIndexer = gcModule->data.spaceIndexer;
    lba = lba >> PAGE_8K_SHIFT << PAGE_8K_SHIFT;

    PageLoc **ppPageLoc = NULL;
    int32_t ret = spaceIndexer->intf->query(spaceIndexer, (void *)&lba, (void **)&ppPageLoc);
    ASSERT(ret == RETURN_OK);

    pageLoc->blockId = (*ppPageLoc)->blockId;
    pageLoc->pageId = (*ppPageLoc)->pageId;

    return RETURN_OK;
}

static int32_t Delete(void *self, uint64_t lba, uint32_t length, PageLoc *pageLoc)
{
    GCModule *gcModule = (GCModule *)self;
    Indexer *spaceIndexer = gcModule->data.spaceIndexer;
    lba = lba >> PAGE_8K_SHIFT << PAGE_8K_SHIFT;

    PageLoc **ppPageLoc = NULL;
    Block *block = NULL;
    int32_t ret = spaceIndexer->intf->query(spaceIndexer, (void *)&lba, (void **)&ppPageLoc);
    ASSERT(ret == RETURN_OK);

    pageLoc->blockId = (*ppPageLoc)->blockId;
    pageLoc->pageId = (*ppPageLoc)->pageId;

    MarkPageInvalid(gcModule, lba, *ppPageLoc, &block);
    AdjustGarbageBuckets(&gcModule->data.garbageBuckets, block);

    return RETURN_OK;
}

bool IsExist(void *self, uint64_t lba)
{
    GCModule *gcModule = (GCModule *)self;
    Indexer *spaceIndexer = gcModule->data.spaceIndexer;
    lba = lba >> PAGE_8K_SHIFT << PAGE_8K_SHIFT;

    void *pad = NULL;
    int32_t ret = spaceIndexer->intf->query(spaceIndexer, (void *)&lba, (void **)&pad);
    return ret == RETURN_OK;
}

uint32_t GetLogicalSizeOccupied(void *self)
{
    GCModule *gcModule = (GCModule *)self;
    return gcModule->data.spaceIndexer->data.indexSize;
}

uint32_t GetStorageCapacity(void *self)
{
    GCModule *gcModule = (GCModule *)self;
    return gcModule->data.blockIndexer->data.indexSize * PAGE_NUM_PER_BLOCK;
}

uint32_t GetFreeBlockNum(void *self)
{
    GCModule *gcModule = (GCModule *)self;
    return gcModule->data.freeQsize;
}

static GCModuleIntf g_gcModuleIntf = {
    .readIO = ReadIO,
    .writeIO = WriteIO,
    .garbageCollection = GarbageCollection,
    .delete = Delete,
    .isExist = IsExist,
    .getLogicalSizeOccupied = GetLogicalSizeOccupied,
    .getStorageCapacity = GetStorageCapacity,
    .getFreeBlockNum = GetFreeBlockNum,
};

int32_t CreateGCModule(GCModule **ppGCModule, GCModuleConfigure *conf)
{
    GCModule *gcModule = (GCModule *)malloc(sizeof(GCModule));
    ASSERT(gcModule);
    memset(gcModule, 0, sizeof(GCModule));
    INIT_LIST_HEAD(&gcModule->data.freeQ);
    for (uint32_t i = 0; i < GC_BUCKET_NUM; i++) {
        INIT_LIST_HEAD(&gcModule->data.garbageBuckets.buckets[i]);
    }

    // blockindex
    Indexer *blockIndexer = NULL;
    IndexerConfigure blockIndexConf = {
        .keySize = sizeof(uint32_t),
        .bucketCount = conf->indexBucketCnt,
    };
    CreateIndexer((void **)&blockIndexer, &blockIndexConf);
    gcModule->data.blockIndexer = blockIndexer;

    // spaceindex
    Indexer *spaceIndexer = NULL;
    IndexerConfigure spaceIndexConf = {
        .keySize = sizeof(uint64_t),
        .bucketCount = conf->indexBucketCnt * 100,
    };
    CreateIndexer((void **)&spaceIndexer, &spaceIndexConf);
    gcModule->data.spaceIndexer = spaceIndexer;

    for (uint32_t i = 0; i < conf->totalBlockCnt; i++) {
        Block *block = (Block *)malloc(sizeof(Block));
        memset(block, 0, sizeof(Block));
        block->blockId = i;
        INIT_LIST_HEAD(&block->gcNode);
        list_add(&block->gcNode, &gcModule->data.freeQ);
        gcModule->data.freeQsize++;

        blockIndexer->intf->insertKV(blockIndexer, &i, block);
    }

    gcModule->intf = &g_gcModuleIntf;
    (*ppGCModule) = gcModule;

    return RETURN_OK;
}

int32_t DestroyGCModule(GCModule **ppGCModule)
{
    DestroyIndexer((*ppGCModule)->data.blockIndexer);
    DestroyIndexer((*ppGCModule)->data.spaceIndexer);
    free(*ppGCModule);
    *ppGCModule = NULL;
    return RETURN_OK;
}
