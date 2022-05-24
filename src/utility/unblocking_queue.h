#ifndef UNBLOCKING_QUEUE_H
#define UNBLOCKING_QUEUE_H

#include <pthread.h>

#include "infracture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagUnblockingQueueData {
    bool shutdown;
    struct list_head queue;

    int32_t cursor;
    uint32_t itemSize;

    pthread_mutex_t mutex;
    pthread_cond_t popCv;
} UnblockingQueueData;

typedef struct tagUnblockingQueueIntf {
    void (*push)(void *self, void *data);
    void (*pop)(void *self, void *data);
    void (*shutdown)(void *self);
    int32_t (*querySize)(void *self);
} UnblockingQueueIntf;

typedef struct tagUnblockingQueue {
    UnblockingQueueData data;
    UnblockingQueueIntf *intf;
} UnblockingQueue;

typedef struct tagUnblockingQueueConfigure {
    uint32_t itemSize;
} UnblockingQueueConfigure;

int32_t CreateUnblockingQueue(void **__queue, UnblockingQueueConfigure *conf);

void DestroyUnblockingQueue(UnblockingQueue *queue);

#ifdef __cplusplus
}
#endif

#endif  // UNBLOCKING_ QUEUE_ H