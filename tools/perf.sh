#!/usr/bin/bash
cd ../build/reldeb/src
sudo perf record --call-graph dwarf ./n64emu
sudo perf script > out.perf
sudo perf report --stdio > perfreport.txt
# need FlameGraph in $PATH
sudo env "PATH=$PATH:/opt/FlameGraph" stackcollapse-perf.pl out.perf > perf.folded
sudo env "PATH=$PATH:/opt/FlameGraph" flamegraph.pl perf.folded > perf.svg
    