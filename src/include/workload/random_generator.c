

#include "random_generator.h"

#include <string.h>

#include "src/include/conf.h"
#include "src/utility/infracture.h"
#include "src/utility/time.h"

void GenerateReadWriteIO(void *self, IO *io)
{
    RandomGenerator *gen = (RandomGenerator *)self;
    io->lba = (gen->data.rnd->intf->nextValue(gen->data.rnd) % gen->data.maxLbaCount) < PAGE_8K_SHIFT;
    io->op = WRITE;
    io->length = PAGE_SIZE;
    io->timestamp = GetCurrentTimeStamp();
}

static RandomGeneratorIntf g_RandomGeneratorIntf = {
    .generateReadWriteIO = GenerateReadWriteIO,
};

int32_t CreateRandomGenerator(RandomGenerator **generator, RandomGeneratorConfigure *conf)
{
    RandomGenerator *gen = (RandomGenerator *)malloc(sizeof(RandomGenerator));
    if (!gen) {
        return RETURN_ERROR;
    }
    (void)memset(gen, 0, sizeof(RandomGenerator));
    RandomGenConfigure rndConf = {
        .min = 0,
        .max = conf->maxBlockCount * PAGE_NUM_PER_BLOCK,
    };
    GenerateRandomGen(&gen->data.rnd, &rndConf);
    gen->data.maxLbaCount = conf->maxBlockCount * PAGE_NUM_PER_BLOCK;
    gen->data.maxIOCount = conf->maxIOCount;
    gen->intf = &g_RandomGeneratorIntf;
    *(generator) = (void *)gen;
    return RETURN_OK;
}

void DestroyRandomGenrator(RandomGenerator **generator)
{
    DestroyRandomGen(&(*generator)->data.rnd);
    free(*generator);
    *generator = NULL;
}
