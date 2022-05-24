
#ifndef BASE_DISK_OPERATOR_H
#define BASE_DISK_OPERATOR_H

#include <pthread.h>

#include "src/utility/blocking_queue.h"
#include "src/utility/histogram.h"
#include "src/utility/unblocking_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagDiskIOType {
    HOST = 1,
    BACK_GC = 2,
    BACK_TIER = 3,
} DiskIOType;

typedef enum tagDiskOpType {
    DISK_READ = 1,
    DISK_WRITE = 2,
    DISK_ERASE = 3,
} DiskOpType;

typedef struct tagDiskIOInfo {
    uint32_t segmentId;
    uint32_t offset;
    uint32_t size;
    DiskIOType ioType;
    DiskOpType opType;
    uint64_t timestamp;
} DiskIOInfo;

typedef struct tagDiskOperatorData {
    int fd;
    char *diskSymbolName;

    uint32_t *segmentCount;
    uint32_t freeSegmentSize;

    BlockingQueue *hostQ;
    UnblockingQueue *backGCQ;
    UnblockingQueue *backTierQ;
    bool *hostShutdown;
    bool backShutdown;
    uint32_t hostConcurrency;
    uint32_t backConcurrency;
    pthread_t *tHostIOId;
    pthread_t *tBackGCId;
    pthread_t *tBackTierId;

    // Histogram *readHist;
    Histogram **writeHists;
    int32_t slowCount;
    int32_t stallCount;
} DiskOperatorData;

typedef struct tagDiskOperatorIntf {
    void (*handleIO)(void *self, DiskIOInfo *io);
    bool (*needStall)(void *self, uint32_t segmentId);
    uint32_t (*queryDiskFreeBlock)(void *self);
    uint32_t (*queryHostQueueSize)(void *self);
    uint32_t (*queryGCQueueSize)(void *self);
    uint32_t (*queryTierQueueSize)(void *self);
} DiskOperatorIntf;

typedef struct tagDiskOperator {
    DiskOperatorData data;
    DiskOperatorIntf *intf;
} DiskOperator;

typedef struct tagDiskConfigure {
    uint32_t segmentCnt;
    uint32_t requestQueueSize;
    uint32_t hostConcurrency;
    uint32_t backConcurrency;
    char *diskSymbolName;
} DiskConfigure;

int32_t CreateDiskOperator(DiskOperator **diskOperator, DiskConfigure *conf);

int32_t DestroyDiskOperator(DiskOperator *diskOperator);

void CreateHostDiskIOThread(DiskOperator *diskOperator, int idx);

void DestroyHostDiskIOThread(DiskOperator *diskOperator, int idx);

#ifdef __cplusplus
}
#endif

#endif  // BASE_DISK_OPERAOTR_H