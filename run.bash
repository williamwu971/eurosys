#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then

  cd build/ || exit
  find . -type f -print0 | xargs --null grep -Z -L '.out' | xargs --null rm

  echo "" >>perf.csv
  echo "section,no_flush,size,pmem,n,Throughput,Elapsed,pmem_read_gb,pmem_read_bw,pmem_write_gb,pmem_write_bw,dram_read_gb,dram_read_bw,dram_write_gb,dram_write_bw" >>perf.csv

  no_flushes=("0" "1")
  no_flushes=("1")
  total_sizes=("8" "16" "32" "64" "128" "256" "512" "1024")
  total_sizes=("512" "1024" "2048" "4096")
  total_sizes=($(seq 512 512 3072))
  total_sizes=("4096")



  for nf in "${no_flushes[@]}"; do
    for t in "${total_sizes[@]}"; do

      rm -rf ./*.txt /pmem0/* /mnt/nvme0/*

      # taskset -c 12 ./ralloc.out 142800 320 || exit
      # taskset -c 12 ./kernel.out 142800 320 || exit
      #      LD_PRELOAD=/home/blepers/gperftools/.libs/libtcmalloc.so masstree_flush="$f" masstree_size="$t" masstree_pmem=0 taskset -c 0-20 ./kv.out 100000000 20 || exit
      #      PMEM_NO_FLUSH="$f" masstree_flush="$f" masstree_size="$t" masstree_pmem=0 taskset -c 0-20 ./kv.out 70000000 55 || exit
      PMEM_NO_FLUSH="$nf" masstree_no_flush="$nf" masstree_size="$t" masstree_pmem=0 numactl -i 4,5 taskset -c 0-27 ./kv.out 10000000 27 || exit
      #  taskset -c 0-20 ./kv_no_flush.out 100000000 19 || exit
      #  rm -rf /pmem0/* && taskset -c 12 ./fs_expand.out 2560 "$((4 * 1024 * 1024))" || exit
      #  rm -rf /pmem0/* && taskset -c 12 ./fs_open.out 2560 "$((4 * 1024 * 1024))" || exit
    done
  done

  cd ..
else
  echo "graph only"
fi

rm -rf ./*.pdf
#outname=ralloc.pdf dataname=build/ralloc_tscs.txt gnuplot <gnuplot.in &
#outname=kernel.pdf dataname=build/kernel_tscs.txt gnuplot <gnuplot.in &
#outname=kv.pdf dataname=build/kv_tscs.txt gnuplot <gnuplot.in &
#outname=fs_expand.pdf dataname=build/fs_expand_tscs.txt gnuplot <gnuplot.in &
#outname=fs_open.pdf dataname=build/fs_open_tscs.txt gnuplot <gnuplot.in &

while pgrep -i -f gnuplot >/dev/null; do
  sleep 1
done
