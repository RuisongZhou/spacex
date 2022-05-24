#!/bin/bash

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make base_zipf 
cd ..

cd bin

x=(10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85)
for i in ${x[@]}
do
    iostat -d /dev/sdd /dev/sdc 1 > ../log/x_iostat_$i.log &
    pid=$!
	sudo ./base_zipf --t0_block_count=2500 --t1_block_count=25000 --enable_bypass=0 --enable_flush=1 --gc_water_level=10 --gc_block_ratio=1 --tierdown_water_level=$i --tierdown_block_ratio=1 --host_queue_size=64 --host_threads=64 --back_threads=32 --t0=/dev/sdd --t1=/dev/sdc --data_ratio=65 --workload_type=write_static_99 --workload_interval=1 > ../result/latency_$i.out

    sleep 2
    kill $pid
done