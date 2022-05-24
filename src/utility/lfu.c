#include "lfu.h"

#include <string.h>

static inline int32_t CreateFreqNode(uint32_t priority, FreqNode **freqNode)
{
    *freqNode = (FreqNode *)malloc(sizeof(FreqNode));
    assert(*freqNode != NULL);
    memset(*freqNode, 0, sizeof(FreqNode));
    (*freqNode)->freq = priority;
    (*freqNode)->pItemHead = (void *)malloc(sizeof(struct list_head));
    assert((*freqNode)->pItemHead != NULL);
    memset((*freqNode)->pItemHead, 0, sizeof(struct list_head));
    INIT_LIST_HEAD((struct list_head *)(*freqNode)->pItemHead);
    return RETURN_OK;
}

static inline int32_t DeleteFreqNode(FreqNode *freqNode)
{
    list_del_init(&freqNode->freqNode);
    free(freqNode->pItemHead);
    free(freqNode);
    return RETURN_OK;
}

static int32_t Update(void *self, void *key, uint32_t freq)
{
    int32_t ret = RETURN_ERROR;
    LFU *lfu = (LFU *)self;
    Indexer *lfuIndexer = lfu->data.lfuIndexer;
    uint32_t priority = freq + lfu->data.age;

    // 1.
    ItemNode **ppVisitedItemNode = NULL;
    ItemNode *visitedItemNode = NULL;
    struct list_head *startFreqListNode = NULL;

    ret = lfuIndexer->intf->query((void *)lfuIndexer, key, (void **)&ppVisitedItemNode);
    if (ret == RETURN_OK) {
        visitedItemNode = *ppVisitedItemNode;
        FreqNode *oldFreqNode = (FreqNode *)list_entry(visitedItemNode->pFreqHead, FreqNode, freqNode);
        if (oldFreqNode->freq == priority) {
            return RETURN_OK;
        }
        // 2(a).
        list_del_init(&visitedItemNode->itemNode);
        startFreqListNode = oldFreqNode->freqNode.prev;
        if (list_empty((struct list_head *)oldFreqNode->pItemHead)) {
            DeleteFreqNode(oldFreqNode);
        }
    }
    // 2(b).
    else {
        visitedItemNode = (ItemNode *)malloc(sizeof(ItemNode));
        assert(visitedItemNode != NULL);
        memset(visitedItemNode, 0, sizeof(ItemNode));
        memcpy(&visitedItemNode->key, key, sizeof(visitedItemNode->key));
        ItemNode **ppItemNode = (ItemNode **)malloc(sizeof(ItemNode *));
        assert(ppItemNode != NULL);
        *ppItemNode = visitedItemNode;
        lfuIndexer->intf->insertKV((void *)lfuIndexer, key, ppItemNode);
        startFreqListNode = &lfu->data.freqQ;
    }
    // 3.
    struct list_head *cur = NULL;
    struct list_head *next = NULL;
    FreqNode *freqNode = NULL;
    list_for_each_safe(cur, next, startFreqListNode)
    {
        if (cur == &lfu->data.freqQ) {
            break;
        }
        freqNode = (FreqNode *)list_entry(cur, FreqNode, freqNode);
        if (freqNode->freq >= priority) {
            break;
        }
    }
    bool flag = false;
    if (freqNode == NULL) {
        flag = true;
    } else if (freqNode->freq != priority) {
        flag = true;
    }
    if (flag) {
        ret = CreateFreqNode(priority, &freqNode);
        list_add(&freqNode->freqNode, cur->prev);
    }
    list_add(&visitedItemNode->itemNode, freqNode->pItemHead);
    assert(((struct list_head *)freqNode->pItemHead)->next == &visitedItemNode->itemNode);

    visitedItemNode->pFreqHead = (void *)&freqNode->freqNode;
    return RETURN_OK;
}

static int32_t Evict(void *self, void *key)
{
    LFU *lfu = (LFU *)self;
    Indexer *lfuIndexer = lfu->data.lfuIndexer;
    assert(list_empty(&lfu->data.freqQ) == 0);

    FreqNode *victimFreqNode = (FreqNode *)list_entry(lfu->data.freqQ.next, FreqNode, freqNode);
    assert(list_empty((struct list_head *)victimFreqNode->pItemHead) == 0);

    ItemNode *victimItemNode =
        (ItemNode *)list_entry(((struct list_head *)victimFreqNode->pItemHead)->prev, ItemNode, itemNode);
    lfu->data.age = victimFreqNode->freq;
    memcpy(key, &victimItemNode->key, lfu->data.keySize);

    int32_t ret = lfuIndexer->intf->delete ((void *)lfuIndexer, &victimItemNode->key);
    assert(ret == RETURN_OK);

    list_del_init(&victimItemNode->itemNode);
    if (list_empty((struct list_head *)victimFreqNode->pItemHead)) {
        DeleteFreqNode(victimFreqNode);
    }
    free(victimItemNode);
    return RETURN_OK;
}

static int32_t Delete(void *self, void *key)
{
    int32_t ret = RETURN_ERROR;
    LFU *lfu = (LFU *)self;

    Indexer *lfuIndexer = lfu->data.lfuIndexer;
    assert(list_empty(&lfu->data.freqQ) == 0);

    ItemNode **ppItemNode = NULL;
    ItemNode *itemNode = NULL;
    ret = lfuIndexer->intf->query((void *)lfuIndexer, key, (void **)&ppItemNode);
    assert(ret == RETURN_OK);

    itemNode = *ppItemNode;
    ret = lfuIndexer->intf->delete ((void *)lfuIndexer, key);
    assert(ret == RETURN_OK);

    FreqNode *freqNode = (FreqNode *)list_entry(itemNode->pFreqHead, FreqNode, freqNode);
    list_del_init(&itemNode->itemNode);
    if (list_empty((struct list_head *)freqNode->pItemHead)) {
        DeleteFreqNode(freqNode);
    }

    free(itemNode);
    return RETURN_OK;
}

static LFUIntf g_LFUIntf = {
    .update = Update,
    .evict = Evict,
    .delete = Delete,
};

int32_t CreateLFU(void **__lfu, LFUConfigure *conf)
{
    LFU *lfu = (LFU *)malloc(sizeof(LFU));
    if (!lfu) {
        return RETURN_ERROR;
    }

    (void)memset(lfu, 0, sizeof(LFU));
    INIT_LIST_HEAD(&lfu->data.freqQ);

    IndexerConfigure lfuIndexerConf = {
        .keySize = conf->keySize,
        .bucketCount = conf->bucketCount,
    };
    void *lfuIndexer = NULL;
    int32_t ret = CreateIndexer(&lfuIndexer, &lfuIndexerConf);
    assert(ret == RETURN_OK);

    lfu->data.lfuIndexer = lfuIndexer;
    lfu->data.keySize = conf->keySize;
    lfu->intf = &g_LFUIntf;

    *__lfu = (void *)lfu;

    return RETURN_OK;
}

void DestroyLFU(LFU *lfu)
{
    if (lfu == NULL) {
        return;
    }
    DestroyIndexer(lfu->data.lfuIndexer);

    struct list_head *cur = NULL;
    struct list_head *next = NULL;
    struct list_head *targetBucket = &lfu->data.freqQ;

    list_for_each_safe(cur, next, targetBucket)
    {
        FreqNode *freqNode = (FreqNode *)list_entry(cur, FreqNode, freqNode);
        if (freqNode == NULL) continue;
        struct list_head *cur = NULL;
        struct list_head *next = NULL;
        struct list_head *targetBucket = (struct list_head *)freqNode->pItemHead;
        list_for_each_safe(cur, next, targetBucket)
        {
            ItemNode *itemNode = (ItemNode *)list_entry(cur, ItemNode, itemNode);
            if (itemNode) {
                free(itemNode);
            }
        }
        free(freqNode);
    }
    free(lfu);
    lfu = NULL;
}
