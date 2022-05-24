#ifndef SPACE_X_H
#define SPACE_X_H

#include <pthread.h>
#include <stdint.h>

#include "src/base/io_scheduler.h"
#include "src/base/layer_module.h"
#include "src/include/io.h"
#include "src/utility/infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagSpaceXData {
    void *storSys;
    int32_t lastLatency;
    double lastFreqRatio;
    uint32_t lastGCCount;
    uint32_t lastTierCount;

} SpaceXData;

typedef struct tagSpaceXIntf {
} SpaceXIntf;

typedef struct tagSpaceX {
    SpaceXData data;
    SpaceXIntf *intf;
} SpaceX;

typedef struct tagSpaceXConfigure {
    void *storSys;
} SpaceXConfigure;

int32_t CreateSpaceX(SpaceX **ppSpaceX, SpaceXConfigure *conf);

void DestroySpaceX(SpaceX **ppSpaceX);

#ifdef __cplusplus
}
#endif
#endif  // SPACE_X_H