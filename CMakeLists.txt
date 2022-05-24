cmake_minimum_required(VERSION 3.9)
# Keep the version below in sync with the one in db.h
project(ss VERSION 0.1.0 LANGUAGES C CXX)

# C standard can be overridden when this is used as a sub-project.
if(NOT CMAKE_C_STANDARD)
  # This project can use C11, but will gracefully decay down to C89.
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED OFF)
  set(CMAKE_C_EXTENSIONS OFF)
endif(NOT CMAKE_C_STANDARD)

# C++ standard can be overridden when this is used as a sub-project.
if(NOT CMAKE_CXX_STANDARD)
  # This project requires C++11.
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

add_compile_options(-D_GNU_SOURCE -Wl,--no-as-needed)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -o0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -Wall -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -o0 -Wall -g -ggdb")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2 -Wall -DNDEBUG")

set(THREADS_PERFER_PTHREAD_FLAGS ON)
find_package(Threads REQUIRED)

include_directories(".")

file(GLOB BASE_FILES src/base/*c)
file(GLOB UTILITY_FILES src/utility/*c src/utility/*.cc)
file(GLOB WORKLOAD_FILES src/include/workload/*c)
file(GLOB PROC_FILES src/proc/*.c)
file(GLOB LOG_FILES src/third_party/c-logger/src/*c)

add_executable(base_zipf src/test/base_zipf_test.c ${BASE_FILES} ${WORKLOAD_FILES} ${UTILITY_FILES} ${PROC_FILES} ${LOG_FILES})
target_link_libraries(base_zipf PRIVATE Threads::Threads m)

add_executable(moti src/test/moti_test.c ${BASE_FILES} ${WORKLOAD_FILES} ${UTILITY_FILES} ${PROC_FILES} ${LOG_FILES})
target_link_libraries(moti PRIVATE Threads::Threads m)

add_executable(uniform src/test/uniform_test.c ${BASE_FILES} ${WORKLOAD_FILES} ${UTILITY_FILES} ${PROC_FILES} ${LOG_FILES})
target_link_libraries(uniform PRIVATE Threads::Threads m)

