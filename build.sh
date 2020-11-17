#!/usr/bin/env bash

set -ex

source_dir=${1}
build_dir=${2}

export CH=${source_dir}
export CH_BUILD=${build_dir}/parallel-for-transformer
rm -rf "$CH_BUILD"
mkdir -p "$CH_BUILD"
cd "$CH_BUILD"

target=install

cmake -G "${CMAKE_GENERATOR:-Ninja}" "$CH"
time cmake --build . --target "${target}"
