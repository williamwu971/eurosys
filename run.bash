

rm -rf /pmem0/*
cd build/ || exit
rm -rf ./*.txt

taskset -c 12 ./simple.out 142800 320 || exit
taskset -c 12 ./fs.out 142800 320 || exit
taskset -c 12 ./kv.out 1000000 1 || exit

cd ..
rm -rf ./*.pdf
outname=simple.pdf dataname=build/simple_tscs.txt gnuplot < gnuplot.in &
outname=fs.pdf     dataname=build/fs_tscs.txt     gnuplot < gnuplot.in &
outname=kv.pdf     dataname=build/kv_tscs.txt     gnuplot < gnuplot.in &

while pgrep -i -f gnuplot >/dev/null; do
  sleep 1
done

