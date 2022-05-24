#include "blocking_queue.h"

#include <string.h>

static inline bool IsFull(BlockingQueue *queue) { return queue->data.currentSize == queue->data.queueSize; }

static inline bool IsEmpty(BlockingQueue *queue) { return queue->data.currentSize == 0; }

static void Push(void *self, void *data)
{
    BlockingQueue *queue = (BlockingQueue *)self;
    pthread_mutex_lock(&queue->data.mutex);
    if (IsFull(queue)) {
        pthread_cond_wait(&queue->data.pushCv, &queue->data.mutex);
    }
    memcpy(queue->data.queue + queue->data.pushCursor * queue->data.itemSize, data, queue->data.itemSize);
    queue->data.pushCursor = (queue->data.pushCursor + 1) % queue->data.queueSize;
    queue->data.currentSize++;
    pthread_cond_signal(&queue->data.popCv);
    pthread_mutex_unlock(&queue->data.mutex);
}

static void Pop(void *self, void *data)
{
    BlockingQueue *queue = (BlockingQueue *)self;
    pthread_mutex_lock(&queue->data.mutex);
    if (IsEmpty(queue) && !queue->data.shutdown) {
        pthread_cond_wait(&queue->data.popCv, &queue->data.mutex);
    }

    if (!IsEmpty(queue)) {
        memcpy(data, queue->data.queue + queue->data.popCursor * queue->data.itemSize, queue->data.itemSize);
        queue->data.popCursor = (queue->data.popCursor + 1) % (queue->data.queueSize);
        queue->data.currentSize--;
    }
    pthread_cond_signal(&queue->data.pushCv);
    pthread_mutex_unlock(&queue->data.mutex);
}

static void Shutdown(void *self)
{
    BlockingQueue *queue = (BlockingQueue *)self;
    queue->data.shutdown = true;
    pthread_cond_broadcast(&queue->data.popCv);
}

static int32_t QuerySize(void *self)
{
    BlockingQueue *queue = (BlockingQueue *)self;
    return queue->data.currentSize;
}

static BlockingQueueIntf g_intf = {
    .push = Push,
    .pop = Pop,
    .shutdown = Shutdown,
    .querySize = QuerySize,
};

int32_t CreateBlockingQueue(void **__queue, BlockingQueueConfigure *conf)
{
    BlockingQueue *queue = (BlockingQueue *)malloc(sizeof(BlockingQueue));
    ASSERT(queue);
    memset(queue, 0, sizeof(BlockingQueue));
    queue->data.queue = malloc(conf->itemSize * conf->queueSize);
    ASSERT(queue->data.queue);
    memset(queue->data.queue, 0, conf->itemSize * conf->queueSize);
    queue->data.itemSize = conf->itemSize;
    queue->data.queueSize = conf->queueSize;
    queue->data.currentSize = 0;
    queue->data.popCursor = 0;
    queue->data.pushCursor = 0;
    queue->data.shutdown = false;
    pthread_mutex_init(&queue->data.mutex, NULL);
    pthread_cond_init(&queue->data.pushCv, NULL);
    pthread_cond_init(&queue->data.popCv, NULL);
    queue->intf = &g_intf;
    (*__queue) = queue;
    return RETURN_OK;
}

void DestroyBlockingQueue(BlockingQueue *queue)
{
    Shutdown(queue);
    pthread_cond_destroy(&queue->data.popCv);
    pthread_cond_destroy(&queue->data.pushCv);
    free(queue->data.queue);
    free(queue);
}
