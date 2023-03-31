#!/usr/bin/env bash

cd build || exit

rm -f update.run.txt

flushes=("clflush" "byte" "xsaveopt" "clflushopt" "clwb" "pmem_persist" "DRAM")


for func in "${flushes[@]}"; do

	echo "$func" >> update.run.txt

for i in {1..3}
do
	echo "$i"	
	masstree_size="4096" flush_func="$func" \
		numactl -i 4,5 taskset -c 0-27 ./kv.out 10000000 27
done
done

#PMEM_NO_FLUSH="0" masstree_size="4096" masstree_pmem=0 ./kv.out 50000000 54


cd .. || exit
