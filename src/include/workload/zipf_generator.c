
#include "zipf_generator.h"

#include <string.h>

#include "src/include/conf.h"
#include "src/utility/time.h"

uint64_t ZipfGenerateReadIO(ZipfGenerator* zipf)
{
    uint64_t lba = zipf->data.readGen->intf->nextValue(zipf->data.readGen);
    return (lba + zipf->data.readLbaOffset) % zipf->data.maxLbaCount;
}

// default swap write
uint64_t ZipfGenerateWriteIO(ZipfGenerator* zipf)
{
    uint64_t lba = zipf->data.writeGen->intf->nextValue(zipf->data.readGen);
    lba = (lba + zipf->data.writeLbaOffset) % zipf->data.maxLbaCount;
    if (lba >= zipf->data.swapStart && lba < zipf->data.swapMid) {
        lba += (zipf->data.swapMid - zipf->data.swapStart);
    } else if (lba >= zipf->data.swapMid && lba < zipf->data.swapEnd) {
        lba -= (zipf->data.swapEnd - zipf->data.swapMid);
    }
    return lba;
}

void ZipfGenerateReadWriteIO(void* self, IO* io)
{
    ZipfGenerator* zipf = (ZipfGenerator*)self;
    uint32_t readOrWrite = ((uint32_t)rand() % 100) < zipf->data.readWriteRatio;
    if (readOrWrite) {
        io->lba = ZipfGenerateReadIO(zipf);
        io->op = READ;
    } else {
        io->lba = ZipfGenerateWriteIO(zipf);
        io->op = WRITE;
    }
    io->length = PAGE_SIZE;
    // heat transfer
    io->lba = (io->lba + zipf->data.transferOffsetBase) % zipf->data.maxLbaCount;
    io->lba = io->lba << PAGE_8K_SHIFT;
    io->timestamp = GetCurrentTimeStamp();
}

void ZipfUpdateHeatDistribution(void* self)
{
    ZipfGenerator* zipf = (ZipfGenerator*)self;
    zipf->data.transferOffsetBase =
        (zipf->data.transferOffsetBase + zipf->data.transferOffset) % (zipf->data.maxLbaCount);
}

static ZipfGeneratorIntf g_ZipfGeneratorIntf = {
    .generateReadWriteIO = ZipfGenerateReadWriteIO,
    .updateHeatDistribution = ZipfUpdateHeatDistribution,
};

int32_t CreateZipfGenerator(ZipfGenerator** generator, ZipfGeneratorConfigure* conf)
{
    ZipfGenerator* zipf = (ZipfGenerator*)malloc(sizeof(ZipfGenerator));
    if (!zipf) {
        return -1;
    }
    (void)memset(zipf, 0, sizeof(ZipfGenerator));
    // SetConfigure(gen, conf);
    zipf->data.maxLbaCount = conf->maxBlockCount * PAGE_NUM_PER_BLOCK;
    zipf->data.readWriteRatio = conf->readWriteRatio;
    zipf->data.transferOffset = zipf->data.maxLbaCount * conf->transferOffsetRatio / 100;
    zipf->data.readLbaOffset = zipf->data.maxLbaCount * conf->readLbaOffsetRatio / 100;
    zipf->data.writeLbaOffset = zipf->data.maxLbaCount * conf->writeLbaOffsetRatio / 100;
    zipf->data.swapStart = zipf->data.maxLbaCount * conf->swapStartRatio / 100;
    zipf->data.swapMid = zipf->data.maxLbaCount * conf->swapMidRatio / 100;
    zipf->data.swapEnd = zipf->data.maxLbaCount * conf->swapEndRatio / 100;
    zipf->data.maxIOCount = conf->maxIOCount;

    ZipfGenConfigure zipfConf = {
        .min = 0,
        .max = zipf->data.maxLbaCount,
        .theta = conf->readTheta,
    };
    GenerateZipfGen((void**)&zipf->data.readGen, &zipfConf);
    zipfConf.theta = conf->writeTheta;
    GenerateZipfGen((void**)&zipf->data.writeGen, &zipfConf);
    zipf->intf = &g_ZipfGeneratorIntf;
    *(generator) = (void*)zipf;
    return 0;
}

void DestroyZipfGenerator(ZipfGenerator** generator)
{
    DestroyZipfGen((void**)&(*generator)->data.writeGen);
    DestroyZipfGen((void**)&(*generator)->data.readGen);

    free(*generator);
    *generator = NULL;
}
