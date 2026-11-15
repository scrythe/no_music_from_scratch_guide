#!/bin/bash

# inspired by https://github.com/EpicGames/raddebugger/blob/master/build.sh

set -eu
cd "$(dirname "$0")"
command_args=""

# --- Unpack Arguments --------------------------------------------------------
while [[ $# -gt 0 ]]; do
  if [[ "$1" == "--" ]]; then
    shift
    command_args=$@
    break
  elif [[ $1 == "-level="* ]]; then
    level="${1#-level=}"
    shift
  else
    declare $1='1'
    shift
  fi
done

if [[ "${clean:-0}" == 1 ]]; then
  echo "[clean build folder]"
  rm -rf build
fi

mkdir -p build

asan_flags=""
if [[ "${asan:-0}" == 1 ]]; then
  echo "[enable asan]"
  asan_flags="-fsanitize=address -fno-omit-frame-pointer"
fi

common_build_flags="-g -std=gnu23 $asan_flags -Wall -Werror -o build/level_$level src/level_$level.c -lm"
if [[ "${release:-0}" == 1 ]]; then
  echo "[release mode]"
  compile="clang -O2 $common_build_flags"
else
  echo "[debug mode]"
  compile="clang -O0 $common_build_flags -Wno-unused-variable -Wno-unused-but-set-variable -DBUILD_DEBUG=1"
fi

echo "[building level_$level]"
$compile

if [[ "${run:-0}" == "1" ]]; then
  echo "[running level_$level]"
  ./build/level_$level $command_args
fi
