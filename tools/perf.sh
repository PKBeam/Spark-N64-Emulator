#!/usr/bin/bash
cd ../build/profiling/src
sudo perf record --call-graph dwarf ./n64emu --num-rdp-syncs=1000
sudo perf script > out.perf
sudo perf report --stdio > perfreport.txt
# need FlameGraph in $PATH
sudo env "PATH=$PATH:/opt/FlameGraph" stackcollapse-perf.pl out.perf > perf.folded
sudo env "PATH=$PATH:/opt/FlameGraph" flamegraph.pl perf.folded > perf.svg
    
