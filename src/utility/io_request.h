#ifndef I0_REQUEST_H
#define I0_REQUEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t OpenFD(char *outputName);

void CloseFD(int32_t fd);

uint64_t SendWriteRequest(int fd, uint32_t offset, uint64_t size);

uint64_t SendReadRequest(int fd, uint32_t offset, uint64_t size);

#ifdef __cplusplus
}

#endif

#endif  // I0_REQUEST_H
