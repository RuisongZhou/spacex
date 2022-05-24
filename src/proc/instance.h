

#ifndef PROC_INSTANCE_H
#define PROC_INSTANCE_H

#include <stdint.h>

#include "src/base/storage_system.h"
#include "src/include/workload/random_generator.h"
#include "src/include/workload/sequential_generator.h"
#include "src/include/workload/zipf_generator.h"

#ifdef __cplusplus
extern "C" {
#endif

void Initialize();

void CreateStorageSystemInstance(char *reportFileName);

void PrintWriteHistgram();

void WaitForStorageSystemInstance();

void CreateZipfGeneratorInstance(ZipfGeneratorConfigure *conf);

void WaitForZipfGeneratorInstance();

void CreateSequentialGeneratorInstance(SequentialGeneratorConfigure *conf);

void WaitForSequentialGeneratorInstance();

void CreateRandomGeneratorInstance(RandomGeneratorConfigure *conf);

void WaitForRandomGeneratorInstance();

#ifdef __cplusplus
}
#endif

#endif  // PROC_INSTANCE_H
