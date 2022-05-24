#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <pthread.h>

#include "infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagBlockingQueueData {
    bool shutdown;
    size_t queueSize;
    size_t currentSize;
    void *queue;
    int32_t pushCursor;
    int32_t popCursor;
    uint32_t itemSize;
    pthread_mutex_t mutex;
    pthread_cond_t pushCv;
    pthread_cond_t popCv;
} BlockingQueueData;

typedef struct tagBlockingQueueIntf {
    void (*push)(void *self, void *data);
    void (*pop)(void *self, void *data);
    void (*shutdown)(void *self);
    int32_t (*querySize)(void *self);
} BlockingQueueIntf;

typedef struct tagBlockingQueue {
    BlockingQueueData data;
    BlockingQueueIntf *intf;
} BlockingQueue;

typedef struct tagBlockingQueueConfigure {
    uint32_t queueSize;
    uint32_t itemSize;
} BlockingQueueConfigure;

int32_t CreateBlockingQueue(void **__queue, BlockingQueueConfigure *conf);

void DestroyBlockingQueue(BlockingQueue *queue);

#ifdef __cplusplus
}
#endif

#endif
