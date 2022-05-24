/*
 * MIT License
 *
 * Copyright (c) 2017 Lucas Lersch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Implementation derived from:
 * "Quickly Generating Billion-Record Synthetic Databases", Jim Gray et al,
 * SIGMOD 1994
 */

/*
 * The zipfian_int_distribution class is intended to be compatible with other
 * distributions introduced in #include <random> by the C++11 standard.
 *
 * Usage example:
 * #include <random>
 * #include "zipfian_int_distribution.h"
 * int main()
 * {
 *   std::default_random_engine generator;
 *   zipfian_int_distribution<int> distribution(1, 10, 0.99);
 *   int i = distribution(generator);
 * }
 */

/*
 * IMPORTANT: constructing the distribution object requires calculating the zeta
 * value which becomes prohibetively expensive for very large ranges. As an
 * alternative for such cases, the user can pass the pre-calculated values and
 * avoid the calculation every time.
 *
 * Usage example:
 * #include <random>
 * #include "zipfian_int_distribution.h"
 * int main()
 * {
 *   std::default_random_engine generator;
 *   zipfian_int_distribution<int>::param_type p(1, 1e6, 0.99, 27.000);
 *   zipfian_int_distribution<int> distribution(p);
 *   int i = distribution(generator);
 * }
 */

#ifndef ZIPFIAN_H
#define ZIPFIAN_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int Zipf(int min, int max, double theta);

typedef struct tagZipfGenData {
    void* generator_;
    void* distribution_;
} ZipfGenData;

typedef struct tagZipfGenIntf {
    uint64_t (*nextValue)(void* self);
} ZipfGenIntf;

typedef struct tagZipfGen {
    ZipfGenData data;
    ZipfGenIntf* intf;
} ZipfGen;

typedef struct tagZipfGenConfigure {
    uint64_t min;
    uint64_t max;
    double theta;
} ZipfGenConfigure;

void GenerateZipfGen(void** zipf, ZipfGenConfigure* conf);

void DestroyZipfGen(void** zipf);

#ifdef __cplusplus
}
#endif
#endif  // ZIPFIAN_H