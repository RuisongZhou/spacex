#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "src/include/conf.h"
#include "src/include/workload/sequential_generator.h"
#include "src/include/workload/zipf_generator.h"
#include "src/proc/instance.h"
#include "src/third_party/c-logger/src/logger.h"
#include "src/utility/infracture.h"

#define MAX_BUFF_SIZE 260

extern int FLAGS_t0BlockCount;
extern int FLAGS_t1BlockCount;
extern int FLAGS_blockCount;
extern bool FLAGS_enableByPass;
extern bool FLAGS_enableFlush;
extern int FLAGS_gcWaterLevel;
extern int FLAGS_tierdownWaterLevel;
extern int FLAGS_gcBlockRatio;
extern int FLAGS_tierdownBlockRatio;
extern int FLAGS_requestQSize;
extern int FLAGS_hostConcurrency;
extern int FLAGS_backConcurrency;
extern char* FLAGS_t0DiskSymbolName;
extern char* FLAGS_t1DiskSymbolName;

extern int FLAGS_IOCount;
extern int FLAGS_dataRatio;
extern int FLAGS_workloadInterval;
extern int FLAGS_log;
char* FLAGS_workload;
static double FLAGS_theta;

void RunExp(ZipfGeneratorConfigure* zipfConf, SequentialGeneratorConfigure* seqConf, char* reportFileName)
{
    Initialize();

    CreateStorageSystemInstance(reportFileName);
    CreateSequentialGeneratorInstance(seqConf);
    WaitForSequentialGeneratorInstance();
    sleep(10);
    PrintWriteHistgram();
    sleep(10);

    CreateZipfGeneratorInstance(zipfConf);
    WaitForZipfGeneratorInstance();
    sleep(10);
    PrintWriteHistgram();
    sleep(10);

    WaitForStorageSystemInstance();
}

int32_t TEST()
{
    char buf[MAX_BUFF_SIZE] = {0};
    char* rootDir = NULL;
    rootDir = "..";
    // log file
    // printf("%lf\n", FLAGS_theta);
    int32_t ret = snprintf(buf, MAX_BUFF_SIZE, "%s/%s/log/%d_%d_%lf.log", rootDir, FLAGS_workload,
                           FLAGS_tierdownWaterLevel, FLAGS_dataRatio, FLAGS_theta);
    ASSERT(ret >= 0);
    logger_initFileLogger(buf, 500 * 1024 * 1024, 10);

    ZipfGeneratorConfigure zipfConf = {
        .maxBlockCount = FLAGS_dataRatio * FLAGS_blockCount / 100,
        .readLbaOffsetRatio = 0,
        .readWriteRatio = 0,
        .swapEndRatio = 0,
        .swapMidRatio = 0,
        .swapStartRatio = 0,
        .transferOffsetRatio = 0,
        .writeLbaOffsetRatio = 0,
        .readTheta = FLAGS_theta,
        .writeTheta = FLAGS_theta,
        .maxIOCount = FLAGS_IOCount,
    };
    SequentialGeneratorConfigure seqConf = {
        .maxBlockCount = FLAGS_dataRatio * FLAGS_blockCount / 100,
    };

    // result file path
    ret = snprintf(buf, MAX_BUFF_SIZE, "%s/%s/result/%d_%d_%lf.csv", rootDir, FLAGS_workload, FLAGS_tierdownWaterLevel,
                   FLAGS_dataRatio, FLAGS_theta);
    ASSERT(ret >= 0);

    RunExp(&zipfConf, &seqConf, buf);
    return 0;
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        double d;
        int n;
        char junk;
        if (sscanf(argv[i], "--t0_block_count=%d%c", &n, &junk) == 1) {
            FLAGS_t0BlockCount = n;
        } else if (sscanf(argv[i], "--t1_block_count=%d%c", &n, &junk) == 1) {
            FLAGS_t1BlockCount = n;
        } else if (sscanf(argv[i], "--enable_bypass=%d%c", &n, &junk) == 1) {
            FLAGS_enableByPass = n;
        } else if (sscanf(argv[i], "--enable_flush=%d%c", &n, &junk) == 1) {
            FLAGS_enableFlush = n;
        } else if (sscanf(argv[i], "--gc_water_level=%d%c", &n, &junk) == 1) {
            FLAGS_gcWaterLevel = n;
        } else if (sscanf(argv[i], "--tierdown_water_level=%d%c", &n, &junk) == 1) {
            FLAGS_tierdownWaterLevel = n;
        } else if (sscanf(argv[i], "--gc_block_ratio=%d%c", &n, &junk) == 1) {
            FLAGS_gcBlockRatio = n;
        } else if (sscanf(argv[i], "--tierdown_block_ratio=%d%c", &n, &junk) == 1) {
            FLAGS_tierdownBlockRatio = n;
        } else if (sscanf(argv[i], "--host_queue_size=%d%c", &n, &junk) == 1) {
            FLAGS_requestQSize = n;
        } else if (sscanf(argv[i], "--host_threads=%d%c", &n, &junk) == 1) {
            FLAGS_hostConcurrency = n;
        } else if (sscanf(argv[i], "--back_threads=%d%c", &n, &junk) == 1) {
            FLAGS_backConcurrency = n;
        } else if (strncmp(argv[i], "--t0=", 5) == 0) {
            FLAGS_t0DiskSymbolName = argv[i] + 5;
        } else if (strncmp(argv[i], "--t1=", 5) == 0) {
            FLAGS_t1DiskSymbolName = argv[i] + 5;
        } else if (strncmp(argv[i], "--workload_type=", 16) == 0) {
            FLAGS_workload = argv[i] + 16;
        } else if (sscanf(argv[i], "--zipf_theta=%lf%c", &d, &junk) == 1) {
            FLAGS_theta = d;
        } else if (sscanf(argv[i], "--data_ratio=%d%c", &n, &junk) == 1) {
            FLAGS_dataRatio = n;
        } else if (sscanf(argv[i], "--workload_interval=%d%c", &n, &junk) == 1) {
            FLAGS_workloadInterval = n;
        } else if (sscanf(argv[i], "--log=%d%c", &n, &junk) == 1) {
            FLAGS_log = n;
        } else {
            fprintf(stderr, "Invalid flag %s\n ", argv[i]);
            exit(1);
        }
    }

    FLAGS_blockCount = FLAGS_t0BlockCount + FLAGS_t1BlockCount;
    FLAGS_IOCount = 5 * FLAGS_blockCount * PAGE_NUM_PER_BLOCK;

    if (FLAGS_t0DiskSymbolName) {
        printf("%s\n ", FLAGS_t0DiskSymbolName);
    }
    if (FLAGS_t1DiskSymbolName) {
        printf("%s\n ", FLAGS_t1DiskSymbolName);
    }
    TEST();

    return 0;
}
