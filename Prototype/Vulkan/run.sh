#!/usr/bin/env bash

set -eo pipefail

./build.sh $1 $2 $3

./build/bin.exe
