#include "io_request.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "src/include/conf.h"
#include "infracture.h"
#include "time.h"

int32_t OpenFD(char *outputName ){
    int fd = open(outputName, O_DIRECT | O_RDWR | O_CREAT, 0644);
    ASSERT(fd != 0);
    return fd;
}

void CloseFD(int32_t fd){
    if(fd!=0)
        close(fd);
}

uint64_t SendWriteRequest(int fd, uint32_t offset, uint64_t size){
    size = (size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
    void *data = NULL;

    int32_t ret = posix_memalign(&data, PAGE_SIZE, size);
    ASSERT(ret == 0 && data != NULL );

    uint64_t startTime = GetCurrentTimeStamp();

    ret = pwrite64(fd, data, size, offset);
    ASSERT(ret != -1);
    if (ret == -1)
        printf( " %d\n", errno);

    uint64_t endTime = GetCurrentTimeStamp();
    uint64_t diffTime = endTime - startTime;

    free( data);
    return diffTime;
}


uint64_t SendReadRequest(int32_t fd, uint32_t offset, uint64_t size ) {
    size = (size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
    void *data = NULL ;

    int32_t ret = posix_memalign(&data, PAGE_SIZE, size);
    ASSERT(ret == 0 && data != NULL);

    uint64_t startTime = GetCurrentTimeStamp();
    ret = pread64(fd, data, size, offset);

    ASSERT(ret != -1);

    if (ret == -1)
        printf( " %d\n", errno);

    uint64_t endTime = GetCurrentTimeStamp();
    uint64_t diffTime = endTime - startTime;

    free( data);
    return diffTime;
}


