#!/usr/bin/env bash

set -ex

module load sparc-dev/clang-9.0.1_openmpi-4.0.3

source_dir=${1}
build_dir=${2}

export CH=${source_dir}
export CH_BUILD=${build_dir}/parallel-for-transformer
rm -rf "$CH_BUILD"
mkdir -p "$CH_BUILD"
cd "$CH_BUILD"

target=install

cmake -G "${CMAKE_GENERATOR:-Ninja}" -DCMAKE_EXPORT_COMPILE_COMMANDS=1 "$CH"
time cmake --build . --target "${target}"
