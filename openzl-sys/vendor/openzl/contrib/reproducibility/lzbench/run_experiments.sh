#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.

mkdir _bench_results

git clone "https://github.com/inikep/lzbench.git" --branch "v2.1"
pushd "lzbench" || exit 1
git checkout master && git fetch && git pull origin master
make \
DONT_BUILD_BRIEFLZ=1 \
DONT_BUILD_BROTLI=1 \
DONT_BUILD_BSC=1 \
DONT_BUILD_BZIP2=1 \
DONT_BUILD_BZIP3=1 \
DONT_BUILD_CRUSH=1 \
DONT_BUILD_CSC=1 \
DONT_BUILD_DENSITY=1 \
DONT_BUILD_FASTLZ=1 \
DONT_BUILD_FASTLZMA2=1 \
DONT_BUILD_GIPFELI=1 \
DONT_BUILD_GLZA=1 \
DONT_BUILD_KANZI=1 \
DONT_BUILD_LIBDEFLATE=1 \
DONT_BUILD_LIZARD=1 \
DONT_BUILD_LZ4=1 \
DONT_BUILD_LZF=1 \
DONT_BUILD_LZFSE=1 \
DONT_BUILD_LZG=1 \
DONT_BUILD_LZHAM=1 \
DONT_BUILD_LZJB=1 \
DONT_BUILD_LZLIB=1 \
DONT_BUILD_LZMA=1 \
DONT_BUILD_LZMAT=1 \
DONT_BUILD_LZO=1 \
DONT_BUILD_LZRW=1 \
DONT_BUILD_LZSSE=1 \
DONT_BUILD_NVCOMP=1 \
DONT_BUILD_PITHY=1 \
DONT_BUILD_PPMD=1 \
DONT_BUILD_QUICKLZ=1 \
DONT_BUILD_SLZ=1 \
DONT_BUILD_SNAPPY=1 \
DONT_BUILD_TAMP=1 \
DONT_BUILD_TORNADO=1 \
DONT_BUILD_UCL=1 \
DONT_BUILD_WFLZ=1 \
DONT_BUILD_YAPPY=1 \
DONT_BUILD_ZLIB_NG=1 \
DONT_BUILD_ZLING=1 \
DONT_BUILD_ZPAQ=1
# DONT_BUILD_XZ=1 \
# DONT_BUILD_ZLIB=1 \
# DONT_BUILD_ZSTD=1 \

#########################
# BENCHMARKS START HERE #
#########################
benchmarks=(
    "binance_canonical"
    "tlc_canonical"
    "rea6_precip"
    "era5_flux"
    "era5_precip"
    "era5_pressure"
    "era5_snow"
    "era5_wind"
    "psam_p"
    "psam_h"
    "ppmf_unit"
    "ppmf_person"
) # remove one or more of these to shorten the benchmark
for corpus in "${benchmarks[@]}"; do
    echo "Running benchmark for $corpus"
    ./lzbench -ezstd,1,3,19/xz,1,6,9/zlib,1,6 -o4 -r "../../../_corpus/$corpus" > "../_bench_results/$corpus.txt"
done

echo "Benchmarks recorded to experiments/_bench_results"
popd || exit 1
