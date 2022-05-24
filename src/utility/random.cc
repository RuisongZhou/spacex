#include "random.h"

#include <random>

static uint32_t NextValue(void* self)
{
    RandomGen* rnd = (RandomGen*)self;
    return (*static_cast<std::uniform_int_distribution<std::mt19937_64::result_type>*>(rnd->data.dist))(
        *static_cast<std::mt19937_64*>(rnd->data.rng));
}

static RandomGenIntf g_intf = {
    .nextValue = NextValue,
};

void GenerateRandomGen(RandomGen** rnd, RandomGenConfigure* conf)
{
    RandomGen* rndGen = (RandomGen*)malloc(sizeof(RandomGen));
    if (!rndGen) {
        exit(-1);
        return;
    }
    rndGen->data.rng = new std::mt19937_64();
    rndGen->data.dist = new std::uniform_int_distribution<std::mt19937_64::result_type>(conf->min, conf->max);
    rndGen->intf = &g_intf;
    *rnd = rndGen;
}

void DestroyRandomGen(RandomGen** rnd)
{
    free(*rnd);
    *rnd = NULL;
}
