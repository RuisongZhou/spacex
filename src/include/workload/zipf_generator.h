#ifndef INCLUDE_WORKLOAD_ZIPF_GENERATOR_H
#define INCLUDE_WORKLOAD_ZIPF_GENERATOR_H

#include <stdint.h>

#include "src/include/io.h"
#include "src/utility/infracture.h"
#include "src/utility/zipfian.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagZipfGeneratorData {
    uint64_t maxLbaCount;
    uint32_t readWriteRatio;
    uint32_t readLbaOffset;
    uint32_t writeLbaOffset;
    uint64_t swapStart;
    uint64_t swapMid;
    uint64_t swapEnd;
    uint64_t transferOffset;
    uint64_t transferOffsetBase;
    double readTheta;
    double writeTheta;

    uint64_t maxIOCount;

    ZipfGen* readGen;
    ZipfGen* writeGen;
} ZipfGeneratorData;

typedef struct tagZipfGeneratorIntf {
    void (*generateReadWriteIO)(void* self, IO* io);
    void (*updateHeatDistribution)(void* self);
} ZipfGeneratorIntf;

typedef struct tagZipfGenerator {
    ZipfGeneratorData data;
    ZipfGeneratorIntf* intf;
} ZipfGenerator;

typedef struct tagZipfGeneratorConfigure {
    uint32_t maxBlockCount;
    uint32_t readWriteRatio;
    uint32_t readLbaOffsetRatio;
    uint32_t writeLbaOffsetRatio;
    uint64_t swapStartRatio;
    uint64_t swapMidRatio;
    uint64_t swapEndRatio;
    uint32_t transferOffsetRatio;
    double readTheta;
    double writeTheta;
    uint64_t maxIOCount;
} ZipfGeneratorConfigure;

int32_t CreateZipfGenerator(ZipfGenerator** generator, ZipfGeneratorConfigure* conf);

void DestroyZipfGenerator(ZipfGenerator** generator);

#ifdef __cplusplus
}
#endif
#endif  // INCLUDE_WORKLOAD_ZIPF_GENERATOR_H