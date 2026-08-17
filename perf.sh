#!/usr/bin/bash
cd build/src
sudo perf record --call-graph dwarf ./n64emu
sudo perf script > out.perf
sudo perf report --stdio > perfreport.txt
sudo ~/FlameGraph/stackcollapse-perf.pl out.perf > perf.folded
sudo ~/FlameGraph/flamegraph.pl perf.folded > perf.svg