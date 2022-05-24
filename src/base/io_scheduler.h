
#ifndef IO_SCHEDULER_H
#define IO_SCHEDULER_H

#include <stdint.h>

#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagHotColdPredictor {
    uint32_t sizeThreshold;
    uint32_t counterThreshold;
    bool (*isHotOrCold)(uint64_t lba, uint32_t length, void *freq, uint32_t sizeThreshold, uint32_t counterThreshold);
} HotColdPredictor;

typedef struct tagIdealBypass {
    bool enable;
    bool (*canBypass)(uint64_t lba);
} IdealBypass;

typedef struct tagBypassHelper {
    bool enable;
    uint32_t iopsThreshold;
} BypassHelper;

typedef struct tagIOSchedulerData {
    void *storSys;
    IdealBypass ideal;
    HotColdPredictor predictor;
    BypassHelper helper;
} IOSchedulerData;

typedef struct tagIOSchedulerIntf {
    int32_t (*scheduleLayer)(void *self, uint64_t lba, uint32_t length, void *pageFreq);
} IOSchedulerIntf;

typedef struct tagIOScheduler {
    IOSchedulerData data;
    IOSchedulerIntf *intf;
} IOScheduler;

typedef struct tagIOSchedulerConfigure {
    void *storSys;
    bool idealEnable;
    bool (*idealBypass)(uint64_t);
    bool normalEnable;
} IOSchedulerConfigure;

int32_t CreateIOScheduler(IOScheduler **__scheduler, IOSchedulerConfigure *conf);

void DestroyIOScheduler(IOScheduler **scheduler);

#ifdef __cplusplus
}
#endif
#endif  // IO_SCHEDULER_H
