

#ifndef STORAGE_MODULE_H
#define STORAGE_MODULE_H

#include <pthread.h>
#include <stdint.h>

#include "src/base/io_scheduler.h"
#include "src/base/layer_module.h"
#include "src/include/io.h"
#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIER_NUM 2

typedef struct tagTierThreadData {
    pthread_t tid;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
} TierThreadData;

typedef struct tagStorageSystemData {
    LayerModule *tiers[TIER_NUM];
    IOScheduler *ioScheduler;
    Histogram *writeHist;
    FILE *reportFD;

    bool running;
    TierThreadData tierDownThread;
} StorageSystemData;

typedef struct tagStorageSystemIntf {
    int32_t (*readIO)(void *self, IO *io);
    int32_t (*writeIO)(void *self, IO *io);
    void (*report)(void *self);
    void (*printWriteHistgram)(void *self);
} StorageSystemIntf;

typedef struct tagStorageSystem {
    StorageSystemData data;
    StorageSystemIntf *intf;
} StorageSystem;

typedef struct tagStorageSystemConfigure {
    LayerModuleConfigure layerConf[TIER_NUM];
    IOSchedulerConfigure schedulerConf;
    char *reportFileName;
} StorageSystemConfigure;

int32_t CreateStorageSystem(StorageSystem **ppStorageSystem, StorageSystemConfigure *conf);

void DestroyStorageSystem(StorageSystem **ppStorageSystem);

#ifdef __cplusplus
}
#endif

#endif  // STORAGE_MODULE_H
