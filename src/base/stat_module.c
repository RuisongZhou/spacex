

#include "stat_module.h"

#include <string.h>

#include "src/utility/infracture.h"

static char *Report(void *self)
{
    StatModule *statModule = (StatModule *)self;

    snprintf(statModule->data.text, kStatTextBuffSize, "%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u",
             statModule->data.hostReadCount, statModule->data.hostWriteCount, statModule->data.gcCount,
             statModule->data.gcWriteCount, statModule->data.tierDownCount, statModule->data.tierDownWriteCount,
             statModule->data.gcSlowCount, statModule->data.tierSlowCount, statModule->data.gcStallCount,
             statModule->data.tierStallCount);
    return statModule->data.text;
}

static void Reset(void *self)
{
    StatModule *statModule = (StatModule *)self;
    memset(&statModule->data, 0, sizeof(StatModuleData));
}

static StatModuleIntf g_intf = {
    .report = Report,
    .reset = Reset,
};

int32_t CreateStatModule(StatModule **ppStatModule)
{
    StatModule *stat = (StatModule *)malloc(sizeof(StatModule));
    ASSERT(stat);
    memset(stat, 0, sizeof(StatModule));

    stat->intf = &g_intf;

    *ppStatModule = stat;
    return RETURN_OK;
}

void DestroyStatModule(StatModule **ppStatModule)
{
    free(*ppStatModule);
    *ppStatModule = NULL;
}
