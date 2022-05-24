
#ifndef INCLUDE_WORKLOAD_SEQUENTIAL_GENERATOR_H
#define INCLUDE_WORKLOAD_SEQUENTIAL_GENERATOR_H

#include <stdint.h>

#include "src/include/io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagSequentialGeneratorData {
    uint64_t maxLbaCount;
    uint64_t curPos;
} SequentialGeneratorData;

typedef struct tagSequentialGeneratorIntf {
    void (*generateWarmupIO)(void* self, IO* io);
} SequentialGeneratorIntf;

typedef struct tagSequentialGenerator {
    SequentialGeneratorData data;
    SequentialGeneratorIntf* intf;
} SequentialGenerator;

typedef struct tagSequentialGeneratorConfigure {
    uint32_t maxBlockCount;
} SequentialGeneratorConfigure;

int32_t CreateSequentialGenerator(SequentialGenerator** generator, SequentialGeneratorConfigure* conf);

void DestroySequentialGenrator(SequentialGenerator** generator);

#ifdef __cplusplus
}
#endif
#endif  // INCLUDE_WORKLOAD_SEQUENTIAL_GENERATOR_H