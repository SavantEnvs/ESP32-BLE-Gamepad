#!/usr/bin/env bash
#
# mayhem/build.sh — build the ESP32-BLE-Gamepad fuzz harnesses and the host self-test.
#
# The library is an Arduino/NimBLE-Arduino sketch library for the ESP32; NimBLE needs the ESP32
# BLE controller and cannot be built for a Linux host. mayhem/host/ is a small host stand-in for
# the Arduino + NimBLE headers the library includes (GATT characteristics, callbacks, HID device),
# so the library's own .cpp files compile unchanged and are driven through their public API.
#
# Outputs (under /mayhem): fuzz_<name> (libFuzzer), fuzz_<name>-standalone (run-once reproducer),
# mayhem/build/selftest (known-answer tests, normal flags; run by mayhem/test.sh).
# Objects go to mayhem/build/ (gitignored). Re-runnable and offline: no network, no package manager.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
mkdir -p "$B/fuzz" "$B/test"

LIB_SRCS="BleConnectionStatus.cpp BleFeatureReport.cpp BleGamepadConfiguration.cpp BleGamepad.cpp
BleOutputReceiver.cpp BleSInput.cpp BleXInput.cpp mayhem/host/nimble_host_stub.cpp"
INCS="-I$SRC/mayhem/host/include -I$SRC -I$SRC/mayhem/harnesses"
CXXSTD="-std=gnu++17"

# Instrumented library objects only when the fuzz engine is libFuzzer-compatible.
COV=""
case "$LIB_FUZZING_ENGINE" in *fuzzer*) COV="-fsanitize=fuzzer-no-link" ;; esac

# 1) The library (+ host stand-in) with the sanitizers, SanCov and DWARF 3.
objs=()
for s in $LIB_SRCS; do
  o="$B/fuzz/$(basename "${s%.cpp}").o"
  $CXX $CXXSTD $SANITIZER_FLAGS $COV $DEBUG_FLAGS $INCS -c "$s" -o "$o"
  objs+=("$o")
done
$CXX $CXXSTD $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o"

# 2) Each harness twice: libFuzzer binary + standalone reproducer.
for h in output_feature sinput xinput descriptor report; do
  $CXX $CXXSTD $SANITIZER_FLAGS $COV $DEBUG_FLAGS $INCS -c "mayhem/harnesses/fuzz_$h.cpp" -o "$B/fuzz/fuzz_$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/fuzz/fuzz_$h.o" "${objs[@]}" "$B/fuzz/lsan_off.o" \
    -o "$SRC/fuzz_$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/fuzz/standalone_main.o" "$B/fuzz/fuzz_$h.o" "${objs[@]}" "$B/fuzz/lsan_off.o" \
    -o "$SRC/fuzz_$h-standalone"
done

# 3) The known-answer self-test, NORMAL flags (no sanitizers), for mayhem/test.sh.
tobjs=()
for s in $LIB_SRCS mayhem/selftest/selftest.cpp; do
  o="$B/test/$(basename "${s%.cpp}").o"
  $CXX $CXXSTD -O1 $COVERAGE_FLAGS $INCS -c "$s" -o "$o"
  tobjs+=("$o")
done
$CXX $COVERAGE_FLAGS "${tobjs[@]}" -o "$B/selftest"
echo "build.sh: done"
