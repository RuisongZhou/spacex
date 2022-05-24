#ifndef TIER_MODULE_H
#define TIER_MODULE_H

#include "src/utility/infracture.h"
#include "src/utility/lfu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagPageFreq {
    uint32_t readFreq;
    uint32_t writeFreq;
} PageFreq;

typedef struct tagTierPage {
    uint64_t lba;
    PageFreq freq;
} TierPage;

typedef struct tagTierModuleData {
    Indexer* pageFreqIndexer; /* logic address to block id mapper. */
    LFU* lfu;                 /* frequency recorder */
} TierModuleData;

typedef struct tagTierModuleIntf {
    int32_t (*readIO)(void* self, uint64_t lba, uint32_t length);
    int32_t (*writeIO)(void* self, uint64_t lba, uint32_t length, PageFreq* oldPageFreq, bool isHost);
    int32_t (*tierDown)(void* self, TierPage* pages, uint32_t pageCnt);
    int32_t (*delete)(void* self, uint64_t lba, uint32_t length);
    int32_t (*queryFrequency)(void* self, uint64_t lba, PageFreq* pageFreq);
} TierModuleIntf;

typedef struct tagTierModule {
    TierModuleData data;
    TierModuleIntf* intf;
} TierModule;

typedef struct tagTierModuleConfigure {
    uint32_t indexBucketNum;
} TierModuleConfigure;

int32_t CreateTierModule(TierModule** __tierModule, TierModuleConfigure* conf);

void DestroyTierModule(TierModule** tierModule);

#ifdef __cplusplus
}
#endif
#endif  // TIER_MODULE_H