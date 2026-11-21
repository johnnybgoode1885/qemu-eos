#!/usr/bin/env bash
# Boot a flat 5D4 ROM dump with the local QEMU EOS build.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    echo "usage: run_5d4_133.sh DUMP_DIR|ROM1.BIN [LOG_FILE] [--gui]"
    echo "Boot 5D4 1.3.3 using disposable disk snapshots (30 seconds headless)."
    exit 0
fi
dump=${1:?usage: run_5d4_133.sh DUMP_DIR|ROM1.BIN [LOG_FILE] [--gui]}
log=${2:-/tmp/5d4-qemu.log}
display=none
if [[ ${2:-} == --gui ]]; then log=/tmp/5d4-qemu.log; display=gtk; fi
if [[ ${3:-} == --gui ]]; then display=gtk; fi
if [[ -f $dump ]]; then dump=$(dirname "$dump"); fi
if [[ ! -s $dump/ROM1.BIN || ! -s $dump/SFDATA.BIN ]]; then
    echo "Expected ROM1.BIN and SFDATA.BIN in $dump" >&2
    exit 2
fi
echo "Starting 5D4 QEMU with $dump (display: $display); log: $log"
stage=$(mktemp -d /tmp/qemu-5d4-133.XXXXXX)
trap 'rm -rf "$stage"' EXIT

test "$(stat -c %s "$dump/ROM1.BIN")" = 33554432
test "$(stat -c %s "$dump/SFDATA.BIN")" = 16777216
mkdir "$stage/5D4"
ln -s "$(realpath "$dump/ROM1.BIN")" "$stage/5D4/ROM1.BIN"
ln -s "$(realpath "$dump/SFDATA.BIN")" "$stage/5D4/SFDATA.BIN"

if [[ $display == gtk ]]; then
    runner=()
else
    runner=(timeout 30s)
fi

TMPDIR=/tmp QEMU_EOS_WORKDIR="$stage" "${runner[@]}" "${QEMU_EOS_BINARY:-$repo/build/arm-softmmu/qemu-system-arm}" \
    -M 5D4,firmware=boot=0 -display "$display" \
    -snapshot \
    -drive "if=sd,file=$repo/magiclantern/disk_images/sd.qcow2" \
    -drive "if=ide,file=$repo/magiclantern/disk_images/cf.qcow2" \
    -serial stdio -monitor none -d io 2>&1 | tee "$log" | \
    awk '/Available keys:|^- |^Key event:|no MPU button codes/ { print; fflush() }' || status=$?

if [[ ${status:-0} != 0 && ${status:-0} != 124 ]]; then
    tail -30 "$log"
    exit "$status"
fi
echo "QEMU log: $log"
rg -n 'loading.*ROM1|loading.*SFDATA|Init - spell|Abort init comp|ERROR' "$log" | tail -20 || true
