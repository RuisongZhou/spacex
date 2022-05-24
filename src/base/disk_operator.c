#include "disk_operator.h"

#include <string.h>

#include "src/include/conf.h"
#include "src/third_party/c-logger/src/logger.h"
#include "src/utility/io_request.h"
#include "src/utility/time.h"

uint32_t g_IOPS = 0;

static uint64_t DiskWrite(void* self, uint32_t segmentId, uint32_t offset, uint32_t size)
{
    DiskOperator* diskOperator = (DiskOperator*)self;
    // uint32_t expected = PAGE_NUM_PER_BLOCK - size;
    // if (__atomic_compare_exchange_n(&diskOperator->data.segmentCount[segmentId], &expected, PAGE_NUM_PER_BLOCK,
    // false,
    //                                 __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
    //     __atomic_fetch_sub(&diskOperator->data.freeSegmentSize, 1, __ATOMIC_ACQUIRE);
    // } else {
    //     __atomic_fetch_add(&diskOperator->data.segmentCount[segmentId], size, __ATOMIC_ACQUIRE);
    // }

    uint64_t segOffset = (uint64_t)segmentId * BLOCK_SIZE + offset * PAGE_SIZE;
    uint64_t writeLatency = SendWriteRequest(diskOperator->data.fd, segOffset, size * PAGE_SIZE);
    return writeLatency;
}

static uint64_t DiskRead(void* self, uint32_t segmentId, uint32_t offset, uint32_t size)
{
    DiskOperator* diskOperator = (DiskOperator*)self;
    uint64_t segOffset = (uint64_t)segmentId * BLOCK_SIZE + offset * PAGE_SIZE;
    uint64_t writeLatency = SendReadRequest(diskOperator->data.fd, segOffset, size * PAGE_SIZE);
    return writeLatency;
}

static int32_t DiskErase(void* self, uint32_t segmentId)
{
    DiskOperator* diskOperator = (DiskOperator*)self;
    // if (__atomic_load_n(&diskOperator->data.segmentCount[segmentId], __ATOMIC_ACQUIRE) < PAGE_NUM_PER_BLOCK) {
    //     int a = 0;
    // }
    // while (__atomic_load_n(&diskOperator->data.segmentCount[segmentId], __ATOMIC_ACQUIRE) < PAGE_NUM_PER_BLOCK) {
    //     MicroSleep(100);
    //     if (diskOperator->data.backShutdown) break;
    // }
    // __atomic_store_n(&diskOperator->data.segmentCount[segmentId], 0, __ATOMIC_RELEASE);
    // __atomic_fetch_add(&diskOperator->data.freeSegmentSize, 1, __ATOMIC_ACQUIRE);
    return RETURN_OK;
}

static inline bool CheckNeedSlowdown(bool needSlowdown)
{
    if (needSlowdown) {
        MicroSleep(100);
        return true;
    }
    return false;
}

static inline bool CheckNeedStall(DiskOperator* diskOperator, DiskIOInfo* io)
{
    bool ret = false;
    // while (__atomic_load_n(&diskOperator->data.segmentCount[io->segmentId], __ATOMIC_ACQUIRE) >= PAGE_NUM_PER_BLOCK)
    // {
    //     ret = true;
    //     MicroSleep(100);
    //     if (diskOperator->data.backShutdown) break;
    // }
    return ret;
}

void* BackGCThread(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    while (!diskOp->data.backShutdown) {
        DiskIOInfo io = {0};
        diskOp->data.backGCQ->intf->pop(diskOp->data.backGCQ, &io);
        switch (io.opType) {
            case DISK_READ:
                DiskRead(diskOp, io.segmentId, io.offset, io.size);
                // MY_LOG("%s GC_read", diskOp->data.diskSymbolName);
                break;
            case DISK_WRITE:
                // need stall
                // CheckNeedStall(diskOp, &io);
                DiskWrite(diskOp, io.segmentId, io.offset, io.size);
                // MY_LOG("%s GC_write: %d", diskOp->data.diskSymbolName, io.size);
                break;
            case DISK_ERASE:
                DiskErase(diskOp, io.segmentId);
                break;
            default:
                break;
        }
    }
    return NULL;
}

void* BackTierThread(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    while (!diskOp->data.backShutdown) {
        DiskIOInfo io = {0};
        diskOp->data.backTierQ->intf->pop(diskOp->data.backTierQ, &io);
        switch (io.opType) {
            case DISK_WRITE:
                // need stall
                // CheckNeedStall(diskOp, &io);
                DiskWrite(diskOp, io.segmentId, io.offset, io.size);
                // MY_LOG("%s TIER_write: %d %lu", diskOp->data.diskSymbolName, io.size);
                break;
            case DISK_READ:
                DiskRead(diskOp, io.segmentId, io.offset, io.size);
                // MY_LOG("%s TIER_read: %lu", diskOp->data.diskSymbolName);
                break;
            default:
                break;
        }
    }
    return NULL;
}

typedef struct tagHostIOThreadParam {
    DiskOperator* diskOp;
    int32_t idx;
} HostIOThreadParam;

void* HostIOThread(void* threadParam)
{
    HostIOThreadParam* param = (HostIOThreadParam*)threadParam;
    DiskOperator* diskOp = param->diskOp;
    int32_t idx = param->idx;
    free(threadParam);

    Histogram* hist = diskOp->data.writeHists[idx];

    while (!diskOp->data.hostShutdown[idx]) {
        DiskIOInfo io = {0};
        diskOp->data.hostQ->intf->pop(diskOp->data.hostQ, &io);
        uint64_t latency = 0;
        switch (io.opType) {
            case DISK_WRITE:
                DiskWrite(diskOp, io.segmentId, io.offset, io.size);
                latency = GetCurrentTimeStamp() - io.timestamp;
                MY_LOG("%s: %lu", diskOp->data.diskSymbolName, latency);
                hist->intf->add(hist, latency);
                break;
            case DISK_READ:
                DiskRead(diskOp, io.segmentId, io.offset, io.size);
                latency = GetCurrentTimeStamp() - io.timestamp;
                MY_LOG("%s: %lu", diskOp->data.diskSymbolName, latency);
                break;
            default:
                break;
        }
    }
    return NULL;
}

static void HandleDiskIO(void* self, DiskIOInfo* io)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    g_IOPS++;
    if (io->ioType == HOST) {
        diskOp->data.hostQ->intf->push(diskOp->data.hostQ, io);
    } else if (io->ioType == BACK_GC) {
        diskOp->data.backGCQ->intf->push(diskOp->data.backGCQ, io);
    } else if (io->ioType == BACK_TIER) {
        diskOp->data.backTierQ->intf->push(diskOp->data.backTierQ, io);
    }
}

// no other space can be written
static bool NeedStall(void* self, uint32_t segmentId)
{
    DiskOperator* diskOperator = (DiskOperator*)self;
    // return __atomic_load_n(&diskOperator->data.segmentCount[segmentId], __ATOMIC_ACQUIRE) >= PAGE_NUM_PER_BLOCK;
    return false;
}

static uint32_t QueryDiskFreeBlock(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    return __atomic_load_n(&diskOp->data.freeSegmentSize, __ATOMIC_ACQUIRE);
}

static uint32_t QueryTierQueueSize(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    return diskOp->data.backTierQ->intf->querySize(diskOp->data.backTierQ);
}

static uint32_t QueryGCQueueSize(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    return diskOp->data.backGCQ->intf->querySize(diskOp->data.backGCQ);
}

static uint32_t QueryHostQueueSize(void* self)
{
    DiskOperator* diskOp = (DiskOperator*)self;
    return diskOp->data.hostQ->intf->querySize(diskOp->data.hostQ);
}

static DiskOperatorIntf g_diskOpIntf = {
    .handleIO = HandleDiskIO,
    .needStall = NeedStall,
    .queryDiskFreeBlock = QueryDiskFreeBlock,
    .queryHostQueueSize = QueryHostQueueSize,
    .queryGCQueueSize = QueryGCQueueSize,
    .queryTierQueueSize = QueryTierQueueSize,
};

void CreateHostDiskIOThread(DiskOperator* diskOperator, int idx)
{
    diskOperator->data.hostShutdown[idx] = false;
    HostIOThreadParam* param = (HostIOThreadParam*)malloc(sizeof(HostIOThreadParam));
    param->diskOp = diskOperator;
    param->idx = idx;
    pthread_create(&diskOperator->data.tHostIOId[idx], NULL, HostIOThread, param);
}

void DestroyHostDiskIOThread(DiskOperator* diskOperator, int idx)
{
    diskOperator->data.hostShutdown[idx] = true;
    pthread_cond_broadcast(&diskOperator->data.hostQ->data.popCv);
    pthread_join(diskOperator->data.tHostIOId[idx], NULL);
}

int32_t CreateDiskOperator(DiskOperator** __diskOperator, DiskConfigure* conf)
{
    int32_t ret = RETURN_ERROR;
    DiskOperator* diskOperator = (DiskOperator*)malloc(sizeof(DiskOperator));
    if (!diskOperator) {
        return RETURN_ERROR;
    }
    (void)memset(diskOperator, 0, sizeof(DiskOperator));
    diskOperator->data.segmentCount = (uint32_t*)malloc(sizeof(uint32_t) * conf->segmentCnt);
    ASSERT(diskOperator->data.segmentCount);
    (void)memset(diskOperator->data.segmentCount, 0, sizeof(uint32_t) * conf->segmentCnt);
    diskOperator->data.freeSegmentSize = conf->segmentCnt;

    //-------------------------------------
    // Create fd.
    //-------------------------------------
    diskOperator->data.diskSymbolName = conf->diskSymbolName;
    diskOperator->data.fd = OpenFD(conf->diskSymbolName);
    ASSERT(diskOperator->data.fd);

    // create hist
    diskOperator->data.writeHists = (Histogram**)malloc(sizeof(Histogram*) * conf->hostConcurrency);
    ASSERT(diskOperator);
    for (uint32_t i = 0; i < conf->hostConcurrency; i++) {
        ret = CreateHistogram(&diskOperator->data.writeHists[i]);
        ASSERT(ret == RETURN_OK);
    }

    // create threads
    BlockingQueueConfigure hostQConf = {
        .queueSize = conf->requestQueueSize,
        .itemSize = sizeof(DiskIOInfo),
    };
    CreateBlockingQueue((void**)&diskOperator->data.hostQ, &hostQConf);
    UnblockingQueueConfigure backQConf = {
        .itemSize = sizeof(DiskIOInfo),
    };
    CreateUnblockingQueue((void**)&diskOperator->data.backGCQ, &backQConf);
    CreateUnblockingQueue((void**)&diskOperator->data.backTierQ, &backQConf);
    diskOperator->data.backShutdown = false;
    diskOperator->data.hostConcurrency = conf->hostConcurrency;
    diskOperator->data.backConcurrency = conf->backConcurrency;
    diskOperator->data.tHostIOId = (pthread_t*)malloc(sizeof(pthread_t) * conf->hostConcurrency);
    ASSERT(diskOperator->data.tHostIOId);
    diskOperator->data.tBackGCId = (pthread_t*)malloc(sizeof(pthread_t) * conf->backConcurrency);
    ASSERT(diskOperator->data.tBackGCId);
    diskOperator->data.tBackTierId = (pthread_t*)malloc(sizeof(pthread_t) * conf->backConcurrency);
    ASSERT(diskOperator->data.tBackTierId);
    diskOperator->data.hostShutdown = (bool*)malloc(sizeof(bool) * conf->hostConcurrency);
    ASSERT(diskOperator->data.hostShutdown);
    for (uint32_t i = 0; i < conf->hostConcurrency; i++) {
        CreateHostDiskIOThread(diskOperator, i);
    }

    for (uint32_t i = 0; i < conf->backConcurrency; i++) {
        pthread_create(&diskOperator->data.tBackGCId[i], NULL, BackGCThread, diskOperator);
        pthread_create(&diskOperator->data.tBackTierId[i], NULL, BackTierThread, diskOperator);
    }
    diskOperator->intf = &g_diskOpIntf;
    (*__diskOperator) = diskOperator;
    return ret;
}

int32_t DestroyDiskOperator(DiskOperator* diskOperator)
{
    diskOperator->data.backShutdown = true;

    for (uint32_t i = 0; i < diskOperator->data.hostConcurrency; i++) {
        diskOperator->data.hostShutdown[i] = true;
    }

    DestroyBlockingQueue(diskOperator->data.hostQ);
    DestroyUnblockingQueue(diskOperator->data.backGCQ);
    DestroyUnblockingQueue(diskOperator->data.backTierQ);

    for (uint32_t i = 0; i < diskOperator->data.hostConcurrency; i++) {
        if (diskOperator->data.tHostIOId[i] != 0) {
            pthread_join(diskOperator->data.tHostIOId[i], NULL);
        }
    }

    for (uint32_t i = 0; i < diskOperator->data.backConcurrency; i++) {
        pthread_join(diskOperator->data.tBackGCId[i], NULL);
        pthread_join(diskOperator->data.tBackTierId[i], NULL);
    }

    free(diskOperator->data.tHostIOId);
    free(diskOperator->data.tBackGCId);
    free(diskOperator->data.tBackTierId);
    free(diskOperator->data.writeHists);
    free(diskOperator);
    diskOperator = NULL;
    return RETURN_OK;
}
