#include "storage_system.h"

#include <string.h>

#include "src/utility/time.h"

#define MAX_BUFF_SIZE 260

static TierPage* g_tierDownPages = NULL;
static uint32_t g_tierDownPageCnt = 0;
static IO* g_io = NULL;
static PageFreq g_pageFreq = {0};

//-----------------------------------------------------------------------
// write io progress.
//-----------------------------------------------------------------------
void* TierDownThread(void* param)
{
    StorageSystem* storSys = (StorageSystem*)param;
    LayerModule* t0 = storSys->data.tiers[0];
    LayerModule* t1 = storSys->data.tiers[1];
    while (storSys->data.running) {
        pthread_mutex_lock(&storSys->data.tierDownThread.mutex);
        pthread_cond_wait(&storSys->data.tierDownThread.cv, &storSys->data.tierDownThread.mutex);
        TierPage* tierPages = g_tierDownPages;
        uint32_t tierPageCnt = g_tierDownPageCnt;
        g_tierDownPages = NULL;
        g_tierDownPageCnt = 0;
        // handle tier down
        if (g_tierDownPages) {
            t1->intf->writeTierDownIO(t1, tierPages, tierPageCnt);
            free(tierPages);
        }
        pthread_mutex_unlock(&storSys->data.tierDownThread.mutex);
    }

    return NULL;
}

static int32_t WriteIOAux(void* self, IO* io, PageFreq* oldPageFreq, int32_t layer)
{
    int32_t ret = RETURN_ERROR;
    StorageSystem* storSys = (StorageSystem*)self;
    TierPage* pages = NULL;
    uint32_t pageCnt = 0;
    ret = storSys->data.tiers[layer]->intf->writeIO(storSys->data.tiers[layer], io, oldPageFreq, &g_tierDownPages,
                                                    &g_tierDownPageCnt);
    ASSERT(ret == RETURN_OK);

    // make its copy invalid in other tiers except for tier i
    for (uint32_t i = 0; i < TIER_NUM; i++) {
        if (i == layer) continue;
        storSys->data.tiers[i]->intf->deleteData(storSys->data.tiers[i], io->lba, io->length);
    }

    if (g_tierDownPages != NULL) {
        ASSERT(layer + 1 < TIER_NUM);  // out of size
        pthread_cond_signal(&storSys->data.tierDownThread.cv);
    }
    return ret;
}

static int32_t QueryPageHistoryFreq(void* self, uint64_t lba, PageFreq* pageFreq)
{
    int32_t ret = RETURN_ERROR;
    StorageSystem* storSys = (StorageSystem*)self;
    for (int32_t i = 0; i < TIER_NUM; i++) {
        ret = storSys->data.tiers[i]->intf->queryFrequency(storSys->data.tiers[i], lba, pageFreq);
        if (ret == RETURN_OK) break;
    }
    return ret;
}

static int32_t WriteHostIO(void* self, IO* io)
{
    StorageSystem* storSys = (StorageSystem*)self;
    IOScheduler* scheduler = storSys->data.ioScheduler;

    int32_t ret = RETURN_ERROR;
    PageFreq pageFreq = {0};
    QueryPageHistoryFreq(self, io->lba, &pageFreq);
    int32_t layerIdx = scheduler->intf->scheduleLayer(scheduler, io->lba, io->length, &pageFreq);
    ASSERT(layerIdx < TIER_NUM);
    ret = WriteIOAux(self, io, &pageFreq, layerIdx);
    return ret;
}

//-----------------------------------------------------------------------
// read io progress.
//-----------------------------------------------------------------------
static int32_t ReadHostIO(void* self, IO* io)
{
    int32_t ret = RETURN_ERROR;
    StorageSystem* storSys = (StorageSystem*)self;
    for (int32_t i = 0; i < TIER_NUM; i++) {
        ret = storSys->data.tiers[i]->intf->readIO(storSys->data.tiers[i], io);
        if (ret == RETURN_OK) {
            /*just find it*/
            break;
        }
    }
    return ret;
}

static void PrintWriteHistgram(void* self)
{
    StorageSystem* storSys = (StorageSystem*)self;
    Histogram* writeHist = storSys->data.writeHist;

    for (int i = 0; i < TIER_NUM; i++) {
        DiskOperator* diskOp = storSys->data.tiers[i]->data.diskOp;
        if (!diskOp) continue;
        for (int j = 0; j < diskOp->data.hostConcurrency; j++) {
            Histogram* other = diskOp->data.writeHists[j];
            writeHist->intf->merge(writeHist, other);
            other->intf->clear(other);
        }
    }

    char* text = writeHist->intf->getHistogram(writeHist);
    printf("%s", text);
    writeHist->intf->clear(writeHist);
}

static void Report(void* self)
{
    StorageSystem* storSys = (StorageSystem*)self;

    char buf[MAX_BUFF_SIZE] = {0};
    StatModule* statModule = storSys->data.tiers[0]->data.statModule;

    snprintf(buf, MAX_BUFF_SIZE, "%s,", statModule->intf->report(statModule));
    statModule->intf->reset(statModule);

    uint64_t len = strlen(buf);

    statModule = storSys->data.tiers[1]->data.statModule;
    snprintf(buf + len, MAX_BUFF_SIZE, "%s,", statModule->intf->report(statModule));
    statModule->intf->reset(statModule);

    len = strlen(buf);
    snprintf(buf + len, MAX_BUFF_SIZE, "%u,%u\n", storSys->data.tiers[0]->data.diskOp->data.slowCount,
             storSys->data.tiers[0]->data.diskOp->data.stallCount);

    storSys->data.tiers[0]->data.diskOp->data.slowCount = 0;
    storSys->data.tiers[0]->data.diskOp->data.stallCount = 0;

    fprintf(storSys->data.reportFD, "%s", buf);
    fflush(storSys->data.reportFD);
}

static StorageSystemIntf g_StorSysIntf = {
    .writeIO = WriteHostIO,
    .readIO = ReadHostIO,
    .printWriteHistgram = PrintWriteHistgram,
    .report = Report,
};

int32_t CreateStorageSystem(StorageSystem** ppStorageSystem, StorageSystemConfigure* conf)
{
    StorageSystem* storSys = (StorageSystem*)malloc(sizeof(StorageSystem));
    if (!storSys) {
        return RETURN_ERROR;
    }
    (void)memset(storSys, 0, sizeof(StorageSystem));

    int32_t ret = RETURN_OK;
    for (int i = 0; i < TIER_NUM; i++) {
        /* initialize layer module */
        conf->layerConf[i].storSys = storSys;
        ret = CreateLayerModule(&(storSys->data.tiers[i]), &conf->layerConf[i]);
        ASSERT(ret == RETURN_OK);
    }

    storSys->data.running = true;
    pthread_mutex_init(&storSys->data.tierDownThread.mutex, NULL);
    pthread_cond_init(&storSys->data.tierDownThread.cv, NULL);
    pthread_create(&storSys->data.tierDownThread.tid, NULL, TierDownThread, (void*)storSys);

    IOScheduler* ioScheduler = NULL;
    conf->schedulerConf.storSys = storSys;
    ret = CreateIOScheduler(&ioScheduler, &conf->schedulerConf);
    storSys->data.ioScheduler = ioScheduler;

    Histogram* writeHist = NULL;
    ret = CreateHistogram(&writeHist);
    storSys->data.writeHist = writeHist;

    storSys->data.reportFD = fopen(conf->reportFileName, "w+");
    ASSERT(storSys->data.reportFD != NULL);

    storSys->intf = &g_StorSysIntf;
    *ppStorageSystem = (void*)storSys;
    return RETURN_OK;
}

void DestroyStorageSystem(StorageSystem** ppStorageSystem)
{
    if (*ppStorageSystem == NULL) {
        return;
    }

    (*ppStorageSystem)->data.running = false;
    pthread_cond_signal(&(*ppStorageSystem)->data.tierDownThread.cv);
    pthread_cond_destroy(&(*ppStorageSystem)->data.tierDownThread.cv);
    pthread_join((*ppStorageSystem)->data.tierDownThread.tid, NULL);
    for (int i = 0; i < TIER_NUM; i++) {
        DestroyLayerModule(&(*ppStorageSystem)->data.tiers[i]);
    }

    DestroyHistogram(&(*ppStorageSystem)->data.writeHist);

    fclose((*ppStorageSystem)->data.reportFD);
    free(*ppStorageSystem);
    *ppStorageSystem = NULL;
}
