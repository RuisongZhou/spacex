#include "unblocking_queue.h"

#include <string.h>

typedef struct tagQueueItem {
    struct list_head node;
    void *data;
} QueueItem;

static inline bool IsEmpty(UnblockingQueue *queue) { return queue->data.cursor == 0; }

static void Push(void *self, void *data)
{
    UnblockingQueue *queue = (UnblockingQueue *)self;
    pthread_mutex_lock(&queue->data.mutex);

    QueueItem *blockData = (QueueItem *)malloc(sizeof(struct list_head) + queue->data.itemSize);
    assert(blockData);
    memcpy(&blockData->data, data, queue->data.itemSize);
    list_add(&blockData->node, queue->data.queue.prev);
    queue->data.cursor++;

    pthread_cond_signal(&queue->data.popCv);
    pthread_mutex_unlock(&queue->data.mutex);
}

static void Pop(void *self, void *data)
{
    UnblockingQueue *queue = (UnblockingQueue *)self;

    pthread_mutex_lock(&queue->data.mutex);
    if (IsEmpty(queue) && !queue->data.shutdown) {
        pthread_cond_wait(&queue->data.popCv, &queue->data.mutex);
    }

    if (!IsEmpty(queue)) {
        QueueItem *blockData = (QueueItem *)list_entry(queue->data.queue.next, QueueItem, node);
        list_del_init(queue->data.queue.next);
        memcpy(data, &blockData->data, queue->data.itemSize);
        free(blockData);
        queue->data.cursor--;
    }
    pthread_mutex_unlock(&queue->data.mutex);
}

static void Shutdown(void *self)
{
    UnblockingQueue *queue = (UnblockingQueue *)self;
    queue->data.shutdown = true;
    pthread_cond_broadcast(&queue->data.popCv);
}

static int32_t QuerySize(void *self)
{
    UnblockingQueue *queue = (UnblockingQueue *)self;
    return queue->data.cursor;
}

static UnblockingQueueIntf g_intf = {
    .push = Push,
    .pop = Pop,
    .shutdown = Shutdown,
    .querySize = QuerySize,
};

int32_t CreateUnblockingQueue(void **__queue, UnblockingQueueConfigure *conf)
{
    UnblockingQueue *queue = (UnblockingQueue *)malloc(sizeof(UnblockingQueue));
    ASSERT(queue);
    memset(queue, 0, sizeof(UnblockingQueue));

    INIT_LIST_HEAD(&queue->data.queue);
    queue->data.itemSize = conf->itemSize;
    queue->data.cursor = 0;
    queue->data.shutdown = false;

    pthread_mutex_init(&queue->data.mutex, NULL);
    pthread_cond_init(&queue->data.popCv, NULL);

    queue->intf = &g_intf;
    (*__queue) = queue;

    return RETURN_OK;
}

void DestroyUnblockingQueue(UnblockingQueue *queue)
{
    Shutdown(queue);
    pthread_cond_destroy(&queue->data.popCv);
    free(queue);
}
