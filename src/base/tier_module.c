#include "tier_module.h"

#include <string.h>

#include "src/include/conf.h"

//-----------------------------------------------------------------------
// write io progress.
//-----------------------------------------------------------------------
static inline int32_t UpdateLFU(TierModule* tierModule, PageFreq* pageFreq, uint64_t lba)
{
    uint32_t freq = 0.7 * pageFreq->readFreq + 0.3 * pageFreq->writeFreq;
    return tierModule->data.lfu->intf->update((void*)tierModule->data.lfu, (void*)&lba, freq);
}

static int32_t WriteIO(void* self, uint64_t lba, uint32_t length, PageFreq* oldPageFreq, bool isHost)
{
    int32_t ret = RETURN_ERROR;
    TierModule* tierModule = (TierModule*)self;
    Indexer* pageFreqIndex = tierModule->data.pageFreqIndexer;
    lba = (lba >> PAGE_8K_SHIFT) << PAGE_8K_SHIFT;
    PageFreq* pageFreq = NULL;
    ret = pageFreqIndex->intf->query((void*)pageFreqIndex, &lba, (void**)&pageFreq);
    if (ret != RETURN_OK) {
        /*new inserted page*/
        pageFreq = (PageFreq*)malloc(sizeof(PageFreq));
        ASSERT(pageFreq != NULL);
        memset(pageFreq, 0, sizeof(PageFreq));
        pageFreq->writeFreq = oldPageFreq->writeFreq;
        pageFreq->readFreq = oldPageFreq->readFreq;
        ret = pageFreqIndex->intf->insertKV((void*)pageFreqIndex, &lba, pageFreq);
        ASSERT(ret == RETURN_OK);
    }

    if (isHost) {
        /*update frequency*/
        pageFreq->writeFreq++;
    }

    /*update lfu*/
    ret = UpdateLFU(tierModule, pageFreq, lba);
    ASSERT(ret == RETURN_OK);
    return ret;
}

//------------------------------------------------------------------------
// read io progress.
//------------------------------------------------------------------------
static int32_t ReadHostIO(void* self, uint64_t lba, uint32_t length)
{
    int32_t ret = RETURN_ERROR;
    TierModule* tierModule = (TierModule*)self;
    Indexer* pageFreqIndex = tierModule->data.pageFreqIndexer;
    lba = (lba >> PAGE_8K_SHIFT) << PAGE_8K_SHIFT;
    PageFreq* pageFreq = NULL;
    ret = pageFreqIndex->intf->query((void*)pageFreqIndex, &lba, (void**)&pageFreq);
    if (ret == RETURN_OK) {
        /* update frequency */
        pageFreq->readFreq++;
        /* update lfu */
        ret = UpdateLFU(tierModule, pageFreq, lba);
        ASSERT(ret == RETURN_OK);
    }
    return ret;
}

//------------------------------------------------------------------------
// tier down progress.
//------------------------------------------------------------------------
static int32_t DeleteData(void* self, uint64_t lba, uint32_t length)
{
    int ret = RETURN_ERROR;
    TierModule* tierModule = (TierModule*)self;
    lba = (lba >> PAGE_8K_SHIFT) << PAGE_8K_SHIFT;
    ret = tierModule->data.lfu->intf->delete (tierModule->data.lfu, &lba);
    ASSERT(ret == RETURN_OK);
    tierModule->data.pageFreqIndexer->intf->delete (tierModule->data.pageFreqIndexer, &lba);
    ASSERT(ret == RETURN_OK);
    return ret;
}

//------------------------------------------------------------------------
// tier down progress.
//------------------------------------------------------------------------
static int32_t TierDown(void* self, TierPage* pages, uint32_t pageCnt)
{
    int ret = RETURN_ERROR;
    TierModule* tierModule = (TierModule*)self;
    Indexer* pageFreqIndex = (Indexer*)tierModule->data.pageFreqIndexer;
    /* 1.evict least frequently used block from lfu */
    for (int i = 0; i < pageCnt; i++) {
        ret = tierModule->data.lfu->intf->evict(tierModule->data.lfu, &(pages[i].lba));
        ASSERT(ret == RETURN_OK);
        PageFreq* pageFreq = NULL;
        ret = pageFreqIndex->intf->query((void*)pageFreqIndex, &(pages[i].lba), (void**)&pageFreq);
        ASSERT(ret == RETURN_OK);
        memcpy(&pages[i].freq, pageFreq, sizeof(PageFreq));
        ret = pageFreqIndex->intf->delete ((void*)pageFreqIndex, &(pages[i].lba));  // memory free in it
        ASSERT(ret == RETURN_OK);
    }
    return ret;
}

//------------------------------------------------------------------------
// query freq progress.
//------------------------------------------------------------------------
static int32_t QueryPageHistoryFreq(void* self, uint64_t lba, PageFreq* pageFreq)
{
    int32_t ret = RETURN_ERROR;
    TierModule* tierModule = (TierModule*)self;
    Indexer* pageFreqIndex = tierModule->data.pageFreqIndexer;
    lba = (lba >> PAGE_8K_SHIFT) << PAGE_8K_SHIFT;
    ret = pageFreqIndex->intf->query((void*)pageFreqIndex, &lba, (void**)&pageFreq);
    return ret;
}

static TierModuleIntf g_TierModuleIntf = {
    .writeIO = WriteIO,
    .readIO = ReadHostIO,
    .tierDown = TierDown,
    .delete = DeleteData,
    .queryFrequency = QueryPageHistoryFreq,
};

int32_t CreateTierModule(TierModule** __tierModule, TierModuleConfigure* conf)
{
    TierModule* tierModule = (TierModule*)malloc(sizeof(TierModule));
    if (!tierModule) {
        return RETURN_ERROR;
    }
    (void)memset(tierModule, 0, sizeof(TierModule));

    //-------------------------------------
    // Create space index.
    //-------------------------------------
    IndexerConfigure pageFreqIndexConf = {
        .keySize = sizeof(uint64_t),
        .bucketCount = conf->indexBucketNum * 100,
    };

    void* __pageFreqIndexer = NULL;
    int32_t ret = CreateIndexer(&__pageFreqIndexer, &pageFreqIndexConf);
    ASSERT(ret == RETURN_OK);
    tierModule->data.pageFreqIndexer = __pageFreqIndexer;

    //-------------------------------------
    // Create LFU.
    //-------------------------------------
    LFUConfigure lfuConf = {
        .keySize = sizeof(uint64_t),
        .bucketCount = conf->indexBucketNum * 100,
    };

    void* __lfu = NULL;
    ret = CreateLFU(&__lfu, &lfuConf);
    ASSERT(ret == RETURN_OK);
    tierModule->data.lfu = __lfu;
    tierModule->intf = &g_TierModuleIntf;
    *(__tierModule) = tierModule;
    return RETURN_OK;
}

void DestroyTierModule(TierModule** tierModule)
{
    if (*tierModule == NULL) {
        return;
    }
    DestroyLFU((*tierModule)->data.lfu);
    DestroyIndexer((*tierModule)->data.pageFreqIndexer);
    free(*tierModule);
    *tierModule = NULL;
}