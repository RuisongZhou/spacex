#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { kNumBuckets = 162, kTextBuffSize = 10240 };

typedef struct tagHistogramData {
    double min;
    double max;
    double num;
    double sum;
    double sum_squares;
    double buckets[kNumBuckets];
    char textBuff[kTextBuffSize];
} HistogramData;

typedef struct tagHistogramIntf {
    void (*clear)(void *self);
    void (*add)(void *self, double value);
    void (*merge)(void *self, void *__other);
    char *(*getHistogram)(void *self);
} HistogramIntf;

typedef struct tagHistogram {
    HistogramData data;
    HistogramIntf *intf;
} Histogram;

int32_t CreateHistogram(Histogram **self);

int32_t DestroyHistogram(Histogram **self);

#ifdef __cplusplus
};

#endif

#endif  // HISTOGRAM_H
