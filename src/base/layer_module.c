
#include "layer_module.h"

#include <string.h>

#include "src/base/storage_system.h"
#include "src/third_party/c-logger/src/logger.h"
#include "src/utility/time.h"

static inline void DiskRead(DiskOperator *diskOp, PageLoc *pageLoc, uint32_t size, DiskIOType ioType, uint64_t ts)
{
    if (!diskOp) {
        return;
    }

    DiskIOInfo io = {
        .segmentId = pageLoc->blockId,
        .offset = pageLoc->pageId,
        .size = size,
        .ioType = ioType,
        .opType = DISK_READ,
        .timestamp = ts,
    };
    diskOp->intf->handleIO(diskOp, &io);
}

static inline void DiskWrite(DiskOperator *diskOp, PageLoc *pageLoc, uint32_t size, DiskIOType ioType, uint64_t ts)
{
    if (!diskOp) {
        return;
    }

    DiskIOInfo io = {
        .segmentId = pageLoc->blockId,
        .offset = pageLoc->pageId,
        .size = size,
        .ioType = ioType,
        .opType = DISK_WRITE,
        .timestamp = ts,
    };
    diskOp->intf->handleIO(diskOp, &io);
}

static inline void DiskErase(DiskOperator *diskOp, uint32_t blockId)
{
    if (!diskOp) {
        return;
    }

    DiskIOInfo io = {
        .segmentId = blockId,
        .ioType = BACK_GC,
        .opType = DISK_ERASE,
    };
    diskOp->intf->handleIO(diskOp, &io);
}

/*
 *  GC condition: reach the gc water level and the last GC task already finished
 */
static inline bool GCCondition(LayerModule *layerModule)
{
    GCModule *gcModule = layerModule->data.gcModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;
    if (diskOperator) {
        // t0 物理空间达到GC水位？ 上一次GC任务完成？
        return gcModule->intf->getFreeBlockNum(gcModule) < layerModule->data.gcWL
               && diskOperator->intf->queryGCQueueSize(diskOperator) == 0;
    } else {
        return gcModule->intf->getFreeBlockNum(gcModule) < layerModule->data.gcWL;
    }
}

/*
 *  GC stall: reach the gc limited water level and the last GC task still not finish
 */
static inline bool GCStallCondition(LayerModule *layerModule, uint32_t segmentId)
{
    GCModule *gcModule = layerModule->data.gcModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;
    if (!diskOperator) {
        return false;
    }
    // t0 物理空间达到临界水位，且上一次GC尚未完成？
    return (gcModule->intf->getFreeBlockNum(gcModule) <= layerModule->data.gcStallWL
            && diskOperator->intf->queryGCQueueSize(diskOperator) != 0);
}

/*
 *  GC slow: reach the gc water level or the last GC task still not finish
 */
static inline bool GCSlowCondition(LayerModule *layerModule)
{
    GCModule *gcModule = layerModule->data.gcModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;
    if (!diskOperator) {
        return false;
    }
    // t0 物理空间达到GC水位？ 上一次GC任务完成？
    return gcModule->intf->getFreeBlockNum(gcModule) < layerModule->data.gcWL
           || diskOperator->intf->queryGCQueueSize(diskOperator) != 0;
}

static int32_t HandleGC(LayerModule *layerModule)
{
    int32_t ret = RETURN_OK;
    GCModule *gcModule = layerModule->data.gcModule;
    StatModule *statModule = layerModule->data.statModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;

    // wait for host queue
    while (diskOperator->intf->queryHostQueueSize(diskOperator) != 0) {
        MicroSleep(100);
    }

    // valid data need to load & write(movement)
    uint32_t gcBlockCnt =
        layerModule->data.gcWL - gcModule->intf->getFreeBlockNum(gcModule) + layerModule->data.gcBlockCnt;
    PageLoc *readPageLoc = (PageLoc *)malloc(sizeof(PageLoc) * gcBlockCnt * PAGE_NUM_PER_BLOCK);
    ASSERT(readPageLoc);
    memset(readPageLoc, 0, sizeof(PageLoc) * gcBlockCnt * PAGE_NUM_PER_BLOCK);

    PageLoc *writePageLoc = (PageLoc *)malloc(sizeof(PageLoc) * gcBlockCnt * PAGE_NUM_PER_BLOCK);
    ASSERT(writePageLoc);
    memset(writePageLoc, 0, sizeof(PageLoc) * gcBlockCnt * PAGE_NUM_PER_BLOCK);

    uint32_t gcWriteCnt = 0;
    // blockId need to erase
    uint32_t *eraseBlockId = (uint32_t *)malloc(sizeof(uint32_t) * gcBlockCnt);
    ASSERT(eraseBlockId);
    memset(eraseBlockId, 0, sizeof(uint32_t) * layerModule->data.gcBlockCnt);
    ret = gcModule->intf->garbageCollection(gcModule, layerModule->data.gcBlockCnt, eraseBlockId, &gcWriteCnt,
                                            readPageLoc, writePageLoc);
    statModule->data.gcWriteCount += gcWriteCnt;
    statModule->data.gcCount++;
    if (gcWriteCnt > 0) {
        PageLoc *pageLoc = &writePageLoc[0];
        int32_t size = 0;
        for (int32_t idx = 0; idx < gcWriteCnt; idx++) {
            DiskRead(diskOperator, &readPageLoc[idx], 1, BACK_GC, 0);
            if (pageLoc->blockId != writePageLoc[idx].blockId) {
                DiskWrite(diskOperator, pageLoc, size, BACK_GC, 0);
                pageLoc = &writePageLoc[idx];
                size = 0;
            }
            size++;
        }
        DiskWrite(diskOperator, pageLoc, size, BACK_GC, 0);
    }
    // for (int32_t i = 0; i < gcBlockCnt; i++) {
    //     DiskErase(diskOperator, eraseBlockId[i]);
    // }
    free(readPageLoc);
    free(writePageLoc);
    free(eraseBlockId);
    return ret;
}

static inline bool TierDownCondition(LayerModule *layerModule)
{
    StorageSystem *storSys = (StorageSystem *)layerModule->data.storSys;
    LayerModule *t0 = storSys->data.tiers[0];
    LayerModule *t1 = storSys->data.tiers[0];
    GCModule *t0GC = t0->data.gcModule;
    GCModule *t1GC = t1->data.gcModule;
    DiskOperator *t0Disk = t0->data.diskOp;
    DiskOperator *t1Disk = t1->data.diskOp;

    if (t0->data.diskOp) {
        // t0 逻辑空间达到水位？ (X)
        return t0GC->intf->getLogicalSizeOccupied(t0GC) >= layerModule->data.tierDownWL
               && t0Disk->intf->queryTierQueueSize(t0Disk) == 0 && t1Disk->intf->queryTierQueueSize(t1Disk) == 0;
    } else {
        return t0GC->intf->getLogicalSizeOccupied(t0GC) >= layerModule->data.tierDownWL;
    }
}

static inline bool TierDownSlowStallCondition(LayerModule *layerModule, uint32_t waterLevel)
{
    DiskOperator *diskOperator = layerModule->data.diskOp;
    if (!diskOperator) {
        return false;
    }

    StorageSystem *storSys = (StorageSystem *)layerModule->data.storSys;
    LayerModule *t0 = storSys->data.tiers[0];
    LayerModule *t1 = storSys->data.tiers[0];
    GCModule *t0GC = t0->data.gcModule;
    GCModule *t1GC = t1->data.gcModule;
    DiskOperator *t0Disk = t0->data.diskOp;
    DiskOperator *t1Disk = t1->data.diskOp;

    return t0GC->intf->getLogicalSizeOccupied(t0GC) >= waterLevel
           && (t0Disk->intf->queryTierQueueSize(t0Disk) || t1Disk->intf->queryTierQueueSize(t1Disk));
}

static inline bool TierDownStallCondition(LayerModule *layerModule)
{
    return TierDownSlowStallCondition(layerModule, layerModule->data.tierDownStallWL);
}

static inline bool TierDownSlowCondition(LayerModule *layerModule)
{
    return TierDownSlowStallCondition(layerModule, layerModule->data.tierDownWL);
}

static int32_t HandleTierDown(LayerModule *layerModule, TierPage **pages, uint32_t *pageCnt)
{
    int32_t ret = RETURN_OK;
    GCModule *gcModule = layerModule->data.gcModule;
    TierModule *tierModule = layerModule->data.tierModule;
    StatModule *statModule = layerModule->data.statModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;

    /* get pages which need to tier down */
    *pageCnt = gcModule->intf->getLogicalSizeOccupied(gcModule) - layerModule->data.tierDownWL
               + layerModule->data.tierDownBlockCnt * PAGE_NUM_PER_BLOCK;
    *pages = (TierPage *)malloc((*pageCnt) * sizeof(TierPage));
    ASSERT((*pages) != NULL);
    memset(*pages, 0, (*pageCnt) * sizeof(TierPage));
    ret = tierModule->intf->tierDown(tierModule, *pages, (*pageCnt));
    ASSERT(ret == RETURN_OK);

    /* mark pages tier down as invalid */
    PageLoc *pageLoc = (PageLoc *)malloc(sizeof(PageLoc) * (*pageCnt));
    ASSERT(pageLoc);
    for (int i = 0; i < (*pageCnt); i++) {
        ret = gcModule->intf->delete (gcModule, (*pages)[i].lba, PAGE_SIZE, &pageLoc[i]);
        ASSERT(ret == RETURN_OK);
        statModule->data.tierDownWriteCount++;
    }

    statModule->data.tierDownCount++;
    ASSERT(gcModule->intf->getLogicalSizeOccupied(gcModule) <= layerModule->data.tierDownWL);
    for (int32_t idx = 0; idx < *pageCnt; idx++) {
        DiskRead(diskOperator, &pageLoc[idx], 1, BACK_TIER, 0);
    }
    free(pageLoc);
    return ret;
}

//
// write io progress.
//
static int32_t WriteHostIO(void *self, IO *io, PageFreq *oldPageFreq, TierPage **pages, uint32_t *pageCnt)
{
    int32_t ret = RETURN_ERROR;
    LayerModule *layerModule = (LayerModule *)self;
    GCModule *gcModule = layerModule->data.gcModule;
    TierModule *tierModule = layerModule->data.tierModule;
    StatModule *statModule = layerModule->data.statModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;

    io->timestamp = GetCurrentTimeStamp();

    /* write data, always write into tier0 */
    PageLoc writePageLoc = {0};
    ret = gcModule->intf->writeIO(gcModule, io->lba, io->length, &writePageLoc);
    ASSERT(ret == RETURN_OK);
    ret = tierModule->intf->writeIO(tierModule, io->lba, io->length, oldPageFreq, true);
    ASSERT(ret == RETURN_OK);

    /* handle disk write */
    // slow down
    if (GCSlowCondition(layerModule)) {
        MicroSleep(100);
        statModule->data.gcSlowCount++;
    }
    if (TierDownSlowCondition(layerModule)) {
        MicroSleep(100);
        statModule->data.tierSlowCount++;
    }

    // stall
    bool gcStall = false;
    while (GCStallCondition(layerModule, writePageLoc.blockId)) {
        gcStall = true;
        MicroSleep(100);
    }
    if (gcStall) {
        statModule->data.gcStallCount++;
    }

    bool tierStall = false;
    while (TierDownStallCondition(layerModule)) {
        tierStall = true;
        MicroSleep(100);
    }
    if (tierStall) {
        statModule->data.tierStallCount++;
    }

    DiskWrite(diskOperator, &writePageLoc, 1, HOST, io->timestamp);

    statModule->data.hostWriteCount++;

    /* check need GC? physical space left < water level */
    bool gc = false;
    if (GCCondition(layerModule)) {
        gc = true;
        ret = HandleGC(layerModule);
    }

    /* check need tier down?logical space occupied > water level */
    bool tierdown = false;
    if (TierDownCondition(layerModule)) {
        tierdown = true;
        ret = HandleTierDown(layerModule, pages, pageCnt);
    }

    if (gc && tierdown) {
        MY_LOG("t%d: GC_TIERDOWN", layerModule->data.layerIdx);
    } else if (gc) {
        MY_LOG("t%d: GC", layerModule->data.layerIdx);
    } else if (tierdown) {
        MY_LOG("t%d: TIERDOWN", layerModule->data.layerIdx);
    }

    return ret;
}

//-----------------------------------------------------------------------
// read io progress.
//-----------------------------------------------------------------------
static int32_t ReadHostIO(void *self, IO *io)
{
    int32_t ret = RETURN_ERROR;
    LayerModule *layerModule = (LayerModule *)self;
    GCModule *gcModule = layerModule->data.gcModule;
    TierModule *tierModule = layerModule->data.tierModule;
    StatModule *statModule = layerModule->data.statModule;

    io->timestamp = GetCurrentTimeStamp();

    PageLoc pageLoc = {0};
    ret = gcModule->intf->readIO(gcModule, io->lba, io->length, &pageLoc);
    ASSERT(ret == RETURN_OK);
    ret = tierModule->intf->readIO(tierModule, io->lba, io->length); /* update access frequency */

    statModule->data.hostReadCount++;
    DiskRead(layerModule->data.diskOp, &pageLoc, 1, HOST, io->timestamp);
    return ret;
}

static int32_t WriteTierDownIO(void *self, TierPage *pages, uint32_t pageCnt)
{
    int32_t ret = RETURN_ERROR;
    LayerModule *layerModule = (LayerModule *)self;
    GCModule *gcModule = layerModule->data.gcModule;
    TierModule *tierModule = layerModule->data.tierModule;
    StatModule *statModule = layerModule->data.statModule;
    DiskOperator *diskOperator = layerModule->data.diskOp;

    // bool flag = false;
    // if (diskOperator && diskOperator->intf->queryTierQueueSize(diskOperator) != 0) {
    //     // means last tier not finished yet, dont do this time
    //     flag = true;
    //     while (TierDownStallCondition(layerModule)) MicroSleep(100);
    // }

    // if (flag) {
    //     statModule->data.tierStallCount++;
    // }

    /* write data,always write into tier0 */
    PageLoc *pageLoc = (PageLoc *)malloc(sizeof(PageLoc) * (pageCnt));
    ASSERT(pageLoc);
    for (int32_t i = 0; i < pageCnt; i++) {
        ret = gcModule->intf->writeIO(gcModule, pages[i].lba, PAGE_SIZE, &pageLoc[i]);
        ASSERT(ret == RETURN_OK);
        ret = tierModule->intf->writeIO(tierModule, pages[i].lba, PAGE_SIZE, &pages[i].freq, false);
        ASSERT(ret == RETURN_OK);
    }

    // slow down
    bool gcSlow = GCSlowCondition(layerModule);
    if (gcSlow) {
        MicroSleep(100);
    }
    if (gcSlow) statModule->data.gcSlowCount++;

    // stall
    bool gcStall = false;
    while (GCStallCondition(layerModule, pageLoc->blockId)) {
        gcStall = true;
        MicroSleep(100);
    }
    if (gcStall) statModule->data.gcStallCount++;

    // handle disk write
    PageLoc writePageLoc = pageLoc[0];
    int32_t size = 0;
    for (int32_t idx = 0; idx < pageCnt; idx++) {
        if (writePageLoc.blockId != pageLoc[idx].blockId) {
            DiskWrite(diskOperator, &writePageLoc, size, BACK_TIER, 0);
            writePageLoc = pageLoc[idx];
            size = 0;
        }
        size++;
    }
    DiskWrite(diskOperator, &writePageLoc, size, BACK_TIER, 0);
    /* check need GC? physical space left < water level */
    if (GCCondition(layerModule)) {
        ret = HandleGC(layerModule);
    }
    return ret;
}

//-----------------------------------------------------------------------
// delete data progress.
//-----------------------------------------------------------------------
static int32_t DeleteData(void *self, uint64_t lba, uint32_t length)
{
    int32_t ret = RETURN_OK;
    LayerModule *layerModule = (LayerModule *)self;
    GCModule *gcModule = layerModule->data.gcModule;
    TierModule *tierModule = layerModule->data.tierModule;

    if (gcModule->intf->isExist(gcModule, lba)) {
        PageLoc pageLoc = {0};
        ret = gcModule->intf->delete (gcModule, lba, length, &pageLoc);
        ASSERT(ret == RETURN_OK);
        ret = tierModule->intf->delete (tierModule, lba, length);
        ASSERT(ret == RETURN_OK);
    }
    return ret;
}

//-----------------------------------------------------------------------
// check full progress.
//-----------------------------------------------------------------------
static bool IsFull(void *self)
{
    LayerModule *layerModule = (LayerModule *)self;
    GCModule *gcModule = layerModule->data.gcModule;
    return gcModule->intf->getLogicalSizeOccupied(gcModule) >= layerModule->data.tierDownWL;
}

//-----------------------------------------------------------------------
// query freq progress.
//-----------------------------------------------------------------------
static int32_t QueryPageHistoryFreq(void *self, uint64_t lba, PageFreq *pageFreq)
{
    int32_t ret = RETURN_ERROR;
    LayerModule *layerModule = (LayerModule *)self;
    TierModule *tierModule = layerModule->data.tierModule;
    ret = tierModule->intf->queryFrequency((void *)tierModule, lba, pageFreq);
    return ret;
}

//-----------------------------------------------------------------------
// query exist progress.
//-----------------------------------------------------------------------
static bool IsExist(void *self, uint64_t lba)
{
    LayerModule *layerModule = (LayerModule *)self;
    return layerModule->data.gcModule->intf->isExist(layerModule->data.gcModule, lba);
}

static LayerModuleIntf g_LayerModuleIntf = {
    .readIO = ReadHostIO,
    .writeIO = WriteHostIO,
    .writeTierDownIO = WriteTierDownIO,
    .deleteData = DeleteData,
    .isFull = IsFull,
    .queryFrequency = QueryPageHistoryFreq,
    .isExist = IsExist,
};

int32_t CreateLayerModule(LayerModule **ppLayerModule, LayerModuleConfigure *conf)
{
    LayerModule *layerModule = (LayerModule *)malloc(sizeof(LayerModule));
    if (!layerModule) {
        return RETURN_ERROR;
    }
    (void)memset(layerModule, 0, sizeof(LayerModule));
    layerModule->data.layerIdx = conf->layerIdx;

    /* initialize gc layerModule */
    int32_t ret = CreateGCModule(&layerModule->data.gcModule, &conf->gcConf);
    ASSERT(ret == RETURN_OK);
    layerModule->data.gcWL = conf->gcWL;
    layerModule->data.gcBlockCnt = conf->gcBlockCnt;
    layerModule->data.gcStallWL = conf->gcStallWL;

    /* initialize tier layerModule */
    ret = CreateTierModule(&layerModule->data.tierModule, &conf->tierConf);
    ASSERT(ret == RETURN_OK);
    layerModule->data.tierDownWL = conf->tierDownWL;
    layerModule->data.tierDownBlockCnt = conf->tierDownBlockCnt;
    layerModule->data.tierDownStallWL = conf->tierDownStallWL;

    /* initialize stat layerModule */
    ret = CreateStatModule(&layerModule->data.statModule);
    ASSERT(ret == RETURN_OK);

    //-------------------------------------
    // Create Disk Operator.
    //-------------------------------------
    if (conf->needDiskOperation) {
        DiskOperator *diskOp = NULL;
        DiskConfigure diskConf = {
            .segmentCnt = conf->gcConf.totalBlockCnt,
            .requestQueueSize = conf->requestQueueSize,
            .hostConcurrency = conf->hostConcurrency,
            .backConcurrency = conf->backConcurrency,
            .diskSymbolName = conf->diskSymbolName,
        };
        ret = CreateDiskOperator(&diskOp, &diskConf);
        ASSERT(ret == RETURN_OK);
        layerModule->data.diskOp = diskOp;
    }

    layerModule->data.storSys = conf->storSys;

    layerModule->intf = &g_LayerModuleIntf;
    *(ppLayerModule) = layerModule;
    return RETURN_OK;
}

void DestroyLayerModule(LayerModule **ppLayerModule)
{
    DestroyGCModule(&(*ppLayerModule)->data.gcModule);
    DestroyTierModule(&(*ppLayerModule)->data.tierModule);
    DestroyStatModule(&(*ppLayerModule)->data.statModule);
    if ((*ppLayerModule)->data.diskOp) {
        DestroyDiskOperator((*ppLayerModule)->data.diskOp);
    }
}
