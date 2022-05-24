
#include "sequential_generator.h"

#include <string.h>

#include "src/include/conf.h"
#include "src/utility/infracture.h"
#include "src/utility/time.h"

void GenerateWarmupIO(void* self, IO* io)
{
    SequentialGenerator* seq = (SequentialGenerator*)self;
    seq->data.curPos--;
    io->lba = seq->data.curPos << PAGE_8K_SHIFT;
    io->op = WRITE;
    if (seq->data.curPos == 0) {
        seq->data.curPos = seq->data.maxLbaCount;
    }
    io->length = PAGE_SIZE;
    io->timestamp = GetCurrentTimeStamp();
}

static SequentialGeneratorIntf g_SequentialGeneratorIntf = {
    .generateWarmupIO = GenerateWarmupIO,
};

int32_t CreateSequentialGenerator(SequentialGenerator** generator, SequentialGeneratorConfigure* conf)
{
    SequentialGenerator* gen = (SequentialGenerator*)malloc(sizeof(SequentialGenerator));
    if (!gen) {
        return RETURN_ERROR;
    }
    (void)memset(gen, 0, sizeof(SequentialGenerator));
    gen->data.maxLbaCount = conf->maxBlockCount * PAGE_NUM_PER_BLOCK;
    gen->data.curPos = gen->data.maxLbaCount;
    gen->intf = &g_SequentialGeneratorIntf;
    *(generator) = gen;
    return RETURN_OK;
}

void DestroySequentialGenrator(SequentialGenerator** generator)
{
    free(*generator);
    *generator = NULL;
}
