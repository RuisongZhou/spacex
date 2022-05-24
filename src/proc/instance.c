
#include "src/proc/instance.h"

#include <math.h>

#include "src/include/conf.h"
#include "src/include/io.h"
#include "src/utility/blocking_queue.h"
#include "src/utility/time.h"

// storage system
int FLAGS_t0BlockCount = 100;
int FLAGS_t1BlockCount = 1000;
int FLAGS_blockCount = 0;
bool FLAGS_enableByPass = false;
bool FLAGS_enableFlush = false;
int FLAGS_gcWaterLevel = 10;
int FLAGS_tierdownWaterLevel = 0;
int FLGAS_tierdownLimitOffset = 5;
int FLAGS_gcBlockRatio = 1;
int FLAGS_tierdownBlockRatio = 1;
int FLAGS_requestQSize = 32;
int FLAGS_hostConcurrency = 1;
int FLAGS_backConcurrency = 1;
char *FLAGS_t0DiskSymbolName = "/dev/sdc";
char *FLAGS_t1DiskSymbolName = "/dev/sda";

// workload
int FLAGS_IOCount = 0;
int FLAGS_dataRatio = 0;
int FLAGS_workloadInterval = 1;
int FLAGS_workloadQSize = 128;
int FLAGS_IOPS = 0;

// instance
bool FLAGS_sysShutdown = false;

static pthread_t FLAGS_storTid;
static pthread_t FLAGS_seqTid;
static pthread_t FLAGS_zipfTid;
static pthread_t FLAGS_rndTid;
static BlockingQueue *FLAGS_queue = NULL;
static StorageSystem *FLAGS_storSys = NULL;
static ZipfGenerator *FLAGS_zipfGen = NULL;
static SequentialGenerator *FLAGS_seqGen = NULL;
static RandomGenerator *FLAGS_rndGen = NULL;

void ReportIOPS()
{
    // printf("%d\n", FLAGS_IOPS);
    FLAGS_IOPS = 0;
}

void Initialize()
{
    BlockingQueueConfigure conf = {
        .queueSize = FLAGS_workloadQSize,
        .itemSize = sizeof(IO),
    };
    CreateBlockingQueue((void **)&FLAGS_queue, &conf);

    InitTimer(1, ReportIOPS);
}

void *StorageSystemInstance(void *param)
{
    StorageSystem *storSys = (StorageSystem *)param;

    uint32_t count = 0;
    while (!FLAGS_sysShutdown) {
        IO io = {0};
        FLAGS_queue->intf->pop(FLAGS_queue, &io);
        // io.timestamp = GetCurrentTimeStamp();
        if (io.op == READ) {
            storSys->intf->readIO(storSys, &io);
        } else if (io.op == WRITE) {
            storSys->intf->writeIO(storSys, &io);
        }
        count++;
        if (count / IO_TIMES) {
            storSys->intf->report(storSys);
            count = 0;
        }
    }

    return NULL;
}

void CreateStorageSystemInstance(char *reportFileName)
{
    // tier0
    LayerModuleConfigure layer0Conf = {
        .layerIdx = 0,
        .gcConf =
            {
                .totalBlockCnt = FLAGS_t0BlockCount,
                .indexBucketCnt = FLAGS_t0BlockCount * 10,
            },
        .gcWL = FLAGS_t0BlockCount * FLAGS_gcWaterLevel / 100,
        .gcBlockCnt = fmax(1, FLAGS_t0BlockCount * FLAGS_gcBlockRatio / 100),
        .gcStallWL = FLAGS_t0BlockCount * ceil(FLAGS_gcWaterLevel / 2) / 100,
        .tierConf =
            {
                .indexBucketNum = FLAGS_t0BlockCount * 10,
            },
        .tierDownWL = FLAGS_t0BlockCount * PAGE_NUM_PER_BLOCK * FLAGS_tierdownWaterLevel / 100,
        .tierDownBlockCnt = fmax(1, FLAGS_t0BlockCount * FLAGS_tierdownBlockRatio / 100),  // 1%
        .tierDownStallWL =
            FLAGS_t0BlockCount * PAGE_NUM_PER_BLOCK * (FLAGS_tierdownWaterLevel + FLGAS_tierdownLimitOffset) / 100,
        .needDiskOperation = FLAGS_enableFlush,
        .requestQueueSize = FLAGS_requestQSize,
        .hostConcurrency = FLAGS_hostConcurrency,
        .backConcurrency = FLAGS_backConcurrency,
        .diskSymbolName = FLAGS_t0DiskSymbolName,
    };

    // offset * T0 / T1
    int32_t reservedSpaceRatio = ceil(1.0 * FLGAS_tierdownLimitOffset * FLAGS_t0BlockCount / FLAGS_t1BlockCount);
    LayerModuleConfigure layer1Conf = {
        .layerIdx = 1,
        .gcConf =
            {
                .totalBlockCnt = FLAGS_t1BlockCount,
                .indexBucketCnt = FLAGS_t1BlockCount * 10,
            },
        // gc 水位 +
        .gcWL = FLAGS_t1BlockCount * (FLAGS_gcWaterLevel + reservedSpaceRatio) / 100,
        .gcStallWL = FLAGS_t1BlockCount * (ceil(FLAGS_gcWaterLevel / 2) + reservedSpaceRatio) / 100,
        .gcBlockCnt = fmax(1, FLAGS_t1BlockCount * FLAGS_gcBlockRatio / 100),
        .tierConf =
            {
                .indexBucketNum = FLAGS_t1BlockCount * 10,
            },
        .tierDownWL = FLAGS_t1BlockCount * PAGE_NUM_PER_BLOCK,
        .tierDownBlockCnt = 0,
        .tierDownStallWL = FLAGS_t1BlockCount * PAGE_NUM_PER_BLOCK,
        .needDiskOperation = FLAGS_enableFlush,
        .requestQueueSize = FLAGS_requestQSize,
        .hostConcurrency = FLAGS_hostConcurrency,
        .backConcurrency = FLAGS_backConcurrency,
        .diskSymbolName = FLAGS_t1DiskSymbolName,
    };

    IOSchedulerConfigure ioScheduler = {
        .idealEnable = false,
        .idealBypass = NULL,
        .normalEnable = FLAGS_enableByPass,
    };
    StorageSystemConfigure conf = {
        .layerConf = {layer0Conf, layer1Conf}, .schedulerConf = ioScheduler, .reportFileName = reportFileName};
    int ret = CreateStorageSystem(&FLAGS_storSys, &conf);
    assert(ret == RETURN_OK);

    pthread_create(&FLAGS_storTid, NULL, StorageSystemInstance, (void *)FLAGS_storSys);
}

void PrintWriteHistgram() { FLAGS_storSys->intf->printWriteHistgram(FLAGS_storSys); }

void WaitForStorageSystemInstance()
{
    while (FLAGS_queue->intf->querySize(FLAGS_queue) > 0) {
        MicroSleep(100);
    }
    FLAGS_sysShutdown = true;
    DestroyBlockingQueue(FLAGS_queue);
    pthread_join(FLAGS_storTid, NULL);
    DestroyStorageSystem(&FLAGS_storSys);
}

void *ZipfWorkloadInstance(void *param)
{
    ZipfGenerator *zipfGen = (ZipfGenerator *)param;
    for (uint64_t i = 0; i < zipfGen->data.maxIOCount; i++) {
        FLAGS_IOPS++;
        IO io = {0};
        zipfGen->intf->generateReadWriteIO(zipfGen, &io);
        FLAGS_queue->intf->push(FLAGS_queue, &io);

        // NanoSleep(FLAGS_workloadInterval);
        // SelectSleep(FLAGS_workloadInterval);
    }
    return NULL;
}

void CreateZipfGeneratorInstance(ZipfGeneratorConfigure *conf)
{
    int32_t ret = CreateZipfGenerator(&FLAGS_zipfGen, conf);
    assert(ret == RETURN_OK);

    pthread_create(&FLAGS_zipfTid, NULL, ZipfWorkloadInstance, (void *)FLAGS_zipfGen);
}

void WaitForZipfGeneratorInstance()
{
    pthread_join(FLAGS_zipfTid, NULL);
    DestroyZipfGenerator(&FLAGS_zipfGen);
}

void *SequentialWorkloadInstance(void *param)
{
    SequentialGenerator *seqGen = (SequentialGenerator *)param;
    for (uint64_t i = 0; i < seqGen->data.maxLbaCount; i++) {
        FLAGS_IOPS++;
        IO io = {0};
        seqGen->intf->generateWarmupIO(seqGen, &io);
        FLAGS_queue->intf->push(FLAGS_queue, &io);

        // NanoSleep(FLAGS_workloadInterval);
        // SelectSleep(FLAGS_workloadInterval);
    }
    return NULL;
}

void CreateSequentialGeneratorInstance(SequentialGeneratorConfigure *conf)
{
    int32_t ret = CreateSequentialGenerator(&FLAGS_seqGen, conf);
    assert(ret == RETURN_OK);
    pthread_create(&FLAGS_seqTid, NULL, SequentialWorkloadInstance, (void *)FLAGS_seqGen);
}

void WaitForSequentialGeneratorInstance()
{
    pthread_join(FLAGS_seqTid, NULL);
    DestroySequentialGenrator(&FLAGS_seqGen);
}

void *RandomWorkloadInstance(void *param)
{
    RandomGenerator *rndGen = (RandomGenerator *)param;
    for (uint64_t i = 0; i < rndGen->data.maxIOCount; i++) {
        FLAGS_IOPS++;
        IO io = {0};
        rndGen->intf->generateReadWriteIO(rndGen, &io);
        FLAGS_queue->intf->push(FLAGS_queue, &io);

        // NanoSleep(FLAGS_workloadInterval);
        // SelectSleep(FLAGS_workloadInterval);
    }
    return NULL;
}

void CreateRandomGeneratorInstance(RandomGeneratorConfigure *conf)
{
    int32_t ret = CreateRandomGenerator(&FLAGS_rndGen, conf);
    assert(ret == RETURN_OK);
    pthread_create(&FLAGS_rndTid, NULL, RandomWorkloadInstance, (void *)FLAGS_rndGen);
}

void WaitForRandomGeneratorInstance()
{
    pthread_join(FLAGS_rndTid, NULL);
    DestroyRandomGenrator(&FLAGS_rndGen);
}
