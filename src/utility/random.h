// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagRandomGenData {
    void* rng;
    void* dist;
} RandomGenData;

typedef struct tagRandomGenIntf {
    uint32_t (*nextValue)(void* self);
} RandomGenIntf;

typedef struct tagRandomGen {
    RandomGenData data;
    RandomGenIntf* intf;
} RandomGen;

typedef struct tagRandomGenConfigure {
    uint32_t min;
    uint32_t max;
} RandomGenConfigure;

void GenerateRandomGen(RandomGen** rnd, RandomGenConfigure* conf);

void DestroyRandomGen(RandomGen** rnd);

#ifdef __cplusplus
}
#endif
#endif  // RANDOM_H
