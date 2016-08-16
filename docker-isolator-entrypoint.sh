#!/bin/bash
set -e

cd /isolator && \
    export LD_LIBRARY_PATH=LD_LIBRARY_PATH:/usr/local/lib && \
    ./bootstrap && \
    rm -Rf build && \
    mkdir build && \
    cd build && \
    ../configure --with-mesos-root=/mesos --with-mesos-build-dir=/mesos

"$@"