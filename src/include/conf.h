#ifndef INCLUDE_CONF_H
#define INLCUDE_CONF_H

#define PAGE_SIZE 8192ULL        // 8K
#define BLOCK_SIZE (4ULL << 20)  // 4M
#define PAGE_NUM_PER_BLOCK 512ULL
#define PAGE_8K_SHIFT 13

#define IO_TIMES 100000

#endif  // INCLUDE_CONF_H
