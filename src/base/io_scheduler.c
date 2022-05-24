#include "src/base/io_scheduler.h"

#include "src/base/disk_operator.h"
#include "src/base/storage_system.h"
#include "src/base/tier_module.h"
#include "src/utility/infracture.h"

extern uint64_t g_IOPS;

// check lba hot or cold
static bool IsHotOrCold(uint64_t lba, uint32_t length, void *freq, uint32_t sizeThreshold, uint32_t counterThreshold)
{
    if (length > sizeThreshold) {
        return false;
    } else {
        return true;
    }
}

static inline bool IsTierServiceAvaliable(DiskOperator *t1DiskOp)
{
    if (t1DiskOp->intf->queryGCQueueSize(t1DiskOp) > 0 || t1DiskOp->intf->queryTierQueueSize(t1DiskOp) > 0
        || t1DiskOp->intf->queryHostQueueSize(t1DiskOp) > 0) {
        return false;
    } else {
        return true;
    }
}

static inline bool IsTierServiceOveruse(DiskOperator *t0DiskOp)
{
    if (t0DiskOp->intf->queryHostQueueSize(t0DiskOp) < 32) {
        return false;
    } else {
        return true;
    }
}
int32_t ScheduleLayer(void *self, uint64_t lba, uint32_t length, void *pageFreq)
{
    IOScheduler *scheduler = (IOScheduler *)self;
    StorageSystem *storSys = (StorageSystem *)scheduler->data.storSys;
    // 理想化的bypass
    if (scheduler->data.ideal.enable) {
        return (scheduler->data.ideal.canBypass(lba) && !storSys->data.tiers[1]->intf->isFull(storSys->data.tiers[1]))
                   ? 1
                   : 0;
    }

    // 不开启bypass
    if (!scheduler->data.helper.enable) {
        return 0;
    }

    // temperature of workload
    bool isHot = scheduler->data.predictor.isHotOrCold(lba, length, pageFreq, scheduler->data.predictor.sizeThreshold,
                                                       scheduler->data.predictor.counterThreshold);

    // cold data and t1 not full, redirect to t1
    if (!isHot && !storSys->data.tiers[1]->intf->isFull(storSys->data.tiers[1])) return 1;

    LayerModule *t0 = storSys->data.tiers[0];
    LayerModule *t1 = storSys->data.tiers[1];

    // overused
    if (g_IOPS > scheduler->data.helper.iopsThreshold && IsTierServiceAvaliable(t1->data.diskOp)) {
        return 1;
    }
    // if (&&t1->data.diskOp->intf->queryHostQueueSize(t1->data.diskOp) < 1) {
    //     return 1;
    // }
    return 0;
}

static IOSchedulerIntf g_schedulerIntf = {
    .scheduleLayer = ScheduleLayer,
};

int32_t CreateIOScheduler(IOScheduler **__scheduler, IOSchedulerConfigure *conf)
{
    IOScheduler *scheduler = (IOScheduler *)malloc(sizeof(IOScheduler));
    ASSERT(scheduler);
    scheduler->data.storSys = conf->storSys;
    scheduler->data.ideal.enable = conf->idealEnable;
    scheduler->data.ideal.canBypass = conf->idealBypass;
    scheduler->data.helper.enable = conf->normalEnable;
    scheduler->data.helper.iopsThreshold = 200000;
    scheduler->data.predictor.isHotOrCold = IsHotOrCold;
    scheduler->intf = &g_schedulerIntf;
    *__scheduler = scheduler;
    return RETURN_OK;
}

void DestroyIOScheduler(IOScheduler **scheduler)
{
    free(*scheduler);
    *scheduler = NULL;
}
