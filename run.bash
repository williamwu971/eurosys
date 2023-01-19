#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then

  cd build/ || exit
  rm -rf ./*.txt

  #  taskset -c 12 ./ralloc.out 142800 320 || exit
  #  taskset -c 12 ./kernel.out 142800 320 || exit
  #  taskset -c 12 ./kv.out 1000000 1 || exit
  rm -rf /pmem0/* && taskset -c 12 ./fs_expand.out 2560 "$((4 * 1024 * 1024))" || exit
#  rm -rf /pmem0/* && taskset -c 12 ./fs_open.out 2560 "$((4 * 1024 * 1024))" || exit

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
