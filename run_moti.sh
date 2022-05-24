#!/bin/bash

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make moti
cd ..

cd bin
iostat -d /dev/sdd /dev/sdc 1 > ../log/moti_iostat.log &
pid=$!

sudo ./moti --t0_block_count=2500 --t1_block_count=25000 --enable_bypass=0 --enable_flush=1 --gc_water_level=10 --gc_block_ratio=1 --tierdown_water_level=60 --tierdown_block_ratio=1 --host_queue_size=64 --host_threads=64 --back_threads=32 --t0=/dev/sdd --t1=/dev/sdc --data_ratio=65 --workload_type=write_static_99 --workload_interval=1

sleep 2
kill $pid