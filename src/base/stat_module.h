#ifndef BASE_STAT_MODULE_H
#define BASE_STAT_MODULE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { kStatTextBuffSize = 80 };

typedef struct tagStatModuleData {
    uint64_t hostWriteCount;
    uint64_t hostReadCount;
    uint64_t gcWriteCount;
    uint64_t gcCount;
    uint64_t tierDownWriteCount;
    uint64_t tierDownCount;

    uint32_t gcStallCount;
    uint32_t tierStallCount;

    uint32_t gcSlowCount;
    uint32_t tierSlowCount;

    char text[kStatTextBuffSize];
} StatModuleData;

typedef struct tagStatModuleIntf {
    char *(*report)(void *self);
    void (*reset)(void *self);
} StatModuleIntf;

typedef struct tagStatModule {
    StatModuleData data;
    StatModuleIntf *intf;
} StatModule;

int32_t CreateStatModule(StatModule **ppStatModule);

void DestroyStatModule(StatModule **ppStatModule);

#ifdef __cplusplus
}
#endif

#endif  // BASE_STAT_MODULE_H