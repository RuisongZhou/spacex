#ifndef INCLUDE_WORKLOAD_RANDOM_GENERATOR_H
#define INCLUDE_WORKLOAD_RANDOM_GENERATOR_H

#include "src/include/io.h"
#include "src/utility/random.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagRandomGeneratorData {
    uint64_t maxLbaCount;
    uint64_t maxIOCount;
    RandomGen *rnd;
} RandomGeneratorData;

typedef struct tagRandomGeneratorConfigure {
    uint32_t maxBlockCount;
    uint64_t maxIOCount;
} RandomGeneratorConfigure;

typedef struct tagRandomGeneratorIntf {
    void (*generateReadWriteIO)(void *self, IO *io);
} RandomGeneratorIntf;

typedef struct tagRandomGenerator {
    RandomGeneratorData data;
    RandomGeneratorIntf *intf;
} RandomGenerator;

int32_t CreateRandomGenerator(RandomGenerator **generator, RandomGeneratorConfigure *conf);

void DestroyRandomGenrator(RandomGenerator **generator);

#ifdef __cplusplus 
}
#endif
#endif  // INCLUDE_WORKLOAD_RANDOM_GENERATOR_H
