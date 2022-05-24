// #include "random.h"

// #include "infracture.h"

// static uint32_t NextValue(void* self)
// {
//     RandomGen* rnd = (RandomGen*)self;
//     static const uint32_t M = 2147483647L;  // 2^31-1
//     static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0
//     // We are computing
//     // seed_ = (seed_ * A) % M, where M = 2^31-1
//     // seed_ must not be zero or M, or else all subsequent computed values
//     // will be zero or M respectively. For all other values, seed_ will end
//     // up cycling through every number in [1,M-1]
//     uint64_t product = rnd->data.seed * A;
//     // Compute (product % M) using the fact that ((x << 31) % M) == x.
//     rnd->data.seed = ((product >> 31) + (product & M));
//     // The first reduction may overflow by 1 bit, so we may need to
//     // repeat. mod == M is not possible; using >
//     // allows the faster sign-bit-based test.
//     if (rnd->data.seed > M) {
//         rnd->data.seed -= M;
//     }
//     return rnd->data.seed;
// }

// static RandomGenIntf g_intf = {
//     .nextValue = NextValue,
// };

// void GenerateRandomGen(RandomGen** rnd, RandomGenConfigure* conf)
// {
//     RandomGen* rndGen = (RandomGen*)malloc(sizeof(RandomGen));
//     assert(rnd);
//     rndGen->data.seed = conf->seed & 0x7fffffffu;  // Avoid bad seeds.
//     if (rndGen->data.seed == 0 || rndGen->data.seed == 2147483647L) {
//         rndGen->data.seed = 1;
//     }
//     rndGen->intf = &g_intf;
//     *rnd = rndGen;
// }

// void DestroyRandomGen(RandomGen** rnd)
// {
//     free(*rnd);
//     *rnd = NULL;
// }

// // Returns a uniformly distributed value in the range [0..n-1]
// // REQUIRES: n > 0
// // uint32_t Uniform(int n) { return Next() % n; }
// // Randomly returns true ~"1/n" of the time, and false otherwise.
// // REQUIRES: n > 0
// // bool OneIn(int n) { return (Next() % n) == 0; }
// // Skewed: pick "base" uniformly from range [0,max_log] and then
// //
// // return "base" random bits. The effect is to pick a number in the
// //
// // range [0,2^max_log-1] with exponential bias towards smaller numbers.
// // uint32_t Skewed(int max_log) { return Uniform(1 << Uniform(max_log + 1)); }