#ifndef HASH_H
#define HASH_H

#include <stdint.h>

static inline uint32_t HashFunc(int32_t *key, uint32_t keySize, uint32_t seed){
    uint32_t m = 0x5bd1e995;
    uint32_t r = 24;

    uint32_t  h = seed ^ sizeof(uint32_t);
    char *data = (char *)key;
    int len = keySize;

    while(len >= 4){
        uint32_t k = *((uint32_t*)data);
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }
    switch(len){
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    }
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

#endif //HASH_H
