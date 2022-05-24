#include "histogram.h"

#include <math.h>
#include <string.h>

#include "infracture.h"

const double kBucketLimit[kNumBuckets] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    12,
    14,
    16,
    18,
    20,
    25,
    30,
    35,
    40,
    45,
    50,
    60,
    70,
    80,
    90,
    100,
    120,
    140,
    160,
    180,
    200,
    250,
    300,
    350,
    400,
    450,
    500,
    600,
    700,
    800,
    900,
    1000,
    1200,
    1400,
    1600,
    1800,
    2000,
    2500,
    3000,
    3500,
    4000,
    4500,
    5000,
    6000,
    7000,
    8000,
    9000,
    10000,
    12000,
    14000,
    16000,
    18000,
    20000,
    25000,
    30000,
    35000,
    40000,
    45000,
    50000,
    60000,
    70000,
    80000,
    90000,
    100000,
    120000,
    140000,
    160000,
    180000,
    200000,
    250000,
    300000,
    350000,
    400000,
    450000,
    500000,
    600000,
    700000,
    800000,
    900000,
    1000000,
    1200000,
    1400000,
    1600000,
    1800000,
    2000000,
    2500000,
    3000000,
    3500000,
    4000000,
    4500000,
    5000000,
    6000000,
    7000000,
    8000000,
    9000000,
    10000000,
    12000000,
    14000000,
    16000000,
    18000000,
    20000000,
    25000000,
    30000000,
    35000000,
    40000000,
    45000000,
    50000000,
    60000000,
    70000000,
    80000000,
    90000000,
    100000000,
    120000000,
    140000000,
    160000000,
    180000000,
    200000000,
    250000000,
    300000000,
    350000000,
    400000000,
    450000000,
    500000000,
    600000000,
    700000000,
    800000000,
    900000000,
    1000000000,
    1200000000,
    1400000000,
    1600000000,
    1800000000,
    2000000000,
    2500000000.0,
    3000000000.0,
    3500000000.0,
    4000000000.0,
    4500000000.0,
    5000000000.0,
    6000000000.0,
    7000000000.0,
    8000000000.0,
    9000000000.0,
    1e200,
};

static double Percentile(Histogram *hist, double p)
{
    double threshold = hist->data.num * (p / 100.0);
    double sum = 0;

    for (int b = 0; b < kNumBuckets; b++) {
        sum += hist->data.buckets[b];
        if (sum >= threshold) {
            double left_point = (b == 0) ? 0 : kBucketLimit[b - 1];
            double right_point = kBucketLimit[b];

            double left_sum = sum - hist->data.buckets[b];
            double right_sum = sum;

            double pos = (threshold - left_sum) / (right_sum - left_sum);
            double r = left_point + (right_point - left_point) * pos;

            if (r < hist->data.min) r = hist->data.min;
            if (r > hist->data.max) r = hist->data.max;
            return r;
        }
    }
    return hist->data.max;
}

static double Median(Histogram *hist) { return Percentile(hist, 50.0); }

static double Average(Histogram *hist)
{
    if (hist->data.num == 0.0) return 0;
    return hist->data.sum / hist->data.num;
}

static double StandardDeviation(Histogram *hist)
{
    if (hist->data.num == 0.0) return 0;
    double variance =
        (hist->data.sum_squares * hist->data.num - hist->data.sum * hist->data.sum) / (hist->data.num * hist->data.num);
    return sqrt(variance);
}

void Add(void *self, double value)
{
    Histogram *hist = (Histogram *)self;
    int b = 0;
    while (b < kNumBuckets - 1 && kBucketLimit[b] <= value) b++;
    hist->data.buckets[b] += 1.0;
    if (hist->data.min > value) hist->data.min = value;
    if (hist->data.max < value) hist->data.max = value;
    hist->data.num++;
    hist->data.sum += value;
    hist->data.sum_squares += (value * value);
}

void Clear(void *self)
{
    Histogram *hist = (Histogram *)self;
    hist->data.min = kBucketLimit[kNumBuckets - 1];
    hist->data.max = 0;
    hist->data.num = 0;
    hist->data.sum = 0;
    hist->data.sum_squares = 0;
    for (int i = 0; i < kNumBuckets; i++) {
        hist->data.buckets[i] = 0;
    }
    memset(hist->data.textBuff, 0, strlen(hist->data.textBuff));
}

void Merge(void *self, void *__other)
{
    Histogram *hist = (Histogram *)self;
    Histogram *other = (Histogram *)__other;
    if (other->data.min < hist->data.min) hist->data.min = other->data.min;
    if (other->data.max > hist->data.max) hist->data.max = other->data.max;
    hist->data.num += other->data.num;
    hist->data.sum += other->data.sum;
    hist->data.sum_squares += other->data.sum_squares;
    for (int b = 0; b < kNumBuckets; b++) {
        hist->data.buckets[b] += other->data.buckets[b];
    }
}

char *GetHistogram(void *self)
{
    Histogram *hist = (Histogram *)self;
    size_t len = 0;

    snprintf(hist->data.textBuff, sizeof(hist->data.textBuff), "Count: %.0f Average: %.4f StdDev: %.2f\n",
             hist->data.num, Average(hist), StandardDeviation(hist));
    len = strlen(hist->data.textBuff);
    snprintf(hist->data.textBuff + len, sizeof(hist->data.textBuff) - len, "Min: %.4f Median: %.4f Max: %.4f\n",
             (hist->data.num == 0.0 ? 0.0 : hist->data.min), Median(hist), hist->data.max);

    const double mult = 100.0 / hist->data.num;
    double sum = 0;
    for (int32_t b = 0; b < kNumBuckets; b++) {
        if (hist->data.buckets[b] <= 0.0) continue;
        sum += hist->data.buckets[b];
        len = strlen(hist->data.textBuff);
        snprintf(hist->data.textBuff + len, sizeof(hist->data.textBuff) - len,
                 "[ %7.0f, %7.0f ) %7.0f %7.3f%% %7.3f%% \n", ((b == 0) ? 0.0 : kBucketLimit[b - 1]), kBucketLimit[b],
                 hist->data.buckets[b], mult * hist->data.buckets[b], mult * sum);
    }
    return hist->data.textBuff;
}

static HistogramIntf g_histIntf = {
    .clear = Clear,
    .add = Add,
    .merge = Merge,
    .getHistogram = GetHistogram,
};

int32_t CreateHistogram(Histogram **self)
{
    Histogram **ppHist = (Histogram **)self;
    *ppHist = (Histogram *)malloc(sizeof(Histogram));
    ASSERT((*ppHist) != NULL);
    memset((*ppHist), 0, sizeof(Histogram));
    (*ppHist)->intf = &g_histIntf;
    return RETURN_OK;
}

int32_t DestroyHistogram(Histogram **self)
{
    char *text = GetHistogram(*self);
    printf("%s", text);

    free(*self);
    *self = NULL;
    return RETURN_OK;
}
