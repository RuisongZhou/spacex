#ifndef INFRACTURE_H
#define INFRACTURE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __cplusplus
typedef int bool;
#endif

#define atomic64_set(ptr, value) ((*ptr) = value)
#define atomic64_inc(ptr) ((*ptr) += 1)
#define atomic64_read(ptr) ((*ptr))

#define inline __inline
#define NO_LOGID 0

#define ASSERT(condition)                                                                     \
    {                                                                                         \
        if (!(condition)) {                                                                   \
            fprintf(stderr, "ASSERT FAILED: %s @ %s (%d)\n", #condition, __FILE__, __LINE__); \
        }                                                                                     \
    }

#define DBG_Print printf

#define DBG_LogInfo(LogID, format, ...)               \
    do {                                              \
        printf("[INFO]" format, ##__VA_ARGS__);       \
        printf("[%s, %u]\n", __FUNCTION__, __LINE__); \
    } while (0)

#define DBG_LogError(LogID, format, ...)              \
    do {                                              \
        printf("[ERR]" format, ##__VA_ARGS__);        \
        printf("[%s, %u]\n", __FUNCTION__, __LINE__); \
    } while (0)

#define TIME_STATS_START(beginTime) ((beginTime) = clock())
#define TIME_STATS_END_WITH_INFO(beginTime, duration, str)         \
    do {                                                           \
        duration = (double)(clock() - beginTime) / CLOCKS_PER_SEC; \
        printf("[%s] cost time: %f seconds\n", str, duration);     \
    } while (0)

struct list_head {
    struct list_head *next, *prev;
};

typedef struct {
    uint64_t key;
    uint64_t value;
} LUN_MAP_NODE_S;

typedef struct {
    struct list_head stList;
    uint32_t num : 7;
    uint32_t pad : 25;
    uint32_t pad2;
    uint8_t data[0];
} LUN_MAP_ROCACHE_NODE_S;

typedef struct {
    struct list_head stList;
    uint32_t nodeNum;
    uint32_t pad;
} LUN_MAP_ROCACHE_HASH_TABLE_S;

#ifndef RETURN_OK
#define RETURN_OK 0
#endif

#ifndef RETURN_ERROR
#define RETURN_ERROR -1
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef true
#define true 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef false
#define false 0
#endif

#ifndef LIST_POISON1
#define LIST_POISON1 ((void *)0x00100100)
#endif

#ifndef LIST_POISON2
#define LIST_POISON2 ((void *)0x00200200)
#endif

#define PRINT_NUM_IN_LINE 3

#define MAX_KEY_NUM 300001ULL
#define MAX_THREAD_NUM 1
#define MAX_TABLE_NUM 5000
#define MAX_HASH_KEY_RANGE 5
#define MAX_ELEM_NUM_PER_NODE 15
#define MAX_NODE_NUM_PER_TABLE 15

#define LUN_MAP_ROCACHE_NODE_LEN (sizeof(LUN_MAP_ROCACHE_NODE_S) + MAX_ELEM_NUM_PER_NODE * sizeof(LUN_MAP_NODE_S))

#ifndef INVALID_VALUE64
#define INVALID_VALUE64 0xFFFFFFFFFFFFFFFFULL
#endif

#ifndef INVALID_VALUE32
#define INVALID_VALUE32 0xFFFFFFFF
#endif

LUN_MAP_ROCACHE_HASH_TABLE_S g_lunMapRocacheTable[MAX_THREAD_NUM][MAX_TABLE_NUM];

#define assert(condition) \
    if (!(condition)) {   \
        exit(-1);         \
    }

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long)(ptrdiff_t) & ((TYPE *)0)->MEMBER)
#endif

#define list_entry(ptr, type, member) ((type *)(void *)((char *)(void *)(ptr)-offsetof(type, member)))

#define INIT_LIST_HEAD(ptr)  \
    {                        \
        (ptr)->next = (ptr); \
        (ptr)->prev = (ptr); \
    }

static inline int list_empty(struct list_head *head) { return head->next == head; }

static inline void __list_add(struct list_head *new_head, struct list_head *prev, struct list_head *next)
{
    next->prev = new_head;
    new_head->next = next;
    new_head->prev = prev;
    prev->next = new_head;
}

static inline void list_add(struct list_head *new_head, struct list_head *head)
{
    __list_add(new_head, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

#define list_for_each_safe(pos, n, head) for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

typedef enum tagKVS_KEY_COMPARE_RESULT_E {
    KVS_KEY_COMPARE_EQUAL = 1,
    KVS_KEY_COMPARE_LARGE = 2,
    KVS_KEY_COMPARE_SMALL = 3,
    KVS_KEY_COMPARE_BUTT
} tagKVS_KEY_COMPARE_RESULT_E;

static inline uint64_t Rand64()
{
    uint64_t r = 0;
    for (int32_t i = 0; i < 5; i++) {
        r = (r << 15) | (rand() & 0x7FFF);
    }
    return r & 0xFFFFFFFFFFFFFFFFULL;
}

#endif  // INFRACTURE_H
