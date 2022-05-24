

#ifndef LAYER_MODULE_H
#define LAYER_MODULE_H

#include <stdint.h>

#include "src/base/disk_operator.h"
#include "src/base/gc_module.h"
#include "src/base/stat_module.h"
#include "src/base/tier_module.h"
#include "src/include/io.h"
#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagLayerModuleData {
    uint32_t layerIdx;

    GCModule *gcModule;
    uint32_t gcWL;
    uint32_t gcBlockCnt;
    uint32_t gcStallWL;

    TierModule *tierModule;
    uint32_t tierDownWL;
    uint32_t tierDownBlockCnt;
    uint32_t tierDownStallWL;

    StatModule *statModule;
    DiskOperator *diskOp;

    void *storSys;
} LayerModuleData;

typedef struct tagLayerModuleIntf {
    int32_t (*readIO)(void *self, IO *io);
    int32_t (*writeIO)(void *self, IO *io, PageFreq *oldPageFreq, TierPage **pages, uint32_t *pageCnt);
    int32_t (*writeTierDownIO)(void *self, TierPage *pages, uint32_t pageCnt);
    int32_t (*deleteData)(void *self, uint64_t lba, uint32_t length);
    bool (*isFull)(void *self);
    int32_t (*queryFrequency)(void *self, uint64_t lba, PageFreq *pageFreq);
    bool (*isExist)(void *self, uint64_t lba);
} LayerModuleIntf;

typedef struct tagLayerModule {
    LayerModuleData data;
    LayerModuleIntf *intf;
} LayerModule;

typedef struct tagLayerModuleConfigure {
    uint32_t layerIdx;
    GCModuleConfigure gcConf;
    uint32_t gcWL;
    uint32_t gcBlockCnt;
    uint32_t gcStallWL;
    TierModuleConfigure tierConf;
    uint32_t tierDownWL;
    uint32_t tierDownBlockCnt;
    uint32_t tierDownStallWL;
    bool needDiskOperation;
    uint32_t requestQueueSize;
    uint32_t hostConcurrency;
    uint32_t backConcurrency;
    char *diskSymbolName;

    void *storSys;
} LayerModuleConfigure;

int32_t CreateLayerModule(LayerModule **ppLayerModule, LayerModuleConfigure *conf);

void DestroyLayerModule(LayerModule **ppLayerModule);

#ifdef __cplusplus
}
#endif

#endif  // LAYER_MODULE_H
