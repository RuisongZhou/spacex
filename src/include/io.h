#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

#include <stdint.h>

typedef enum tagOpType {
    READ = 1,
    WRITE = 2,
} OpType;

typedef struct tagIO {
    uint64_t lba;
    uint32_t length;
    OpType op;
    uint64_t timestamp;
} IO;

#endif  // INCLUDE_IO_H
